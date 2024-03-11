#include <tcl.h>

#define USE_TCL_STUBS

static int
Hello_Cmd(ClientData data, Tcl_Interp* interp, int objc, Tcl_Obj* const objv[])
{
  Tcl_SetObjResult(interp, Tcl_NewStringObj("Hello, World!", -1));
  return TCL_OK;
}

int DLLEXPORT
Goosebump_Init(Tcl_Interp* interp)
{
  if (Tcl_InitStubs(interp, TCL_VERSION, 0) == NULL
      || Tcl_PkgProvide(interp, "Hello", "1.0") == TCL_ERROR) {
    return TCL_ERROR;
  }
  Tcl_CreateObjCommand(interp, "hello", Hello_Cmd, NULL, NULL);
  return TCL_OK;
}

