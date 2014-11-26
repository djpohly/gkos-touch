#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <math.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/XTest.h>

#include "chorder.h"
#include "english_optimized.h"
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

	state->touchids = calloc(state->ntouches, sizeof(state->touchids[0]));
	if (!state->touchids) {
		fprintf(stderr, "Failed to allocate touchids\n");
		free(state->touches);
		return 1;
	}

	return 0;
}

/*
 * Clean up resources allocated for touch device
 */
void destroy_touch_device(struct kbd_state *state)
{
	free(state->touchids);
	free(state->touches);
}

/*
 * Establishes passive grabs for touch events on the given window
 */
int grab_touches(struct kbd_state *state)
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

	// Grab the touch device
	return XIGrabDevice(state->dpy, state->input_dev,
			DefaultRootWindow(state->dpy), CurrentTime, None,
			XIGrabModeAsync, XIGrabModeAsync, False, &em);
}

/*
 * Releases passive grabs for touch events
 */
void ungrab_touches(struct kbd_state *state)
{
	// Ungrab all the things
	XIUngrabDevice(state->dpy, state->input_dev, CurrentTime);
}

/*
 * Returns the button window structure, if any, at the given coordinates
 */
struct layout_win *get_layout_win(struct kbd_state *state, double x, double y)
{
	int i;
	for (i = 0; i < state->nwins; i++) {
		double r = sqrt(pow(x - state->wins[i].cx, 2) + pow(state->wins[i].cy - y, 2));
		int th = 64 * 180 * atan2(state->wins[i].cy - y, x - state->wins[i].cx) / M_PI;
		if (r >= state->wins[i].r1 && r <= state->wins[i].r2 &&
				th >= state->wins[i].th &&
				th <= state->wins[i].th + state->wins[i].dth)
			return &state->wins[i];
	}
	return NULL;
}

/*
 * Creates and maps windows for the GKOS keyboard.
 */
int create_windows(struct kbd_state *state, const struct layout_btn *btns,
		int num_btns)
{
	int i;

	// Allocate space for keys on both sides
	state->wins = calloc(num_btns * 2, sizeof(state->wins[0]));
	if (!state->wins)
		return 1;
	state->nwins = num_btns * 2;

	// Set up the class hint for the GKOS window
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
		.background_pixel = TRANSPARENT,
		.border_pixel = TRANSPARENT,
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

	// Calculate key positions in grid
	for (i = 0; i < state->nwins / 2; i++) {
		// Index of mirrored key
		int m = i + state->nwins / 2;

		state->wins[i].r1 = state->wins[m].r1 = IR + btns[i].row * DR;
		state->wins[i].r2 = state->wins[m].r2 = state->wins[i].r1 + DR;
		state->wins[i].th = 5760 - (btns[i].th + btns[i].dth) * DTH;
		state->wins[m].th = 5760 + btns[i].th * DTH;
		state->wins[i].dth = state->wins[m].dth = btns[i].dth * DTH;
		state->wins[i].cx = CX;
		state->wins[m].cx = swidth - 1 - CX;
		state->wins[i].cy = state->wins[m].cy = CY;
		state->wins[i].bits = btns[i].bits;
		state->wins[m].bits = btns[i].bits << 3;
	}

	// Grab touch events for the new window
	if (grab_touches(state)) {
		fprintf(stderr, "Failed to grab touch event\n");
		free(state->wins);
		return 1;
	}

	return 0;
}

void map_windows(struct kbd_state *state)
{
	// Map everything and wait for the notify event
	XMapWindow(state->dpy, state->win);

	XEvent ev;
	while (XMaskEvent(state->dpy, StructureNotifyMask, &ev) == Success &&
			(ev.type != MapNotify || ev.xmap.event != state->win))
		;
}

/*
 * Tear down keyboard windows
 */
void destroy_windows(struct kbd_state *state)
{
	free(state->wins);

	ungrab_touches(state);
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
		if (state->touchids[i] && state->touches[i])
			bits |= state->touches[i]->bits;
	return bits;
}

/*
 * Turn on/off a window's highlight
 */
void highlight_win(struct kbd_state *state, struct layout_win *win, int on)
{
	// Fill
	XSetForeground(state->dpy, state->gc,
			on ? PRESSED_COLOR : UNPRESSED_COLOR);
	XFillArc(state->dpy, state->win, state->gc,
			win->cx - win->r2, win->cy - win->r2,
			2*win->r2, 2*win->r2,
			win->th, win->dth);
	XSetForeground(state->dpy, state->gc, TRANSPARENT);
	XFillArc(state->dpy, state->win, state->gc,
			win->cx - win->r1, win->cy - win->r1,
			2*win->r1, 2*win->r1,
			win->th, win->dth);

	// Border
	XSetForeground(state->dpy, state->gc, BORDER_COLOR);
	XArc arcs[] = {
		{
			.x = win->cx - win->r2, .y = win->cy - win->r2,
			.width = 2*win->r2, .height = 2*win->r2,
			.angle1 = win->th, .angle2 = win->dth,
		},
		{
			.x = win->cx - win->r1, .y = win->cy - win->r1,
			.width = 2*win->r1, .height = 2*win->r1,
			.angle1 = win->th, .angle2 = win->dth,
		},
	};
	XDrawArcs(state->dpy, state->win, state->gc,
			arcs, sizeof(arcs)/sizeof(arcs[0]));
	XSegment segs[] = {
		{
			.x1 = win->cx + win->r2 * cos(M_PI * win->th / 11520.0),
			.x2 = win->cx + win->r1 * cos(M_PI * win->th / 11520.0),
			.y1 = win->cy - win->r2 * sin(M_PI * win->th / 11520.0),
			.y2 = win->cy - win->r1 * sin(M_PI * win->th / 11520.0),
		},
		{
			.x1 = win->cx + win->r2 * cos(M_PI * (win->th+win->dth) / 11520.0),
			.x2 = win->cx + win->r1 * cos(M_PI * (win->th+win->dth) / 11520.0),
			.y1 = win->cy - win->r2 * sin(M_PI * (win->th+win->dth) / 11520.0),
			.y2 = win->cy - win->r1 * sin(M_PI * (win->th+win->dth) / 11520.0),
		},
	};
	XDrawSegments(state->dpy, state->win, state->gc,
			segs, sizeof(segs)/sizeof(segs[0]));
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
		highlight_win(state, &state->wins[i], on);
	}
}

