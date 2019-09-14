#ifndef UI_EVENT_H
#define UI_EVENT_H

#include <stdbool.h>
#include <stdint.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-names.h>
#include <xkbcommon/xkbcommon-keysyms.h>

#ifdef __cplusplus
extern "C" {
#endif


//So the app surface becomes a event based surface. What we wanted to avoid in
//the beginning. When the events becames so complex.
enum app_event_type {
	/**
	 * app_surface would receive of such event and frame call, most of those
	 * events are interested by application only. We do not make event
	 * system sophisticated like QEvent ,thus we do take many assumptions
	 * for the app so we can preprocess many events as possible, for
	 * example, keymap is handled by xkbcommon in the client library, so
	 * user are forced to use it.
	 *
	 * pointer and touch has a frame concept, so we need to aggregate them
	 * and send them there
	 */
	//general
	TW_FOCUS, TW_UNFOCUS,
	//called app_surface_frame
	TW_FRAME_START,
	//animation
	TW_TIMER,
	//pointer
	TW_POINTER_MOTION, TW_POINTER_BTN, TW_POINTER_AXIS,
	//keyboard
	TW_KEY_BTN, //no keymap event, repeat info, or modifier. We output modifier
		 //directly
	//touch
	TW_TOUCH_MOTION,
	//resize, fullscreen and all that should be here as well.
	TW_RESIZE,
	TW_FULLSCREEN,
	TW_MINIMIZE,
};

struct app_event {
	enum app_event_type type;
	uint32_t time; //maybe I should process it take timestamp, like milliseconds
	union {
		struct {
			xkb_keycode_t code;
			xkb_keysym_t sym;
			uint32_t mod;
			bool state;
		} key;
		struct {
			uint32_t mod;
			uint32_t x, y;
			uint32_t btn; bool state;
		} ptr;

		struct {
			uint32_t mod;
			//in the context of touch pad, this represents the
			//travel of finger in surface coordinates
			int dx; int dy;
		} axis;
		struct {
			uint32_t mod;
		} touch;
		//resize and max/minimize requires specific protocols
		//`wl_shell_surface` or `xdg_shell_surface`. app_surface will
		//need to have those protocols ready

		//
		struct {
			//xdg surface and shell surface share the same enum
			uint32_t edge;
			uint32_t nw, nh, ns;
			uint32_t serial;
		} resize;
	};
};



#ifdef __cplusplus
}
#endif



#endif /* EOF */
