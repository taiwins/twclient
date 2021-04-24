# wayland client utility library

This library wraps many routines of writing a wayland client, makes it much
easier to write a quick wayland client application, with few dependencies.

With the help of the [twidgets](https://github.com/taiwins/twidgets), you can
also create wayland window application under 20 lines of code. See twidgets for
an example.

## features
This library implements 
- event queue handler
- wl_global handlers (flexable, you can deal with your own `tw_globals`)
- an App surface for rendering.
- a few app surface implementations(calling `*_impl_app_surface()` for
  implementing a surface.
- a handler for packing icons and generating atlas textures.
- a handler for reading linux desktop entries.
- mores to come.
