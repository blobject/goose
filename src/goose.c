#define _POSIX_C_SOURCE 201710L
#include <fcntl.h>
#include <getopt.h>
#include <pango/pangocairo.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "goose.h"


/* globals *******************************************************************/

struct Server server = {0};
int ipc_socket = -1;


/* entry *********************************************************************/

int
main(int argc, char** argv)
{
  int opt;
  static struct option long_options[] = {
    { "help", 0, NULL, 'h' },
    { "version", 0, NULL, 'v' }
  };

  while ((opt = getopt_long(argc, argv, "hv", long_options, NULL)) != -1) {
    if ('h' == opt) {
      help(argv[0], EXIT_SUCCESS);
    } else if ('v' == opt) {
      version(argv[0]);
      exit(EXIT_SUCCESS);
    } else {
      help(argv[0], EXIT_FAILURE);
    }
  }
  if (optind < argc) {
    help(argv[0], EXIT_FAILURE);
  }

  if (!getenv("XDG_RUNTIME_DIR")) {
    ERR("XDG_RUNTIME_DIR must be set");
  }

  if (!drop_root()) {
    fin_server(&server);
    exit(EXIT_FAILURE);
  }

  signal(SIGTERM, handle_signal);
  signal(SIGINT, handle_signal);
  signal(SIGPIPE, SIG_IGN);

  prep_init();
  init_server();
  init_ipc(&server);
  setenv("WAYLAND_DISPLAY", server.socket, true);
  if (!start_server(&server)) {
    quit(EXIT_FAILURE);
  }
  run_server(&server);
  quit(EXIT_SUCCESS);
}


static void
help(char* me, int code)
{
  HONK(stdout, "Usage: %s -[hv]\n\
  -h --help     show this help\n\
  -v --version  show version", me);
  exit(code);
}


void
version(char* me)
{
  HONK(stdout, "%s 0.1", me);
}


void
handle_signal(int signal)
{
  quit(EXIT_SUCCESS);
}


/* up ************************************************************************/

static bool
drop_root(void)
{
  if (getuid() != geteuid() || getgid() != getegid()) {
    // set gid first
    if (setgid(getgid()) != 0) {
      // TODO: log, cannot drop root group
      return false;
    }
    if (setuid(getuid()) != 0) {
      // TODO: log, cannot drop root user
      return false;
    }
  }
  if (setgid(0) != -1 || setuid(0) != -1) {
    // TODO: log, cannot drop root
    return false;
  }
  return true;
}


bool
prep_init(void)
{
  // TODO: log, preparing wayland server init
  server.display = wl_display_create();
  server.event_loop = wl_display_get_event_loop(server.display);
  server.backend = wlr_backend_autocreate(server.display, NULL);
  if (!server.backend) {
    // TODO: log, cannot create backend
    return false;
  }
  return true;
}


