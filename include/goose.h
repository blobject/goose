#ifndef _GOOSE_SERVER_H
#define _GOOSE_SERVER_H

#include <wlr/backend.h>
#include <wlr/backend/headless.h>
#include <wlr/backend/multi.h>
#include <wlr/backend/noop.h>
#include <wlr/backend/session.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_data_control_v1.h>
#include <wlr/types/wlr_export_dmabuf_v1.h>
#include <wlr/types/wlr_foreign_toplevel_management_v1.h>
#include <wlr/types/wlr_gamma_control_v1.h>
#include <wlr/types/wlr_idle.h>
#include <wlr/types/wlr_idle_inhibit_v1.h>
#include <wlr/types/wlr_input_method_v2.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_output_management_v1.h>
#include <wlr/types/wlr_output_power_management_v1.h>
#include <wlr/types/wlr_pointer_constraints_v1.h>
#include <wlr/types/wlr_presentation_time.h>
#include <wlr/types/wlr_primary_selection_v1.h>
#include <wlr/types/wlr_relative_pointer_v1.h>
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_server_decoration.h>
#include <wlr/types/wlr_text_input_v3.h>
#include <wlr/types/wlr_viewporter.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#ifdef XWAYLAND
#include <xcb/xcb.h>
#include <wlr/xwayland.h>
#endif


/* macros ********************************************************************/

#define HONK(stream, fmt, ...) do { fprintf(stream, fmt "\n", ##__VA_ARGS__); } while (0)
#define ERR(fmt, ...) do { HONK(stderr, fmt, ##__VA_ARGS__); exit(EXIT_FAILURE); } while (0)


/* structures ****************************************************************/

enum XwaylandAtom {
  NET_WM_WINDOW_TYPE_NORMAL,
  NET_WM_WINDOW_TYPE_DIALOG,
  NET_WM_WINDOW_TYPE_UTILITY,
  NET_WM_WINDOW_TYPE_TOOLBAR,
  NET_WM_WINDOW_TYPE_SPLASH,
  NET_WM_WINDOW_TYPE_MENU,
  NET_WM_WINDOW_TYPE_DROPDOWN_MENU,
  NET_WM_WINDOW_TYPE_POPUP_MENU,
  NET_WM_WINDOW_TYPE_TOOLTIP,
  NET_WM_WINDOW_TYPE_NOTIFICATION,
  NET_WM_STATE_MODAL,
  ATOM_LAST,
};


struct Xwayland {
  struct wlr_xwayland* xwayland;
  struct wlr_xcursor_manager* xcursor_manager;
  xcb_atom_t atoms[ATOM_LAST];
};


struct Server {
  struct wl_display*                      display;
  struct wl_event_loop*                   event_loop;
  const char*                             socket;
  struct wlr_backend*                     backend;
  struct wlr_backend*                     noop_backend;
  struct wlr_backend*                     headless_backend;
  struct wlr_compositor*                  compositor;
  struct wl_listener                      compositor_new_surface;
  struct wlr_data_device_manager*         data_device_manager;
  // input_manager
  struct wl_listener                      new_output;
  //struct wl_listener                      output_layout_change;
  struct wlr_idle*                        idle;
  //struct wlr_idle_inhibit_v1*             idle_inhibit;
  struct wlr_layer_shell_v1*              layer_shell;
  struct wl_listener                      layer_shell_surface;
  struct wlr_xdg_shell*                   xdg_shell;
  struct wl_listener                      xdg_shell_surface;
  #ifdef XWAYLAND
  struct Xwayland                         xwayland;
  struct wl_listener                      xwayland_surface;
  struct wl_listener                      xwayland_ready;
  #endif
  struct wlr_relative_pointer_manager_v1* relative_pointer_manager;
  struct wlr_server_decoration_manager*   server_decoration_manager;
  struct wl_listener                      server_decoration;
  struct wl_list                          server_decorations;
  struct wlr_xdg_decoration_manager_v1*   xdg_decoration_manager;
  struct wl_listener                      xdg_decoration;
  struct wl_list                          xdg_decorations;
  struct wlr_presentation*                presentation;
  struct wlr_pointer_constraints_v1*      pointer_constraints;
  struct wl_listener                      pointer_constraint;
  struct wlr_output_manager_v1*           output_manager;
  struct wl_listener                      output_manager_apply;
  struct wl_listener                      output_manager_test;
  struct wlr_output_power_manager_v1*     output_power_manager;
  struct wl_listener                      output_power_manager_set_mode;
  struct wlr_input_method_manager_v2*     input_method_manager;
  struct wlr_text_input_manager_v3*       text_input_manager;
  struct wlr_foreign_toplevel_manager_v1* foreign_toplevel_manager;
  size_t txn_timeout_ms;
  //list_t* dirty_nodes;
};


/* functions *****************************************************************/

/* program */

static void help(char* me, int code);
static void version(char* me);
static void handle_signal(int signal);

/* server */

static bool drop_root(void);
bool prep_init(void);
bool init_server(void);
void init_ipc(struct Server* server);
bool start_server(struct Server* server);
void run_server(struct Server* server);
void quit(int code);
void fin_server(struct Server* server);

/* desktop */

void handle_compositor_new_surface(struct wl_listener* listener, void* data);
void handle_new_output(struct wl_listener* listener, void* data);
void handle_output_layout_change(struct wl_listener* listener, void* data);
void handle_layer_shell_surface(struct wl_listener* listener, void* data);
void handle_xdg_shell_surface(struct wl_listener* listener, void* data);
void handle_server_decoration(struct wl_listener* listener, void* data);
void handle_xdg_decoration(struct wl_listener* listener, void* data);
void handle_pointer_constraint(struct wl_listener* listener, void* data);
void handle_output_manager_apply(struct wl_listener *listener, void *data);
void handle_output_manager_test(struct wl_listener *listener, void *data);
void handle_output_power_manager_set_mode(
  struct wl_listener *listener, void *data);

/* xwayland */

#ifdef XWAYLAND
void handle_xwayland_surface(struct wl_listener* listener, void* data);
void handle_xwayland_ready(struct wl_listener* listener, void* data);
#endif

#endif

