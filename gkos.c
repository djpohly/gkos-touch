#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XInput2.h>

#include "gkos.h"

/*
 * 1 2 4 8 16 32
 * 3 6 24 48
 * 5 40
 * 11 19 35
 * 14 22 38
 * 25 26 28
 * 49 50 52
 * 13 29 21 53 37
 * 41 43 42 46 44
 *
 * 17 34 12 10 20 33 30 51
 *
 * 7  - BS
 * 9  - Up
 * 15 - Ctrl-Left (back one word)
 * 18 - Shift (lowercase->uppercase->capslock->lowercase)
 * 23 - Left
 * 27 - PgUp
 * 31 - Esc
 * 36 - Down
 * 39 - Home
 * 45 - Symbols (lowercase->symbols->lowercase)
 * 47 - Ctrl
 * 54 - PgDn
 * 55 - Alt
 * 56 - Space
 * 57 - Ctrl-Right (forward one word)
 * 58 - Right
 * 59 - Return
 * 60 - End
 * 61 - Tab
 * 62 - Del
 * 63 - Numbers (lowercase->numbers->lowercase)
 */



/*
 * Searches the input hierarchy for a touch-capable device.  The id parameter
 * gives a specific device ID to check, XIAllDevices, or XIAllMasterDevices.
 */
int init_touch_device(struct kbd_state *state, int id)
{
	// Get list of input devices and parameters
	XIDeviceInfo *di;
	int ndev;
	di = XIQueryDevice(state->dpy, id, &ndev);
	if (!di) {
		fprintf(stderr, "Failed to query devices\n");
		return 1;
	}

	// Find any direct touch devices (such as touchscreens)
	int i;
	for (i = 0; i < ndev; i++) {
		int j;
		for (j = 0; j < di[i].num_classes; j++) {
			XITouchClassInfo *tci =
				(XITouchClassInfo *) di[i].classes[j];
			if (tci->type == XITouchClass &&
					tci->mode == XIDirectTouch) {
				state->input_dev = di[i].deviceid;
				state->ntouches = tci->num_touches;
				goto done;
			}
		}
	}

done:
	XIFreeDeviceInfo(di);
	if (i >= ndev) {
		fprintf(stderr, "No touch device found\n");
		return 1;
	}

	state->touches = calloc(state->ntouches, sizeof(state->touches[0]));
	if (!state->touches) {
		fprintf(stderr, "Failed to allocate touches\n");
		return 1;
	}

	return 0;
}

/*
 * Clean up resources allocated for touch device
 */
void destroy_touch_device(struct kbd_state *state)
{
	free(state->touches);
}

/*
 * Establishes passive grabs for touch events on the given window
 */
int grab_touches(struct kbd_state *state, Window win)
{
	// Set up event mask for touch events
	unsigned char mask[XIMaskLen(XI_LASTEVENT)];
	memset(mask, 0, sizeof(mask));
	XISetMask(mask, XI_TouchBegin);
	XISetMask(mask, XI_TouchUpdate);
	XISetMask(mask, XI_TouchEnd);

	XIEventMask em = {
		.mask = mask,
		.mask_len = sizeof(mask),
		.deviceid = state->input_dev,
	};

	// Set up modifier structure
	XIGrabModifiers mods;
	mods.modifiers = XIAnyModifier;

	// Grab all the things
	return XIGrabTouchBegin(state->dpy, state->input_dev, win,
			XINoOwnerEvents, &em, 1, &mods);
}

/*
 * Releases passive grabs for touch events
 */
void ungrab_touches(struct kbd_state *state, Window win)
{
	// Reconstruct modifier structure
	XIGrabModifiers mods;
	mods.modifiers = XIAnyModifier;

	// Ungrab all the things
	XIUngrabTouchBegin(state->dpy, state->input_dev, win, 1, &mods);
}

/*
 * Returns the button window structure corresponding to a given X Window
 */
struct layout_win *get_layout_win(struct kbd_state *state, Window win)
{
	int i;
	for (i = 0; i < state->nwins; i++)
		if (state->wins[i].win == win)
			return &state->wins[i];
	return NULL;
}

/*
 * Creates and maps windows for the GKOS keyboard.
 */
