/* pc-panel-tray: the tray icon of PC Panel.
 *
 * Left click opens the dashboard. The menu has only three entries:
 *   Change port...      asks for a new port and restarts the dashboard on it
 *   Start with system   starts PC Panel (dashboard and icon) when you log in
 *   Quit                stops the dashboard service and closes the icon
 *
 * It is a StatusNotifierItem with a dbusmenu, written on sd-bus (libsystemd), so it
 * needs no GUI toolkit. It has no timers and no polling: while nobody clicks it the
 * process sleeps in the kernel and uses no CPU. The only other thing that wakes it
 * is a change to its own files on disk (an update), seen through inotify.
 *
 * Alerts: the dashboard writes the worst level of its current problems to
 * $XDG_RUNTIME_DIR/pc-panel/alert (also watched with inotify). The icon then turns
 * yellow (warning), orange (serious) or red (critical), lists the problems in its
 * tooltip, and from serious up asks the tray to keep it visible (NeedsAttention).
 * Nothing blinks.
 *
 * The dashboard runs as the systemd user service pc-panel.service, or as a child
 * process of this icon when that service does not exist (AppImage, source folder).
 *
 * MIT license.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <systemd/sd-bus.h>
#include <systemd/sd-event.h>

#ifndef VERSION
#define VERSION "dev"
#endif
#define UNIT "pc-panel.service"
#define APP_ID "io.github.gabrielmf1998.PcPanel"
#define DEFAULT_PORT 47822
#define UPDATED 75              /* exit code of the dashboard after it replaced the AppImage */
#define SNI_PATH "/StatusNotifierItem"
#define MENU_PATH "/MenuBar"
#define SNI_IFACE "org.kde.StatusNotifierItem"
#define MENU_IFACE "com.canonical.dbusmenu"

enum { M_ROOT, M_PORT, M_AUTOSTART, M_SEP, M_QUIT, M_COUNT };

static sd_bus *bus;
static sd_event *ev;
static char self_path[PATH_MAX], self_dir[PATH_MAX], server_path[PATH_MAX], icons_dir[PATH_MAX];
static char cfg_dir[PATH_MAX], port_file[PATH_MAX], autostart_file[PATH_MAX];
static const char *appimage;
static int port, use_systemd, quitting, relaunch;
static pid_t child = -1, dialog_pid = -1;
static int quick_fails;
static struct timespec child_started;
static uint32_t revision = 1;
static struct stat st_self, st_server;
static sd_event_source *settle_timer;
static char dialog_buf[64];
static char alert_dir[PATH_MAX], alert_file[PATH_MAX], alert_text[1024];
static int alert_level;
static const char *const LEVEL_ICON[] = {"pc-panel", "pc-panel-warning", "pc-panel-serious", "pc-panel-critical"};
static size_t dialog_len;

/* ------------------------------------------------------------------ small helpers */

static void reset_child(void)
{
    sigset_t none;
    sigemptyset(&none);
    sigprocmask(SIG_SETMASK, &none, NULL);
}

/* runs argv and waits; with out != NULL stdout is captured; returns the exit code or -1 */
static int run(char *const argv[], char *out, size_t outlen)
{
    int fd[2] = {-1, -1}, st = 0;
    if (out && pipe2(fd, O_CLOEXEC) < 0)
        return -1;
    pid_t p = fork();
    if (p < 0) {
        if (out) { close(fd[0]); close(fd[1]); }
        return -1;
    }
    if (p == 0) {
        reset_child();
        int n = open("/dev/null", O_RDWR);
        dup2(n, 0);
        dup2(out ? fd[1] : n, 1);
        dup2(n, 2);
        execvp(argv[0], argv);
        _exit(127);
    }
    if (out) {
        size_t len = 0;
        ssize_t r;
        char junk[256];
        close(fd[1]);
        while (len + 1 < outlen && (r = read(fd[0], out + len, outlen - 1 - len)) > 0)
            len += (size_t)r;
        while (read(fd[0], junk, sizeof junk) > 0)
            ;
        close(fd[0]);
        out[len] = 0;
        while (len && (out[len - 1] == '\n' || out[len - 1] == ' '))
            out[--len] = 0;
    }
    while (waitpid(p, &st, 0) < 0 && errno == EINTR)
        ;
    return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}

/* starts argv in its own session, not as our child, and does not wait */
static void spawn_detached(char *const argv[])
{
    pid_t p = fork();
    if (p == 0) {
        reset_child();
        setsid();
        if (fork() == 0) {
            int n = open("/dev/null", O_RDWR);
            dup2(n, 0); dup2(n, 1); dup2(n, 2);
            execvp(argv[0], argv);
            _exit(127);
        }
        _exit(0);
    }
    if (p > 0)
        while (waitpid(p, NULL, 0) < 0 && errno == EINTR)
            ;
}

static int systemctl(const char *a, const char *b, const char *c)
{
    char *argv[] = {"systemctl", "--user", (char *)a, (char *)b, (char *)c, NULL};
    return run(argv, NULL, 0);
}

