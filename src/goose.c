#define _POSIX_C_SOURCE 201710L
#include <fcntl.h>
#include <getopt.h>
#include <pango/pangocairo.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include "goose.h"


/* globals *******************************************************************/

struct Server   _server = {0};
int             _ipc_socket = -1;
struct timespec _start_time = {-1, -1};
Verbosity       _verbosity = LOG_LAST;
bool            _colored = true;

const char* log_colors[] = {
  [LOG_QUIET] = "",
  [LOG_ERROR] = "\x1B[1;31m",
  [LOG_INFO]  = "\x1B[1;32m",
  [LOG_DEBUG] = "\x1B[1;34m",
};
const char* log_tags[] = {
  [LOG_QUIET] = "",
  [LOG_ERROR] = "[ERROR]",
  [LOG_INFO]  = "[INFO]",
  [LOG_DEBUG] = "[DEBUG]",
};


/* entry *********************************************************************/

int
main(int argc, char** argv)
{
  int a;
  if (0 != (a = argue(argc, argv))) {
    exit(0 > a ? EXIT_FAILURE : EXIT_SUCCESS);
  }
  if (!init_all()) {
    exit(EXIT_FAILURE);
  }
  if (!start_server(&_server)) {
    quit(EXIT_FAILURE);
  }
  spawn("imv");
  run_server(&_server);
  quit(EXIT_SUCCESS);
}


int
argue(int argc, char** argv)
{
  int opt;
  struct option longs[] = {
    { "help", 0, NULL, 'h' },
    { "quiet", 0, NULL, 'q' },
    { "verbose", 0, NULL, 'v' },
    { "version", 0, NULL, 'V' }
  };

  while ((opt = getopt_long(argc, argv, "hv", longs, NULL)) != -1) {
    if ('h' == opt) {
      help(argv[0], EXIT_SUCCESS);
      return 1;
    } else if ('q' == opt) {
      _verbosity = LOG_QUIET;
    } else if ('v' == opt) {
      _verbosity = LOG_INFO;
      // count 'v's: debug
    } else if ('V' == opt) {
      version(argv[0]);
      return 1;
    } else {
      help(argv[0], EXIT_FAILURE);
      return -1;
    }
  }
  if (optind < argc) {
    help(argv[0], EXIT_FAILURE);
    return -1;
  }
  return 0;
}


void
help(char* me, int code)
{
  fprintf(EXIT_SUCCESS == code ? stdout : stderr,
          "Usage: %s -[hv]\n\
  -h --help     show this help\n\
  -v --version  show version", me);
}


void
version(char* me)
{
  fprintf(stdout, "%s 0.1", me);
}


void
handle_signal(int signal)
{
  quit(EXIT_SUCCESS);
}


/* log ***********************************************************************/

void
init_log()
{
  init_time();
  // TODO
}


void
init_time(void)
{
  if (0 <= _start_time.tv_sec) {
    return;
  }
  clock_gettime(CLOCK_MONOTONIC, &_start_time);
}


void
honk(Verbosity v, const char* format, ...)
{
  va_list args;
  va_start(args, format);
  _honk(v, format, args);
  va_end(args);
}


void
honk_va(Verbosity v, const char* format, va_list args)
{
  _honk(v, format, args);
}


void
_honk(Verbosity v, const char* format, va_list args)
{
  init_time();
  if (v > _verbosity) {
    return;
  }
  unsigned c = (LOG_LAST > v) ? v : LOG_LAST - 1;
  bool colored = _colored && isatty(STDERR_FILENO);
  if (colored) {
    fprintf(stderr, "%s", log_colors[c]);
  }
  fprintf(stderr, "%s%s ", ME, log_tags[c]);
  vfprintf(stderr, format, args);
  if (colored) {
    fprintf(stderr, "\x1B[0m");
  }
  fprintf(stderr, "\n");
}


/* spawn *********************************************************************/