int create_windows(struct kbd_state *state, const struct layout_btn *btns,
		int num_btns)
{
	int i;

	// Allocate space for windows
	state->wins = calloc(num_btns, sizeof(state->wins[0]));
	if (!state->wins)
		return 1;
	state->nwins = num_btns;

	// Set up the class hint for the windows
	XClassHint *class = XAllocClassHint();
	if (!class) {
		fprintf(stderr, "Failed to allocate class hint\n");
		free(state->wins);
		return 1;
	}
	class->res_name = class->res_class = "GKOS-multitouch";

	// Create the main fullscreen window
	Screen *scr = DefaultScreenOfDisplay(state->dpy);
	int swidth = WidthOfScreen(scr);
	int sheight = HeightOfScreen(scr);
	XSetWindowAttributes attrs = {
		.background_pixel = 0x00000000,
		.border_pixel = 0x00000000,
		.override_redirect = True,
		.colormap = state->cmap,
	};
	state->win = XCreateWindow(state->dpy, DefaultRootWindow(state->dpy),
			0, 0, swidth, sheight, 0,
			state->xvi.depth, InputOutput, state->xvi.visual,
			CWBackPixel | CWBorderPixel | CWOverrideRedirect | CWColormap, &attrs);
	XSetClassHint(state->dpy, state->win, class);
	XSelectInput(state->dpy, state->win, StructureNotifyMask);

	// Free the class hint
	XFree(class);

	// Prepare attributes for subwindows
	attrs.background_pixel = UNPRESSED_COLOR;
	attrs.border_pixel = BORDER_COLOR;

	for (i = 0; i < state->nwins; i++) {
		// Calculate window position in grid
		int x = btns[i].x * GRID_X;
		if (x < 0)
			x += swidth;
		int y = btns[i].y * GRID_Y + TOP_Y;
		int h = btns[i].h * GRID_Y;

		// Create subwindow
		state->wins[i].win = XCreateWindow(state->dpy, state->win,
				x, y, GRID_X-2, h-2, 1,
				state->xvi.depth, InputOutput, state->xvi.visual,
				CWBackPixel | CWBorderPixel | CWOverrideRedirect | CWColormap,
				&attrs);
		state->wins[i].bits = btns[i].bits;
	}

	// Grab touch events for the new window
	if (grab_touches(state, state->win)) {
		fprintf(stderr, "Failed to grab touch event\n");
		return 1;
	}

	// Map everything and wait for the notify event
	XMapSubwindows(state->dpy, state->win);
	XMapWindow(state->dpy, state->win);

	XEvent ev;
	while (XMaskEvent(state->dpy, StructureNotifyMask, &ev) == Success &&
			(ev.type != MapNotify || ev.xmap.event != state->win))
		;

	return 0;
}

/*
 * Tear down keyboard windows
 */
void destroy_windows(struct kbd_state *state)
{
	int i;
	for (i = 0; i < state->nwins; i++)
		XDestroyWindow(state->dpy, state->wins[i].win);
	free(state->wins);

	ungrab_touches(state, state->win);
	XDestroyWindow(state->dpy, state->win);
}

/*
 * Calculate the bits corresponding to the currently touched windows
 */
uint8_t get_pressed_bits(struct kbd_state *state)
{
	uint8_t bits = 0;
	int i;
	for (i = 0; i < state->ntouches; i++)
		if (state->touches[i])
			bits |= state->touches[i]->bits;
	return bits;
}

/*
 * Turn on/off a window's highlight
 */
void highlight_win(struct kbd_state *state, Window win, int on)
{
	XWindowAttributes attrs;
	XGetWindowAttributes(state->dpy, win, &attrs);

	XSetForeground(state->dpy, state->gc,
			on ? PRESSED_COLOR : UNPRESSED_COLOR);
	XFillRectangle(state->dpy, win, state->gc, 0, 0,
			attrs.width, attrs.height);
}

/*
 * Highlight windows according to what is currently pressed
 */
void update_display(struct kbd_state *state)
{
	uint8_t bits = get_pressed_bits(state);
	int i;
	for (i = 0; i < state->nwins; i++) {
		int on = (bits & state->wins[i].bits) == state->wins[i].bits;
		highlight_win(state, state->wins[i].win, on);
	}
}

/*
 * Remember a window as "touched" at the start of a touch event
 */
int add_touch(struct kbd_state *state, Window win)
{
	struct layout_win *lwin = get_layout_win(state, win);
	if (!lwin) {
		fprintf(stderr, "Couldn't get touched window\n");
		return 1;
	}

	int i;
	for (i = 0; i < state->ntouches && state->touches[i]; i++)
		;
	if (i >= state->ntouches) {
		fprintf(stderr, "No open touch slots found\n");
		return 1;
	}

	state->touches[i] = lwin;
	update_display(state);
	return 0;
}