static int in_path(const char *name)
{
    const char *path = getenv("PATH");
    char buf[PATH_MAX];
    if (!path)
        path = "/usr/local/bin:/usr/bin:/bin";
    while (*path) {
        size_t n = strcspn(path, ":");
        if (n && n + strlen(name) + 2 < sizeof buf) {
            snprintf(buf, sizeof buf, "%.*s/%s", (int)n, path, name);
            if (access(buf, X_OK) == 0)
                return 1;
        }
        path += n + (path[n] == ':');
    }
    return 0;
}

static int parse_port(const char *s)
{
    char *end;
    while (*s == ' ' || *s == '\t')
        s++;
    if (!*s)
        return 0;
    errno = 0;
    long p = strtol(s, &end, 10);
    while (*end == ' ' || *end == '\n' || *end == '\r' || *end == '\t')
        end++;
    return (!errno && *end == 0 && p >= 1024 && p <= 65535) ? (int)p : 0;
}

static int read_port(void)
{
    char buf[32] = "";
    const char *env = getenv("PC_PANEL_PORT");
    int p = 0;
    if (env && *env) {
        p = parse_port(env);
    } else {
        FILE *f = fopen(port_file, "r");
        if (f) {
            if (fgets(buf, sizeof buf, f))
                p = parse_port(buf);
            fclose(f);
        }
    }
    return p ? p : DEFAULT_PORT;
}

static void notify(const char *fmt, ...)
{
    char body[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(body, sizeof body, fmt, ap);
    va_end(ap);
    sd_bus_call_method_async(bus, NULL, "org.freedesktop.Notifications", "/org/freedesktop/Notifications",
                             "org.freedesktop.Notifications", "Notify", NULL, NULL, "susssasa{sv}i",
                             "PC Panel", (uint32_t)0, "pc-panel", "PC Panel", body, 0, 0, (int32_t)6000);
}

static void url(char *buf, size_t n)
{
    snprintf(buf, n, "http://127.0.0.1:%d", port);
}

/* ------------------------------------------------------------------ the dashboard */

static int restart_later(sd_event_source *s, uint64_t usec, void *u);

static int child_exited(sd_event_source *s, const siginfo_t *si, void *u)
{
    (void)s; (void)u;
    if (si->si_pid != child)
        return 0;
    child = -1;
    if (quitting)
        return 0;
    if (si->si_code == CLD_EXITED && si->si_status == UPDATED && appimage) {
        relaunch = 1;           /* the dashboard swapped the AppImage file */
        sd_event_exit(ev, 0);
    } else if (!(si->si_code == CLD_EXITED && si->si_status == 0) &&
               !(si->si_code == CLD_KILLED && si->si_status == SIGTERM)) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        quick_fails = now.tv_sec - child_started.tv_sec < 20 ? quick_fails + 1 : 0;
        if (quick_fails >= 3)       /* never loop: tell the user once and wait for a click */
            notify("The dashboard keeps failing to start. Is port %d used by another program?", port);
        else
            sd_event_add_time_relative(ev, NULL, CLOCK_MONOTONIC, 5000000, 0, restart_later, NULL);
    }
    return 0;
}

static void start_dashboard(void)
{
    if (use_systemd) {
        systemctl("reset-failed", UNIT, NULL);     /* a click always gets a fresh start limit */
        systemctl("start", "--no-block", UNIT);
        return;
    }
    if (child > 0)
        return;
    pid_t parent = getpid(), p = fork();
    if (p == 0) {
        reset_child();
        prctl(PR_SET_PDEATHSIG, SIGTERM);
        if (getppid() != parent)
            _exit(0);
        execlp("python3", "python3", "-I", server_path, (char *)NULL);
        _exit(127);
    }
    if (p > 0) {
        child = p;
        clock_gettime(CLOCK_MONOTONIC, &child_started);
        sd_event_add_child(ev, NULL, p, WEXITED, child_exited, NULL);
    }
}

static int restart_later(sd_event_source *s, uint64_t usec, void *u)
{
    (void)s; (void)usec; (void)u;
    if (!quitting)
        start_dashboard();
    return 0;
}

static void stop_child(void)
{
    if (child <= 0)
        return;
    pid_t p = child;
    child = -1;
    kill(p, SIGTERM);
    for (int i = 0; i < 100; i++) {
        if (waitpid(p, NULL, WNOHANG) == p)
            return;
        nanosleep(&(struct timespec){0, 100000000}, NULL);
    }
    kill(p, SIGKILL);
    waitpid(p, NULL, 0);
}

static void stop_dashboard(void)
{
    if (use_systemd)
        systemctl("stop", UNIT, NULL);
    else
        stop_child();
}

static void restart_dashboard(void)
{
    if (use_systemd) {
        systemctl("reset-failed", UNIT, NULL);
        systemctl("restart", "--no-block", UNIT);
    } else {
        stop_child();
        quick_fails = 0;
        start_dashboard();
    }
}

static void open_dashboard(void)
{
    char u[64];
    start_dashboard();
    url(u, sizeof u);
    char *argv[] = {"xdg-open", u, NULL};
    spawn_detached(argv);
}

/* ------------------------------------------------------------------ start with system */

