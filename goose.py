import ctypes

libwlroots = ctypes.CDLL("libwlroots.so.12")

bool_type = ctypes.c_uint8
pointer_type = ctypes.c_void_p

functions = {
    "wl_display_create": ((), pointer_type),
    "wlr_backend_autocreate": ((pointer_type,), pointer_type),
    "wlr_renderer_autocreate": ((pointer_type,), pointer_type),
    "wlr_renderer_init_wl_display": ((pointer_type, pointer_type), bool_type),
    "wlr_allocator_autocreate": ((pointer_type, pointer_type), pointer_type),
    "wlr_compositor_create": ((pointer_type, ctypes.c_uint32, pointer_type), pointer_type),
    "wlr_subcompositor_create": ((pointer_type,), pointer_type),
    "wlr_data_device_manager_create": ((pointer_type,), pointer_type),
    "wlr_output_layout_create": ((), pointer_type),
}

for func_name, (argtypes, restype) in functions.items():
    func = getattr(libwlroots, func_name)
    func.argtypes = argtypes
    func.restype = restype

display = libwlroots.wl_display_create()
backend = libwlroots.wlr_backend_autocreate(display)
renderer = libwlroots.wlr_renderer_autocreate(backend)
libwlroots.wlr_renderer_init_wl_display(renderer, display)
allocator = libwlroots.wlr_allocator_autocreate(backend, renderer)

libwlroots.wlr_compositor_create(display, 5, renderer)
libwlroots.wlr_subcompositor_create(display)
libwlroots.wlr_data_device_manager_create(display)
output_layout = libwlroots.wlr_output_layout_create()