/*
 * Unregister a touched window when the touch is released
 */
int remove_touch(struct kbd_state *state, Window win)
{
	int i;
	for (i = 0; i < state->ntouches; i++)
		if (state->touches[i] && state->touches[i]->win == win)
			break;
	if (i >= state->ntouches) {
		fprintf(stderr, "Released window was not touched\n");
		return 1;
	}

	state->touches[i] = NULL;
	update_display(state);
	return 0;
}

/*
 * Event handling for XInput generic events
 */
int handle_xi_event(struct kbd_state *state, XIDeviceEvent *ev)
{
	switch (ev->evtype) {
		case XI_TouchBegin:
			if (!ev->child || ev->child == state->win)
				break;
			if (add_touch(state, ev->child))
				return 1;
			state->active = 1;
			break;
		case XI_TouchUpdate:
			//fprintf(stderr, "touch update\n");
			break;
		case XI_TouchEnd:
			if (!ev->child || ev->child == state->win)
				break;
			if (state->active) {
				fprintf(stderr, "bits %u\n", get_pressed_bits(state));
				state->active = 0;
			}
			if (remove_touch(state, ev->child))
				return 1;
			break;
		default:
			fprintf(stderr, "other event %d\n", ev->evtype);
			break;
	}
	return 0;
}

/*
 * Main event handling loop
 */
int event_loop(struct kbd_state *state)
{
	XEvent ev;
	XGenericEventCookie *cookie = &ev.xcookie;

	while (XNextEvent(state->dpy, &ev) == Success) {
		if (ev.type == GenericEvent &&
				cookie->extension == state->xi_opcode &&
				XGetEventData(state->dpy, cookie)) {
			// GenericEvent from XInput
			handle_xi_event(state, cookie->data);
			XFreeEventData(state->dpy, cookie);
		} else {
			// Regular event type
			switch (ev.type) {
				case MappingNotify:
					XRefreshKeyboardMapping(&ev.xmapping);
					break;
				default:
					fprintf(stderr, "regular event %d\n", ev.type);
			}
		}
	}

	return 0;
}

/*
 * Program entry point
 */
int main(int argc, char **argv)
{
	int ret = 0;

	struct kbd_state state;
	state.active = 0;

	// Open display
	state.dpy = XOpenDisplay(NULL);
	if (!state.dpy) {
		fprintf(stderr, "Could not open display\n");
		return 1;
	}

	// Ensure we have XInput...
	int xi_event, xi_error;
	if (!XQueryExtension(state.dpy, "XInputExtension", &state.xi_opcode,
				&xi_event, &xi_error)) {
		ret = 1;
		fprintf(stderr, "Server does not support XInput\n");
		goto out_close;
	}

	// ... in particular, XInput version 2.2
	int major = 2, minor = 2;
	XIQueryVersion(state.dpy, &major, &minor);
	if (major * 1000 + minor < 2002) {
		ret = 1;
		fprintf(stderr, "Server does not support XInput 2.2\n");
		goto out_close;
	}

	// Get a specific device if given, otherwise find anything capable of
	// direct-style touch input
	int id = (argc > 1) ? atoi(argv[1]) : XIAllDevices;
	ret = init_touch_device(&state, id);
	if (ret)
		goto out_close;

	// Get visual and colormap for transparent windows
	ret = !XMatchVisualInfo(state.dpy, DefaultScreen(state.dpy),
				32, TrueColor, &state.xvi);
	if (ret) {
		fprintf(stderr, "Couldn't find 32-bit visual\n");
		goto out_destroy_touch;
	}

	state.cmap = XCreateColormap(state.dpy, DefaultRootWindow(state.dpy),
			state.xvi.visual, AllocNone);

	// Create and map windows for keyboard
	ret = create_windows(&state, default_btns,
			sizeof(default_btns) / sizeof(default_btns[0]));
	if (ret) {
		fprintf(stderr, "Failed to create windows\n");
		goto out_free_cmap;
	}

	state.gc = XCreateGC(state.dpy, state.win, 0, NULL);

	ret = event_loop(&state);

	XFreeGC(state.dpy, state.gc);

	destroy_windows(&state);

out_free_cmap:
	XFreeColormap(state.dpy, state.cmap);

out_destroy_touch:
	destroy_touch_device(&state);

out_close:
	XCloseDisplay(state.dpy);
	return ret;
}