static int autostart_on(void)
{
    char buf[4096];
    FILE *f = fopen(autostart_file, "r");
    if (!f)
        return 0;
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    buf[n] = 0;
    return !strstr(buf, "Hidden=true") && !strstr(buf, "X-GNOME-Autostart-enabled=false");
}

static void emit_menu(void)
{
    revision++;
    sd_bus_emit_signal(bus, MENU_PATH, MENU_IFACE, "LayoutUpdated", "ui", revision, (int32_t)0);
}

static void set_autostart(int on)
{
    if (on) {
        char tmp[PATH_MAX + 8], dir[PATH_MAX], exe[2 * PATH_MAX];
        const char *src = appimage ? appimage : self_path;
        size_t j = 0;
        for (const char *c = src; *c && j + 3 < sizeof exe; c++) {
            if (*c == '"' || *c == '\\' || *c == '`' || *c == '$')
                exe[j++] = '\\';
            exe[j++] = *c;
        }
        exe[j] = 0;
        snprintf(dir, sizeof dir, "%s", autostart_file);
        *strrchr(dir, '/') = 0;
        mkdir(dir, 0755);
        snprintf(tmp, sizeof tmp, "%s.new", autostart_file);
        FILE *f = fopen(tmp, "w");
        if (!f || fprintf(f, "[Desktop Entry]\nType=Application\nName=PC Panel\n"
                             "Comment=Tray icon of the PC Panel dashboard\nExec=\"%s\"\nIcon=pc-panel\n"
                             "Terminal=false\nNoDisplay=true\nX-GNOME-Autostart-enabled=true\n", exe) < 0 ||
            fclose(f) != 0 || rename(tmp, autostart_file) != 0) {
            notify("Could not write %s: %s", autostart_file, strerror(errno));
            unlink(tmp);
        }
    } else if (unlink(autostart_file) != 0 && errno != ENOENT) {
        notify("Could not remove %s: %s", autostart_file, strerror(errno));
    }
    if (use_systemd)
        systemctl(on ? "enable" : "disable", UNIT, NULL);
    emit_menu();
}

/* ------------------------------------------------------------------ port */

static void port_chosen(const char *answer)
{
    int p = parse_port(answer);
    if (!p) {
        notify("The port must be a number from 1024 to 65535.");
        return;
    }
    if (p == port)
        return;
    int s = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    struct sockaddr_in a = {.sin_family = AF_INET, .sin_port = htons((uint16_t)p),
                            .sin_addr.s_addr = htonl(INADDR_LOOPBACK)};
    int busy = s < 0 || bind(s, (struct sockaddr *)&a, sizeof a) != 0;
    if (s >= 0)
        close(s);
    if (busy) {
        notify("Port %d is already in use by another program.", p);
        return;
    }
    char tmp[PATH_MAX + 8];
    snprintf(tmp, sizeof tmp, "%s.new", port_file);
    mkdir(cfg_dir, 0755);
    FILE *f = fopen(tmp, "w");
    if (!f || fprintf(f, "%d\n", p) < 0 || fclose(f) != 0 || rename(tmp, port_file) != 0) {
        notify("Could not save the port: %s", strerror(errno));
        return;
    }
    port = read_port();
    if (port != p) {
        notify("Saved, but the PC_PANEL_PORT environment variable still sets port %d.", port);
        return;
    }
    restart_dashboard();
    emit_menu();
    sd_bus_emit_signal(bus, SNI_PATH, SNI_IFACE, "NewToolTip", "");
    notify("The dashboard now runs at http://127.0.0.1:%d", port);
}

static int dialog_io(sd_event_source *s, int fd, uint32_t revents, void *u)
{
    (void)revents; (void)u;
    ssize_t r = read(fd, dialog_buf + dialog_len, sizeof dialog_buf - 1 - dialog_len);
    if (r > 0) {
        dialog_len += (size_t)r;
        if (dialog_len < sizeof dialog_buf - 1)
            return 0;
    } else if (r < 0 && (errno == EAGAIN || errno == EINTR)) {
        return 0;
    }
    dialog_buf[dialog_len] = 0;
    sd_event_source_disable_unref(s);
    close(fd);
    int st = 0;
    while (waitpid(dialog_pid, &st, 0) < 0 && errno == EINTR)
        ;
    dialog_pid = -1;
    if (WIFEXITED(st) && WEXITSTATUS(st) == 0 && dialog_len)
        port_chosen(dialog_buf);
    return 0;
}