bool
init_server(void)
{
  // TODO: log, initialising wayland server
  struct wlr_renderer* renderer = wlr_backend_get_renderer(server.backend);
  // TODO: what if no renderer?
  wlr_renderer_init_wl_display(renderer, server.display);
  server.compositor = wlr_compositor_create(server.display, renderer);
  server.compositor_new_surface.notify = handle_compositor_new_surface;
  wl_signal_add(&server.compositor->events.new_surface,
                &server.compositor_new_surface);
  server.data_device_manager = wlr_data_device_manager_create(server.display);
  wlr_gamma_control_manager_v1_create(server.display);
  server.new_output.notify = handle_new_output;
  wl_signal_add(&server.backend->events.new_output, &server.new_output);
  //server.output_layout_change.notify = handle_output_layout_change;
  //wlr_xdg_output_manager_v1_create(server.display);
  server.idle = wlr_idle_create(server.display);
  //server.idle_inhibit_manager = sway_
  server.layer_shell = wlr_layer_shell_v1_create(server.display);
  wl_signal_add(&server.layer_shell->events.new_surface,
                &server.layer_shell_surface);
  server.layer_shell_surface.notify = handle_layer_shell_surface;
  server.xdg_shell = wlr_xdg_shell_create(server.display);
  wl_signal_add(&server.xdg_shell->events.new_surface,
                &server.xdg_shell_surface);
  server.xdg_shell_surface.notify = handle_xdg_shell_surface;
  server.server_decoration_manager =
    wlr_server_decoration_manager_create(server.display);
  wlr_server_decoration_manager_set_default_mode(
    server.server_decoration_manager,
    WLR_SERVER_DECORATION_MANAGER_MODE_SERVER);
  wl_signal_add(&server.server_decoration_manager->events.new_decoration,
                &server.server_decoration);
  server.server_decoration.notify = handle_server_decoration;
  wl_list_init(&server.server_decorations);
  server.xdg_decoration_manager =
    wlr_xdg_decoration_manager_v1_create(server.display);
  wl_signal_add(
    &server.xdg_decoration_manager->events.new_toplevel_decoration,
    &server.xdg_decoration);
  server.xdg_decoration.notify = handle_xdg_decoration;
  wl_list_init(&server.xdg_decorations);
  server.relative_pointer_manager =
    wlr_relative_pointer_manager_v1_create(server.display);
  server.pointer_constraints =
    wlr_pointer_constraints_v1_create(server.display);
  server.pointer_constraint.notify = handle_pointer_constraint;
  wl_signal_add(&server.pointer_constraints->events.new_constraint,
                &server.pointer_constraint);
  server.presentation =
    wlr_presentation_create(server.display, server.backend);
  server.output_manager =
    wlr_output_manager_v1_create(server.display);
  server.output_manager_apply.notify = handle_output_manager_apply;
  wl_signal_add(&server.output_manager->events.apply,
                &server.output_manager_apply);
  server.output_manager_test.notify = handle_output_manager_test;
  wl_signal_add(&server.output_manager->events.test,
                &server.output_manager_test);
  server.output_power_manager =
    wlr_output_power_manager_v1_create(server.display);
  server.output_power_manager_set_mode.notify =
    handle_output_power_manager_set_mode;
  wl_signal_add(&server.output_power_manager->events.set_mode,
                &server.output_power_manager_set_mode);
  server.input_method_manager =
    wlr_input_method_manager_v2_create(server.display);
  server.text_input_manager = wlr_text_input_manager_v3_create(server.display);
  server.foreign_toplevel_manager =
    wlr_foreign_toplevel_manager_v1_create(server.display);
  wlr_export_dmabuf_manager_v1_create(server.display);
  wlr_screencopy_manager_v1_create(server.display);
  wlr_data_control_manager_v1_create(server.display);
  wlr_primary_selection_v1_device_manager_create(server.display);
  wlr_viewporter_create(server.display);

  char name[16];
  for (int i = 1; i <= 32; ++i) {
    sprintf(name, "wayland-%d", i);
    if (wl_display_add_socket(server.display, name) >= 0) {
      server.socket = strdup(name);
      break;
    }
  }
  if (!server.socket) {
    // TODO: log, unable to open wayland socket
    wlr_backend_destroy(server.backend);
    return false;
  }

  server.noop_backend = wlr_noop_backend_create(server.display);
  struct wlr_output* output = wlr_noop_add_output(server.noop_backend);
  server.headless_backend =
    wlr_headless_backend_create_with_renderer(server.display, renderer);
  if (!server.headless_backend) {
    // TODO: log, cannot create headless backend, starting without
  } else {
    wlr_multi_backend_add(server.backend, server.headless_backend);
  }

  if (!server.txn_timeout_ms) {
    server.txn_timeout_ms = 200;
  }
  //server.dirty_nodes = create_list();
  //server.input = input_manager_create(server);
  //input_manager_get_default_seat();

  return true;
}


void
init_ipc(struct Server* server)
{
  ipc_socket = socket(AF_UNIX, SOCK_STREAM, 0);
  if (ipc_socket == -1) {
    // TODO: quit, cannot create ipc socket
  }
  if (fcntl(ipc_socket, F_SETFD, FD_CLOEXEC) == -1) {
    // TODO: quit, cannot set CLOEXEC on ipc socket
  }
  if (fcntl(ipc_socket, F_SETFL, O_NONBLOCK) == -1) {
    // TODO: quit, cannot set NONBLOCK on ipc socket
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
    // TODO: log, cannot start xwayland
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
  // TODO: log, starting backend on wayland display: %s, server.socket
  if (!wlr_backend_start(server->backend)) {
    // TODO: log, cannot start backend
    wlr_backend_destroy(server->backend);
    return false;
  }
  return true;
}


void
run_server(struct Server* server)
{
  // TODO: log, running ME on wayland display: %s, server.socket
  wl_display_run(server->display);
}


/* down **********************************************************************/

void
quit(int code) {
  if (!server.display) {
    // ipc client
    exit(code);
  }
  // server
  wl_display_terminate(server.display);
  // TODO: log, closing
  fin_server(&server);
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

static void
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
    // TODO: log, new xwayland unmanaged surface
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