void
spawn(char* c)
{
  honk(LOG_DEBUG, "spawning child: %s", c);
  int p[2];
  if (0 != pipe(p)) {
    honk(LOG_ERROR, "cannot create pipe for fork");
  }
  pid_t pid = -1;
  pid_t child = -1;
  if (0 == (pid = fork())) {
    setsid();
    sigset_t set;
    sigemptyset(&set);
    sigprocmask(SIG_SETMASK, &set, NULL);
    close(p[0]);
    if (0 == (child = fork())) {
      close(p[1]);
      honk(LOG_DEBUG, "starting imv");
      execl("/bin/sh", "/bin/sh", "-c", c, (void*)NULL);
      exit(0);
    }
    close(p[1]);
    exit(0);
  } else if (0 > pid) {
    close(p[0]);
    close(p[1]);
    honk(LOG_ERROR, "cannot fork");
    return;
  }
  close(p[1]);
  close(p[0]);
  waitpid(pid, NULL, 0);
  if (0 < child) {
    honk(LOG_DEBUG, "child created with pid: %d", child);
  }
}


/* bump **********************************************************************/


/* up ************************************************************************/

bool
init_all(void) {
  init_log(_verbosity);

  if (!getenv("XDG_RUNTIME_DIR")) {
    honk(LOG_ERROR, "XDG_RUNTIME_DIR must be set");
    return false;
  }

  if (!drop_root()) {
    return false;
  }

  signal(SIGTERM, handle_signal);
  signal(SIGINT, handle_signal);
  signal(SIGPIPE, SIG_IGN);

  prep_init();
  init_server();
  init_ipc(&_server);
  setenv("WAYLAND_DISPLAY", _server.socket, true);

  return true;
}


bool
drop_root(void)
{
  if (getuid() != geteuid() || getgid() != getegid()) {
    // set gid first
    if (0 != setgid(getgid())) {
      honk(LOG_ERROR, "cannot drop root group");
      return false;
    }
    if (0 != setuid(getuid())) {
      honk(LOG_ERROR, "cannot drop root user");
      return false;
    }
  }
  if (-1 != setgid(0) || -1 != setuid(0)) {
    honk(LOG_ERROR, "cannot drop root");
    return false;
  }
  return true;
}


bool
prep_init(void)
{
  honk(LOG_DEBUG, "preparing server init");
  _server.display = wl_display_create();
  _server.event_loop = wl_display_get_event_loop(_server.display);
  _server.backend = wlr_backend_autocreate(_server.display, NULL);
  if (!_server.backend) {
    honk(LOG_ERROR, "cannot create backend");
    return false;
  }
  return true;
}