static void ask_port(void)
{
    char cur[16], *argv[10];
    int fd[2];
    if (dialog_pid > 0)
        return;
    snprintf(cur, sizeof cur, "%d", port);
    char *text = "Port for the dashboard (1024 to 65535):";
    if (in_path("kdialog")) {
        char *a[] = {"kdialog", "--title", "PC Panel", "--inputbox", text, cur, NULL};
        memcpy(argv, a, sizeof a);
    } else if (in_path("zenity")) {
        char *a[] = {"zenity", "--entry", "--title", "PC Panel", "--text", text, "--entry-text", cur, NULL};
        memcpy(argv, a, sizeof a);
    } else {
        notify("Install zenity or kdialog to change the port here, or write the port number into %s", port_file);
        return;
    }
    if (pipe2(fd, O_CLOEXEC | O_NONBLOCK) < 0)
        return;
    pid_t p = fork();
    if (p == 0) {
        reset_child();
        int n = open("/dev/null", O_RDWR);
        int fl = fcntl(fd[1], F_GETFL);
        fcntl(fd[1], F_SETFL, fl & ~O_NONBLOCK);
        dup2(n, 0); dup2(fd[1], 1); dup2(n, 2);
        execvp(argv[0], argv);
        _exit(127);
    }
    close(fd[1]);
    if (p < 0) {
        close(fd[0]);
        return;
    }
    dialog_pid = p;
    dialog_len = 0;
    if (sd_event_add_io(ev, NULL, fd[0], EPOLLIN, dialog_io, NULL) < 0) {
        close(fd[0]);
        kill(p, SIGTERM);
        waitpid(p, NULL, 0);
        dialog_pid = -1;
    }
}

/* ------------------------------------------------------------------ menu actions */

static void quit(void)
{
    quitting = 1;
    stop_dashboard();
    sd_event_exit(ev, 0);
}

static int clicked(sd_event_source *s, void *u)
{
    int id = (int)(intptr_t)u;
    (void)s;
    if (id == M_PORT)
        ask_port();
    else if (id == M_AUTOSTART)
        set_autostart(!autostart_on());
    else if (id == M_QUIT)
        quit();
    return 0;
}

static void schedule_click(int id, const char *event)
{
    if (!strcmp(event, "clicked"))
        sd_event_add_defer(ev, NULL, clicked, (void *)(intptr_t)id);
}

/* ------------------------------------------------------------------ StatusNotifierItem */

static int sni_prop(sd_bus *b, const char *path, const char *iface, const char *prop, sd_bus_message *reply,
                    void *u, sd_bus_error *err)
{
    (void)b; (void)path; (void)iface; (void)u; (void)err;
    char tip[64];
    if (!strcmp(prop, "Category")) return sd_bus_message_append(reply, "s", "SystemServices");
    if (!strcmp(prop, "Id")) return sd_bus_message_append(reply, "s", "pc-panel");
    if (!strcmp(prop, "Title")) return sd_bus_message_append(reply, "s", "PC Panel");
    if (!strcmp(prop, "Status")) return sd_bus_message_append(reply, "s", alert_level >= 2 ? "NeedsAttention" : "Active");
    if (!strcmp(prop, "IconName") || !strcmp(prop, "AttentionIconName"))
        return sd_bus_message_append(reply, "s", LEVEL_ICON[alert_level]);
    if (!strcmp(prop, "IconThemePath")) return sd_bus_message_append(reply, "s", icons_dir);
    if (!strcmp(prop, "OverlayIconName"))
        return sd_bus_message_append(reply, "s", "");
    if (!strcmp(prop, "IconPixmap") || !strcmp(prop, "OverlayIconPixmap") || !strcmp(prop, "AttentionIconPixmap"))
        return sd_bus_message_append(reply, "a(iiay)", 0);
    if (!strcmp(prop, "ItemIsMenu")) return sd_bus_message_append(reply, "b", 0);
    if (!strcmp(prop, "Menu")) return sd_bus_message_append(reply, "o", MENU_PATH);
    if (!strcmp(prop, "WindowId")) return sd_bus_message_append(reply, "i", 0);
    if (!strcmp(prop, "ToolTip")) {
        char body[sizeof alert_text + 80];
        url(tip, sizeof tip);
        if (alert_level)
            snprintf(body, sizeof body, "%s\n%s", alert_text, tip);
        else
            snprintf(body, sizeof body, "%s", tip);
        return sd_bus_message_append(reply, "(sa(iiay)ss)", LEVEL_ICON[alert_level], 0, "PC Panel", body);
    }
    return -ENOENT;
}

static int sni_activate(sd_bus_message *m, void *u, sd_bus_error *err)
{
    (void)u; (void)err;
    open_dashboard();
    return sd_bus_reply_method_return(m, "");
}

static int sni_ignore(sd_bus_message *m, void *u, sd_bus_error *err)
{
    (void)u; (void)err;
    return sd_bus_reply_method_return(m, "");
}

