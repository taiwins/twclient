# wayland client utility library

This library wraps many routines of writing a wayland client, makes it much
easier to write a quick wayland client application, with very little
dependencies.

It provides also **nuklear** GUI library for creating GUI elements, thus makes
it ideal for creating tiny application under wayland.

## how to use this library

- initializing wayland client with a few lines:
```
	struct wl_display *wl_display = wl_display_connect(NULL);
	tw_globals_init(&tw_globals, wl_display);
	
	struct wl_registry *registry = wl_display_get_registry(wl_display);
	wl_registry_add_listener(registry, &registry_listener, NULL);
	
	wl_display_dispatch(wl_display);
	wl_display_roundtrip(wl_display);
```	

- now grabs a surface and hook it up with a nuklear backend.
```
	struct nk_wl_backend *nk_backend = nk_egl_create_backend(wl_display);
	struct wl_surface *wl_surface = wl_compositor_create_surface(tw_globals.compositor);
	struct wl_shell_surface *shell_surface = wl_shell_get_shell_surface(App.shell, wl_surface);
	tw_appsurf_init_default(&app_surface, wl_surface, &tw_globals);
	//deal with wl_shell_surface protocols.
	nk_wl_impl_wl_shell_surface(&app_surface, shell_surface);
	//hook it up with a nuklear backend
	nk_egl_impl_app_surface(&app_surface, nk_bkend, sample_widget, make_bbox_origin(200, 400, 1));
```	

- now you can run your application
```
	tw_appsurf_frame(&app_surface, false);
	wl_globals_dispatch_event_queue(&tw_globals);
```	

- when you are done, release the resources.
```
	wl_shell_surface_destroy(shell_surface);
	tw_appsurf_release(&app_surface);
	nk_egl_destroy_backend(nk_bkend);
	wl_globals_release(&tw_globals);
	wl_registry_destroy(registry);
	wl_display_disconnect(wl_display);
```

see the [EGL](test/test_nk_egl.c) and [cairo](test/test_nk_cairo.c) examples.


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