bool
init_server(void)
{
  honk(LOG_DEBUG, "initialising server");
  struct wlr_renderer* renderer = wlr_backend_get_renderer(_server.backend);
  // TODO: what if no renderer?
  wlr_renderer_init_wl_display(renderer, _server.display);
  _server.compositor = wlr_compositor_create(_server.display, renderer);
  _server.compositor_new_surface.notify = handle_compositor_new_surface;
  wl_signal_add(&_server.compositor->events.new_surface,
                &_server.compositor_new_surface);
  _server.data_device_manager =
    wlr_data_device_manager_create(_server.display);
  wlr_gamma_control_manager_v1_create(_server.display);
  _server.new_output.notify = handle_new_output;
  wl_signal_add(&_server.backend->events.new_output, &_server.new_output);
  //_server.output_layout_change.notify = handle_output_layout_change;
  //wlr_xdg_output_manager_v1_create(_server.display);
  _server.idle = wlr_idle_create(_server.display);
  //_server.idle_inhibit_manager = sway_
  _server.layer_shell = wlr_layer_shell_v1_create(_server.display);
  wl_signal_add(&_server.layer_shell->events.new_surface,
                &_server.layer_shell_surface);
  _server.layer_shell_surface.notify = handle_layer_shell_surface;
  _server.xdg_shell = wlr_xdg_shell_create(_server.display);
  wl_signal_add(&_server.xdg_shell->events.new_surface,
                &_server.xdg_shell_surface);
  _server.xdg_shell_surface.notify = handle_xdg_shell_surface;
  _server.server_decoration_manager =
    wlr_server_decoration_manager_create(_server.display);
  wlr_server_decoration_manager_set_default_mode(
    _server.server_decoration_manager,
    WLR_SERVER_DECORATION_MANAGER_MODE_SERVER);
  wl_signal_add(&_server.server_decoration_manager->events.new_decoration,
                &_server.server_decoration);
  _server.server_decoration.notify = handle_server_decoration;
  wl_list_init(&_server.server_decorations);
  _server.xdg_decoration_manager =
    wlr_xdg_decoration_manager_v1_create(_server.display);
  wl_signal_add(
    &_server.xdg_decoration_manager->events.new_toplevel_decoration,
    &_server.xdg_decoration);
  _server.xdg_decoration.notify = handle_xdg_decoration;
  wl_list_init(&_server.xdg_decorations);
  _server.relative_pointer_manager =
    wlr_relative_pointer_manager_v1_create(_server.display);
  _server.pointer_constraints =
    wlr_pointer_constraints_v1_create(_server.display);
  _server.pointer_constraint.notify = handle_pointer_constraint;
  wl_signal_add(&_server.pointer_constraints->events.new_constraint,
                &_server.pointer_constraint);
  _server.presentation =
    wlr_presentation_create(_server.display, _server.backend);
  _server.output_manager =
    wlr_output_manager_v1_create(_server.display);
  _server.output_manager_apply.notify = handle_output_manager_apply;
  wl_signal_add(&_server.output_manager->events.apply,
                &_server.output_manager_apply);
  _server.output_manager_test.notify = handle_output_manager_test;
  wl_signal_add(&_server.output_manager->events.test,
                &_server.output_manager_test);
  _server.output_power_manager =
    wlr_output_power_manager_v1_create(_server.display);
  _server.output_power_manager_set_mode.notify =
    handle_output_power_manager_set_mode;
  wl_signal_add(&_server.output_power_manager->events.set_mode,
                &_server.output_power_manager_set_mode);
  _server.input_method_manager =
    wlr_input_method_manager_v2_create(_server.display);
  _server.text_input_manager =
    wlr_text_input_manager_v3_create(_server.display);
  _server.foreign_toplevel_manager =
    wlr_foreign_toplevel_manager_v1_create(_server.display);
  wlr_export_dmabuf_manager_v1_create(_server.display);
  wlr_screencopy_manager_v1_create(_server.display);
  wlr_data_control_manager_v1_create(_server.display);
  wlr_primary_selection_v1_device_manager_create(_server.display);
  wlr_viewporter_create(_server.display);

  char name[16];
  for (int i = 1; i <= 32; ++i) {
    sprintf(name, "wayland-%d", i);
    if (0 <= wl_display_add_socket(_server.display, name)) {
      _server.socket = strdup(name);
      break;
    }
  }
  if (!_server.socket) {
    honk(LOG_ERROR, "cannot open wayland socket");
    wlr_backend_destroy(_server.backend);
    return false;
  }

  _server.noop_backend = wlr_noop_backend_create(_server.display);
  struct wlr_output* output = wlr_noop_add_output(_server.noop_backend);
  _server.headless_backend =
    wlr_headless_backend_create_with_renderer(_server.display, renderer);
  if (!_server.headless_backend) {
    honk(LOG_INFO, "cannot create headless backend, starting without");
  } else {
    wlr_multi_backend_add(_server.backend, _server.headless_backend);
  }

  if (!_server.txn_timeout_ms) {
    _server.txn_timeout_ms = 200;
  }
  //_server.dirty_nodes = create_list();
  //_server.input = input_manager_create(_server);
  //input_manager_get_default_seat();

  return true;
}


void
init_ipc(struct Server* server)
{
  _ipc_socket = socket(AF_UNIX, SOCK_STREAM, 0);
  if (-1 == _ipc_socket) {
    honk(LOG_ERROR, "cannot create ipc socket");
    quit(EXIT_FAILURE);
  }
  if (-1 == fcntl(_ipc_socket, F_SETFD, FD_CLOEXEC)) {
    honk(LOG_ERROR, "cannot set CLOEXEC on ipc socket");
    quit(EXIT_FAILURE);
  }
  if (-1 == fcntl(_ipc_socket, F_SETFL, O_NONBLOCK)) {
    honk(LOG_ERROR, "cannot set NONBLOCK on ipc socket");
    quit(EXIT_FAILURE);
  }
}