static const sd_bus_vtable sni_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_PROPERTY("Category", "s", sni_prop, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("Id", "s", sni_prop, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("Title", "s", sni_prop, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("Status", "s", sni_prop, 0, SD_BUS_VTABLE_PROPERTY_EMITS_INVALIDATION),
    SD_BUS_PROPERTY("WindowId", "i", sni_prop, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("IconName", "s", sni_prop, 0, SD_BUS_VTABLE_PROPERTY_EMITS_INVALIDATION),
    SD_BUS_PROPERTY("IconThemePath", "s", sni_prop, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("IconPixmap", "a(iiay)", sni_prop, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("OverlayIconName", "s", sni_prop, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("OverlayIconPixmap", "a(iiay)", sni_prop, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("AttentionIconName", "s", sni_prop, 0, SD_BUS_VTABLE_PROPERTY_EMITS_INVALIDATION),
    SD_BUS_PROPERTY("AttentionIconPixmap", "a(iiay)", sni_prop, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("ToolTip", "(sa(iiay)ss)", sni_prop, 0, SD_BUS_VTABLE_PROPERTY_EMITS_INVALIDATION),
    SD_BUS_PROPERTY("ItemIsMenu", "b", sni_prop, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("Menu", "o", sni_prop, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_METHOD("Activate", "ii", "", sni_activate, 0),
    SD_BUS_METHOD("SecondaryActivate", "ii", "", sni_ignore, 0),
    SD_BUS_METHOD("ContextMenu", "ii", "", sni_ignore, 0),
    SD_BUS_METHOD("Scroll", "is", "", sni_ignore, 0),
    SD_BUS_SIGNAL("NewTitle", "", 0),
    SD_BUS_SIGNAL("NewIcon", "", 0),
    SD_BUS_SIGNAL("NewAttentionIcon", "", 0),
    SD_BUS_SIGNAL("NewToolTip", "", 0),
    SD_BUS_SIGNAL("NewStatus", "s", 0),
    SD_BUS_VTABLE_END
};

/* ------------------------------------------------------------------ dbusmenu */

static int put_s(sd_bus_message *m, const char *k, const char *v)
{
    return sd_bus_message_append(m, "{sv}", k, "s", v);
}

static int append_props(sd_bus_message *m, int id)
{
    char label[48];
    int r = sd_bus_message_open_container(m, 'a', "{sv}");
    if (r < 0)
        return r;
    switch (id) {
    case M_ROOT:
        put_s(m, "children-display", "submenu");
        break;
    case M_PORT:
        snprintf(label, sizeof label, "Change port (%d)…", port);
        put_s(m, "label", label);
        break;
    case M_AUTOSTART:
        put_s(m, "label", "Start with system");
        put_s(m, "toggle-type", "checkmark");
        sd_bus_message_append(m, "{sv}", "toggle-state", "i", (int32_t)autostart_on());
        break;
    case M_SEP:
        put_s(m, "type", "separator");
        break;
    case M_QUIT:
        put_s(m, "label", "Quit");
        put_s(m, "icon-name", "application-exit");
        break;
    }
    return sd_bus_message_close_container(m);
}

static int append_node(sd_bus_message *m, int id, int with_children)
{
    sd_bus_message_open_container(m, 'r', "ia{sv}av");
    sd_bus_message_append(m, "i", (int32_t)id);
    append_props(m, id);
    sd_bus_message_open_container(m, 'a', "v");
    if (with_children)
        for (int c = M_PORT; c < M_COUNT; c++) {
            sd_bus_message_open_container(m, 'v', "(ia{sv}av)");
            append_node(m, c, 0);
            sd_bus_message_close_container(m);
        }
    sd_bus_message_close_container(m);
    return sd_bus_message_close_container(m);
}

static int send_reply(sd_bus_message *reply, int r)
{
    if (r >= 0)
        r = sd_bus_message_send(reply);
    sd_bus_message_unref(reply);
    return r;
}

static int menu_get_layout(sd_bus_message *m, void *u, sd_bus_error *err)
{
    (void)u; (void)err;
    int32_t parent = 0, depth = -1;
    sd_bus_message *reply = NULL;
    int r = sd_bus_message_read(m, "ii", &parent, &depth);
    if (r < 0)
        return r;
    if (parent < 0 || parent >= M_COUNT)
        return sd_bus_reply_method_errorf(m, "com.canonical.dbusmenu.Error", "no item %d", parent);
    r = sd_bus_message_new_method_return(m, &reply);
    if (r < 0)
        return r;
    r = sd_bus_message_append(reply, "u", revision);
    if (r >= 0)
        r = append_node(reply, parent, parent == M_ROOT && depth != 0);
    return send_reply(reply, r);
}

static int menu_group_props(sd_bus_message *m, void *u, sd_bus_error *err)
{
    (void)u; (void)err;
    const int32_t *ids = NULL;
    size_t size = 0;
    sd_bus_message *reply = NULL;
    int r = sd_bus_message_read_array(m, 'i', (const void **)&ids, &size);
    if (r < 0)
        return r;
    size /= sizeof(int32_t);
    r = sd_bus_message_new_method_return(m, &reply);
    if (r < 0)
        return r;
    r = sd_bus_message_open_container(reply, 'a', "(ia{sv})");
    for (int id = M_ROOT; id < M_COUNT && r >= 0; id++) {
        int want = size == 0;
        for (size_t i = 0; i < size; i++)
            want |= ids[i] == id;
        if (!want)
            continue;
        sd_bus_message_open_container(reply, 'r', "ia{sv}");
        sd_bus_message_append(reply, "i", (int32_t)id);
        append_props(reply, id);
        r = sd_bus_message_close_container(reply);
    }
    if (r >= 0)
        r = sd_bus_message_close_container(reply);
    return send_reply(reply, r);
}

static int menu_get_property(sd_bus_message *m, void *u, sd_bus_error *err)
{
    (void)u; (void)err;
    int32_t id;
    const char *name;
    char label[48];
    int r = sd_bus_message_read(m, "is", &id, &name);
    if (r < 0)
        return r;
    if (!strcmp(name, "label") && id == M_PORT) {
        snprintf(label, sizeof label, "Change port (%d)…", port);
        return sd_bus_reply_method_return(m, "v", "s", label);
    }
    if (!strcmp(name, "label") && id == M_AUTOSTART)
        return sd_bus_reply_method_return(m, "v", "s", "Start with system");
    if (!strcmp(name, "label") && id == M_QUIT)
        return sd_bus_reply_method_return(m, "v", "s", "Quit");
    if (!strcmp(name, "toggle-state") && id == M_AUTOSTART)
        return sd_bus_reply_method_return(m, "v", "i", (int32_t)autostart_on());
    return sd_bus_reply_method_errorf(m, "com.canonical.dbusmenu.Error", "no property %s", name);
}

static int menu_event(sd_bus_message *m, void *u, sd_bus_error *err)
{
    (void)u; (void)err;
    int32_t id;
    const char *event;
    int r = sd_bus_message_read(m, "is", &id, &event);
    if (r < 0)
        return r;
    schedule_click(id, event);
    return sd_bus_reply_method_return(m, "");
}

static int menu_event_group(sd_bus_message *m, void *u, sd_bus_error *err)
{
    (void)u; (void)err;
    int r = sd_bus_message_enter_container(m, 'a', "(isvu)");
    if (r < 0)
        return r;
    while ((r = sd_bus_message_enter_container(m, 'r', "isvu")) > 0) {
        int32_t id;
        const char *event;
        if (sd_bus_message_read(m, "is", &id, &event) >= 0)
            schedule_click(id, event);
        sd_bus_message_skip(m, "vu");
        sd_bus_message_exit_container(m);
    }
    sd_bus_message_exit_container(m);
    return sd_bus_reply_method_return(m, "ai", 0);
}

static int menu_about_to_show(sd_bus_message *m, void *u, sd_bus_error *err)
{
    (void)u; (void)err;
    return sd_bus_reply_method_return(m, "b", 0);
}

static int menu_about_to_show_group(sd_bus_message *m, void *u, sd_bus_error *err)
{
    (void)u; (void)err;
    return sd_bus_reply_method_return(m, "aiai", 0, 0);
}

static int menu_prop(sd_bus *b, const char *path, const char *iface, const char *prop, sd_bus_message *reply,
                     void *u, sd_bus_error *err)
{
    (void)b; (void)path; (void)iface; (void)u; (void)err;
    if (!strcmp(prop, "Version")) return sd_bus_message_append(reply, "u", (uint32_t)3);
    if (!strcmp(prop, "TextDirection")) return sd_bus_message_append(reply, "s", "ltr");
    if (!strcmp(prop, "Status")) return sd_bus_message_append(reply, "s", "normal");
    if (!strcmp(prop, "IconThemePath")) return sd_bus_message_append(reply, "as", 0);
    return -ENOENT;
}

static const sd_bus_vtable menu_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_PROPERTY("Version", "u", menu_prop, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("TextDirection", "s", menu_prop, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("Status", "s", menu_prop, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("IconThemePath", "as", menu_prop, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_METHOD("GetLayout", "iias", "u(ia{sv}av)", menu_get_layout, 0),
    SD_BUS_METHOD("GetGroupProperties", "aias", "a(ia{sv})", menu_group_props, 0),
    SD_BUS_METHOD("GetProperty", "is", "v", menu_get_property, 0),
    SD_BUS_METHOD("Event", "isvu", "", menu_event, 0),
    SD_BUS_METHOD("EventGroup", "a(isvu)", "ai", menu_event_group, 0),
    SD_BUS_METHOD("AboutToShow", "i", "b", menu_about_to_show, 0),
    SD_BUS_METHOD("AboutToShowGroup", "ai", "aiai", menu_about_to_show_group, 0),
    SD_BUS_SIGNAL("LayoutUpdated", "ui", 0),
    SD_BUS_SIGNAL("ItemsPropertiesUpdated", "a(ia{sv})a(ias)", 0),
    SD_BUS_SIGNAL("ItemActivationRequested", "iu", 0),
    SD_BUS_VTABLE_END
};

/* ------------------------------------------------------------------ tray host */

static void register_item(void)
{
    const char *me = NULL;
    sd_bus_get_unique_name(bus, &me);
    sd_bus_call_method_async(bus, NULL, "org.kde.StatusNotifierWatcher", "/StatusNotifierWatcher",
                             "org.kde.StatusNotifierWatcher", "RegisterStatusNotifierItem", NULL, NULL, "s", me);
}

static int watcher_changed(sd_bus_message *m, void *u, sd_bus_error *err)
{
    (void)u; (void)err;
    const char *name, *old, *now;
    if (sd_bus_message_read(m, "sss", &name, &old, &now) >= 0 && *now)
        register_item();
    return 0;
}

/* ------------------------------------------------------------------ updates on disk */

static int same_file(const char *path, const struct stat *old)
{
    struct stat st;
    return stat(path, &st) != 0 || (st.st_ino == old->st_ino && st.st_mtim.tv_sec == old->st_mtim.tv_sec &&
                                    st.st_mtim.tv_nsec == old->st_mtim.tv_nsec);
}

static int files_settled(sd_event_source *s, uint64_t usec, void *u)
{
    (void)s; (void)usec; (void)u;
    if (!same_file(server_path, &st_server)) {
        stat(server_path, &st_server);
        if (use_systemd) {
            systemctl("daemon-reload", NULL, NULL);
            systemctl("reset-failed", UNIT, NULL);
            systemctl("try-restart", "--no-block", UNIT);
        }
    }
    if (!same_file(self_path, &st_self) && access(self_path, X_OK) == 0) {
        /* a new icon was installed: hand the name over and become the new version */
        if (!use_systemd)
            stop_child();
        sd_bus_release_name(bus, APP_ID);
        sd_bus_flush(bus);
        setenv("PC_PANEL_TRAY_REEXEC", "1", 1);
        execl(self_path, self_path, (char *)NULL);
        notify("Could not restart the tray icon after an update: %s", strerror(errno));
        sd_bus_request_name(bus, APP_ID, 0);
    }
    return 0;
}

static int inotify_ready(sd_event_source *s, int fd, uint32_t revents, void *u)
{
    (void)s; (void)revents; (void)u;
    char buf[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
    const char *self_name = strrchr(self_path, '/') + 1, *server_name = strrchr(server_path, '/') + 1;
    int hit = 0;
    ssize_t n;
    while ((n = read(fd, buf, sizeof buf)) > 0)
        for (char *p = buf; p < buf + n; p += sizeof(struct inotify_event) + ((struct inotify_event *)p)->len) {
            struct inotify_event *e = (struct inotify_event *)p;
            if (e->len && (!strcmp(e->name, self_name) || !strcmp(e->name, server_name)))
                hit = 1;
        }
    if (!hit)
        return 0;
    if (settle_timer) {
        sd_event_source_set_time_relative(settle_timer, 4000000);
        sd_event_source_set_enabled(settle_timer, SD_EVENT_ONESHOT);
    } else {
        sd_event_add_time_relative(ev, &settle_timer, CLOCK_MONOTONIC, 4000000, 0, files_settled, NULL);
    }
    return 0;
}

static void watch_files(void)
{
    if (appimage)
        return;                 /* files inside a mounted AppImage never change */
    int fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (fd < 0)
        return;
    uint32_t mask = IN_CLOSE_WRITE | IN_MOVED_TO | IN_CREATE | IN_ATTRIB;
    char server_dir[PATH_MAX];
    snprintf(server_dir, sizeof server_dir, "%s", server_path);
    *strrchr(server_dir, '/') = 0;
    inotify_add_watch(fd, self_dir, mask);
    if (strcmp(server_dir, self_dir))
        inotify_add_watch(fd, server_dir, mask);
    stat(self_path, &st_self);
    stat(server_path, &st_server);
    sd_event_add_io(ev, NULL, fd, EPOLLIN, inotify_ready, NULL);
}

/* ------------------------------------------------------------------ alerts from the dashboard */

static void read_alert(void)
{
    char buf[sizeof alert_text + 4] = "";
    int level = 0;
    FILE *f = fopen(alert_file, "r");
    if (f) {
        size_t n = fread(buf, 1, sizeof buf - 1, f);
        fclose(f);
        buf[n] = 0;
        if (buf[0] >= '0' && buf[0] <= '3')
            level = buf[0] - '0';
    }
    const char *text = strchr(buf, '\n');
    text = text ? text + 1 : "";
    size_t len = strlen(text);
    while (len && text[len - 1] == '\n')
        len--;
    if (level == alert_level && strlen(alert_text) == len && !strncmp(alert_text, text, len))
        return;
    snprintf(alert_text, sizeof alert_text, "%.*s", (int)len, text);
    alert_level = level;
    sd_bus_emit_signal(bus, SNI_PATH, SNI_IFACE, "NewIcon", "");
    sd_bus_emit_signal(bus, SNI_PATH, SNI_IFACE, "NewAttentionIcon", "");
    sd_bus_emit_signal(bus, SNI_PATH, SNI_IFACE, "NewStatus", "s", level >= 2 ? "NeedsAttention" : "Active");
    sd_bus_emit_signal(bus, SNI_PATH, SNI_IFACE, "NewToolTip", "");
}

static int alert_ready(sd_event_source *s, int fd, uint32_t revents, void *u)
{
    (void)s; (void)revents; (void)u;
    char buf[2048] __attribute__((aligned(__alignof__(struct inotify_event))));
    int hit = 0;
    ssize_t n;
    while ((n = read(fd, buf, sizeof buf)) > 0)
        for (char *p = buf; p < buf + n; p += sizeof(struct inotify_event) + ((struct inotify_event *)p)->len) {
            struct inotify_event *e = (struct inotify_event *)p;
            if (e->len && !strcmp(e->name, "alert"))
                hit = 1;
        }
    if (hit)
        read_alert();
    return 0;
}

static void watch_alert(void)
{
    int fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (fd < 0)
        return;
    mkdir(alert_dir, 0700);
    if (inotify_add_watch(fd, alert_dir, IN_CLOSE_WRITE | IN_MOVED_TO | IN_DELETE | IN_MOVED_FROM) < 0 ||
        sd_event_add_io(ev, NULL, fd, EPOLLIN, alert_ready, NULL) < 0) {
        close(fd);
        return;
    }
    read_alert();
}

/* ------------------------------------------------------------------ main */

static int on_signal(sd_event_source *s, const struct signalfd_siginfo *si, void *u)
{
    (void)s; (void)si; (void)u;
    /* logout or kill: leave the systemd service to systemd, never orphan a child */
    quitting = 1;
    stop_child();
    sd_event_exit(ev, 0);
    return 0;
}

static void setup_paths(void)
{
    const char *home = getenv("HOME"), *xdg = getenv("XDG_CONFIG_HOME");
    char base[PATH_MAX];
    if (!realpath("/proc/self/exe", self_path))
        snprintf(self_path, sizeof self_path, "/usr/bin/pc-panel-tray");
    snprintf(self_dir, sizeof self_dir, "%s", self_path);
    *strrchr(self_dir, '/') = 0;
    snprintf(server_path, sizeof server_path, "%s/pc-panel", self_dir);
    snprintf(base, sizeof base, "%s/../share/icons", self_dir);
    if (!realpath(base, icons_dir))
        icons_dir[0] = 0;
    if (xdg && *xdg == '/')
        snprintf(base, sizeof base, "%s", xdg);
    else
        snprintf(base, sizeof base, "%s/.config", home ? home : "/tmp");
    snprintf(cfg_dir, sizeof cfg_dir, "%s/pc-panel", base);
    snprintf(port_file, sizeof port_file, "%s/port", cfg_dir);
    snprintf(autostart_file, sizeof autostart_file, "%s/autostart/pc-panel.desktop", base);
    const char *run = getenv("XDG_RUNTIME_DIR");
    snprintf(alert_dir, sizeof alert_dir, "%s/pc-panel", run && *run == '/' ? run : "/tmp");
    snprintf(alert_file, sizeof alert_file, "%s/alert", alert_dir);
    appimage = getenv("APPIMAGE");
    if (appimage && !*appimage)
        appimage = NULL;
}

int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "--version")) {
        puts("pc-panel-tray " VERSION);
        return 0;
    }
    if (argc > 1) {
        fprintf(stderr, "usage: pc-panel-tray [--version]\n");
        return 2;
    }
    setup_paths();
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGTERM); sigaddset(&mask, SIGINT); sigaddset(&mask, SIGHUP); sigaddset(&mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask, NULL);
    signal(SIGPIPE, SIG_IGN);

    if (sd_event_default(&ev) < 0 || sd_bus_open_user(&bus) < 0) {
        fprintf(stderr, "pc-panel-tray: no D-Bus session bus\n");
        return 1;
    }
    port = read_port();
    int r = sd_bus_request_name(bus, APP_ID, 0);
    for (int i = 0; r == -EEXIST && getenv("PC_PANEL_TRAY_REEXEC") && i < 20; i++) {
        nanosleep(&(struct timespec){0, 200000000}, NULL);
        r = sd_bus_request_name(bus, APP_ID, 0);
    }
    unsetenv("PC_PANEL_TRAY_REEXEC");
    if (r == -EEXIST) {
        /* another PC Panel icon is already running: just open the dashboard */
        char u[64], *a[] = {"xdg-open", u, NULL};
        url(u, sizeof u);
        spawn_detached(a);
        return 0;
    }
    use_systemd = !appimage && systemctl("cat", UNIT, NULL) == 0;

    sd_bus_add_object_vtable(bus, NULL, SNI_PATH, SNI_IFACE, sni_vtable, NULL);
    sd_bus_add_object_vtable(bus, NULL, MENU_PATH, MENU_IFACE, menu_vtable, NULL);
    sd_bus_add_match(bus, NULL, "type='signal',sender='org.freedesktop.DBus',interface='org.freedesktop.DBus',"
                     "member='NameOwnerChanged',arg0='org.kde.StatusNotifierWatcher'", watcher_changed, NULL);
    sd_bus_attach_event(bus, ev, SD_EVENT_PRIORITY_NORMAL);
    sd_event_add_signal(ev, NULL, SIGTERM, on_signal, NULL);
    sd_event_add_signal(ev, NULL, SIGINT, on_signal, NULL);
    sd_event_add_signal(ev, NULL, SIGHUP, on_signal, NULL);
    register_item();
    start_dashboard();
    watch_files();
    watch_alert();

    sd_event_loop(ev);

    if (relaunch) {
        char *a[] = {(char *)appimage, NULL};
        sd_bus_release_name(bus, APP_ID);
        sd_bus_flush(bus);
        spawn_detached(a);
    }
    sd_bus_flush_close_unref(bus);
    sd_event_unref(ev);
    return 0;
}