/*
 * Remember a window as "touched" at the start of a touch event
 */
int add_touch(struct kbd_state *state, struct layout_win *lwin, int touchid)
{
	int i;
	for (i = 0; i < state->ntouches && state->touchids[i]; i++)
		;
	if (i >= state->ntouches) {
		fprintf(stderr, "No open touch slots found\n");
		return 1;
	}

	state->touches[i] = lwin;
	state->touchids[i] = touchid;
	update_display(state);
	return 0;
}

/*
 * Find the touch index corresponding to the given touch event ID
 */
int get_touch_index(struct kbd_state *state, int touchid)
{
	int i;
	for (i = 0; i < state->ntouches; i++)
		if (state->touchids[i] == touchid)
			return i;

	return -1;
}

/*
 * Unregister a touched window when the touch is released
 */
int remove_touch(struct kbd_state *state, int index)
{
	state->touches[index] = NULL;
	state->touchids[index] = 0;
	update_display(state);
	return 0;
}

/*
 * Chorder press handler
 */
void handle_press(void *arg, unsigned long sym, int press)
{
	struct kbd_state *state = arg;
	unsigned long code = XKeysymToKeycode(state->dpy, sym);
	XTestFakeKeyEvent(state->dpy, code, press, CurrentTime);
}

/*
 * Event handling for XInput generic events
 */
int handle_xi_event(struct kbd_state *state, XIDeviceEvent *ev)
{
	struct layout_win *lwin;
	int idx;

	switch (ev->evtype) {
		case XI_TouchBegin:
			// Claim the touch event
			XIAllowTouchEvents(state->dpy, state->input_dev,
					ev->detail, ev->event, XIAcceptTouch);

			// Find and record which window was touched
			lwin = get_layout_win(state, ev->root_x, ev->root_y);
			if (add_touch(state, lwin, ev->detail))
				return 1;

			// Comes after add_touch so we can recognize touches
			// that were outside a defined window
			if (!lwin)
				break;

			state->active = 1;
			break;
		case XI_TouchUpdate:
			//fprintf(stderr, "touch update\n");
			break;
		case XI_TouchEnd:
			// Find which touch was released
			idx = get_touch_index(state, ev->detail);
			if (idx < 0) {
				fprintf(stderr, "Released window was not touched\n");
				return 1;
			}

			// Shut down on release of an outside touch
			if (!state->touches[idx]) {
				state->shutdown = 1;
				break;
			}

			// If this is the first release after a touch, generate
			// key event
			if (state->active) {
				chorder_press(&state->chorder,
						get_pressed_bits(state));
				state->active = 0;
			}

			// Update touch tracking
			if (remove_touch(state, idx))
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

	while (!state->shutdown && XNextEvent(state->dpy, &ev) == Success) {
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
	state.shutdown = 0;

	// Initialize chorder
	chorder_init(&state.chorder, (const struct chord_entry *) map,
			3, 64, handle_press, &state);

	// Open display
	state.dpy = XOpenDisplay(NULL);
	if (!state.dpy) {
		ret = 1;
		fprintf(stderr, "Could not open display\n");
		goto out_destroy_chorder;
	}

	// Ensure we have XInput...
	int event, error;
	if (!XQueryExtension(state.dpy, "XInputExtension", &state.xi_opcode,
				&event, &error)) {
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

	// Ensure we have XTest
	if (!XTestQueryExtension(state.dpy, &event, &error, &major, &minor)) {
		ret = 1;
		fprintf(stderr, "Server does not support XTest\n");
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

	// Create windows for keyboard
	ret = create_windows(&state, default_btns,
			sizeof(default_btns) / sizeof(default_btns[0]));
	if (ret) {
		fprintf(stderr, "Failed to create windows\n");
		goto out_free_cmap;
	}

	state.gc = XCreateGC(state.dpy, state.win, 0, NULL);

	map_windows(&state);
	update_display(&state);

	ret = event_loop(&state);

	XFreeGC(state.dpy, state.gc);

	destroy_windows(&state);

out_free_cmap:
	XFreeColormap(state.dpy, state.cmap);

out_destroy_touch:
	destroy_touch_device(&state);

out_close:
	XCloseDisplay(state.dpy);

out_destroy_chorder:
	chorder_destroy(&state.chorder);
	return ret;
}