bool
start_server(struct Server* server)
{
#ifdef XWAYLAND
  server->xwayland.xwayland =
    wlr_xwayland_create(server->display, server->compositor,
                        false); // XWAYLAND_MODE_LAZY
  if (!server->xwayland.xwayland) {
    honk(LOG_ERROR, "cannot start xwayland");
    unsetenv("DISPLAY");
  } else {
    wl_signal_add(&server->xwayland.xwayland->events.new_surface,
                  &server->xwayland_surface);
    server->xwayland_surface.notify = handle_xwayland_surface;
    wl_signal_add(&server->xwayland.xwayland->events.ready,
                  &server->xwayland_ready);
    server->xwayland_ready.notify = handle_xwayland_ready;
    setenv("DISPLAY", server->xwayland.xwayland->display_name, true);
    /* xcursor configured by default seat */
  }
#endif
  honk(LOG_INFO, "starting backend on display: %s", server->socket);
  if (!wlr_backend_start(server->backend)) {
    honk(LOG_INFO, "cannot start backend");
    wlr_backend_destroy(server->backend);
    return false;
  }
  return true;
}


void
run_server(struct Server* server)
{
  honk(LOG_INFO, "running %s on display: %s", ME, server->socket);
  wl_display_run(server->display);
}


/* down **********************************************************************/

void
quit(int code) {
  if (!_server.display) {
    // ipc client
    exit(code);
  }
  // server
  wl_display_terminate(_server.display);
  honk(LOG_INFO, "stopping %s on display: %s", ME, _server.socket);
  fin_server(&_server);
  pango_cairo_font_map_set_default(NULL);
  exit(code);
}


void
fin_server(struct Server* server) {
#ifdef XWAYLAND
  wlr_xwayland_destroy(server->xwayland.xwayland);
#endif
  wl_display_destroy_clients(server->display);
  wl_display_destroy(server->display);
}


/* desktop *******************************************************************/

void
handle_destroy(struct wl_listener* listener, void* data) {
  // TODO
}


void
handle_compositor_new_surface(struct wl_listener* listener, void* data) {
  struct wlr_surface* surface = data;
  // TODO
}


void
handle_new_output(struct wl_listener* listener, void* data) {
  struct wlr_output* output = data;
  // TODO
}


void
handle_output_layout_change(struct wl_listener* listener, void* data) {
  //struct Server* server =
  //  wl_container_of(listener, server, output_layout_change);
}


void
handle_layer_shell_surface(struct wl_listener* listener, void* data) {
  struct wlr_layer_surface_v1* surface = data;
  // TODO
}


void
handle_xdg_shell_surface(struct wl_listener* listener, void* data) {
  struct wlr_xdg_surface* surface = data;
  // TODO
}


void
handle_server_decoration(struct wl_listener* listener, void* data) {
  struct wlr_server_decoration* decoration = data;
  // TODO
}


void
handle_xdg_decoration(struct wl_listener* listener, void* data) {
  struct wlr_xdg_toplevel_decoration_v1* decoration = data;
  // TODO
}


void
handle_pointer_constraint(struct wl_listener* listener, void* data) {
  struct wlr_pointer_constraint_v1* coinstraint = data;
  // TODO
}


void
handle_output_manager_apply(struct wl_listener *listener, void *data) {
  struct wlr_output_configuration_v1* config = data;
  // TODO
}


void
handle_output_manager_test(struct wl_listener *listener, void *data) {
  struct wlr_output_configuration_v1* config = data;
  // TODO
}


void
handle_output_power_manager_set_mode(
  struct wl_listener *listener, void *data
) {
  struct wlr_output_power_v1_set_mode_event* event = data;
  // TODO
}


/* xwayland ******************************************************************/

// TODO: return object
void
create_unmanaged(struct wlr_xwayland_surface* surface)
{
  return;
}


// TODO: return object
void
create_xwayland_view(struct wlr_xwayland_surface* surface)
{
  return;
}


void
handle_xwayland_surface(struct wl_listener* listener, void* data)
{
  struct wlr_xwayland_surface* surface = data;
  if (surface->override_redirect) {
    honk(LOG_DEBUG, "new xwayland unmanaged surface");
    create_unmanaged(surface);
    return;
  }
  create_xwayland_view(surface);
}


void
handle_xwayland_ready(struct wl_listener* listener, void* data)
{
  struct Server* server = wl_container_of(listener, server, xwayland_ready);
  struct Xwayland* xwayland = &server->xwayland;
}

