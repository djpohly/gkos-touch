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
 * Searches the input hierarchy for a direct-touch device (e.g. a touchscreen,
 * but not most touchpads).  The id parameter gives either a specific device ID
 * to check or one of the special values XIAllDevices or XIAllMasterDevices.
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

	// Find the first direct-touch device
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

	// Allocate space for keeping track of currently held touches and the
	// corresponding XInput touch event IDs
	state->touches = calloc(state->ntouches, sizeof(state->touches[0]));
	state->touchids = calloc(state->ntouches, sizeof(state->touchids[0]));

	if (!state->touches || !state->touchids) {
		fprintf(stderr, "Failed to allocate touches/IDs\n");
		free(state->touchids);
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
 * Establishes an active grab on the touch device
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
 * Releases grab for the touch device
 */
void ungrab_touches(struct kbd_state *state)
{
	XIUngrabDevice(state->dpy, state->input_dev, CurrentTime);
}

/*
 * Returns the button structure, if any, at the given coordinates
 */
struct layout_btn *get_layout_btn(struct kbd_state *state, double x, double y)
{
	int i;
	for (i = 0; i < state->nbtns; i++) {
		double r = sqrt(pow(x - state->btns[i].cx, 2) + pow(state->btns[i].cy - y, 2));
		int th = 64 * 180 * atan2(state->btns[i].cy - y, x - state->btns[i].cx) / M_PI;
		if (r >= state->btns[i].r1 && r <= state->btns[i].r2 &&
				th >= state->btns[i].th &&
				th <= state->btns[i].th + state->btns[i].dth)
			return &state->btns[i];
	}
	return NULL;
}

/*
 * Creates the main window for the GKOS keyboard
 */
int create_window(struct kbd_state *state, const struct layout *lt,
		int num_btns)
{
	int i;

	// Allocate space for keys on both sides
	state->btns = calloc(num_btns * 2, sizeof(state->btns[0]));
	if (!state->btns)
		return 1;
	state->nbtns = num_btns * 2;

	// Set up the class hint for the GKOS window
	XClassHint *class = XAllocClassHint();
	if (!class) {
		fprintf(stderr, "Failed to allocate class hint\n");
		free(state->btns);
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

	// Calculate button positions in grid
	for (i = 0; i < state->nbtns / 2; i++) {
		// Index of mirrored key
		int m = i + state->nbtns / 2;

		state->btns[i].r1 = state->btns[m].r1 = IR + lt[i].row * DR;
		state->btns[i].r2 = state->btns[m].r2 = state->btns[i].r1 + DR;
		state->btns[i].th = 5760 - (lt[i].th + lt[i].dth) * DTH;
		state->btns[m].th = 5760 + lt[i].th * DTH;
		state->btns[i].dth = state->btns[m].dth = lt[i].dth * DTH;
		state->btns[i].cx = CX;
		state->btns[m].cx = swidth - 1 - CX;
		state->btns[i].cy = state->btns[m].cy = CY;
		state->btns[i].bits = lt[i].bits;
		state->btns[m].bits = lt[i].bits << 3;
	}

	// Grab touch events for the new window
	if (grab_touches(state)) {
		fprintf(stderr, "Failed to grab touch event\n");
		free(state->btns);
		return 1;
	}

	return 0;
}

/*
 * Map the main window and wait for confirmation
 */
void map_window(struct kbd_state *state)
{
	// Map everything and wait for the notify event
	XMapWindow(state->dpy, state->win);

	XEvent ev;
	while (XMaskEvent(state->dpy, StructureNotifyMask, &ev) == Success &&
			(ev.type != MapNotify || ev.xmap.event != state->win))
		;
}

/*
 * Tear down everything created in create_window
 */
void destroy_window(struct kbd_state *state)
{
	free(state->btns);

	ungrab_touches(state);
	XDestroyWindow(state->dpy, state->win);
}

/*
 * Calculate the bits corresponding to the currently touched buttons
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
 * Turn on/off a button's highlight
 */
void highlight_win(struct kbd_state *state, struct layout_btn *btn, int on)
{
	// Fill
	XSetForeground(state->dpy, state->gc,
			on ? PRESSED_COLOR : UNPRESSED_COLOR);
	XFillArc(state->dpy, state->win, state->gc,
			btn->cx - btn->r2, btn->cy - btn->r2,
			2*btn->r2, 2*btn->r2,
			btn->th, btn->dth);
	XSetForeground(state->dpy, state->gc, TRANSPARENT);
	XFillArc(state->dpy, state->win, state->gc,
			btn->cx - btn->r1, btn->cy - btn->r1,
			2*btn->r1, 2*btn->r1,
			btn->th, btn->dth);

	// Border
	XSetForeground(state->dpy, state->gc, BORDER_COLOR);
	XArc arcs[] = {
		{
			.x = btn->cx - btn->r2, .y = btn->cy - btn->r2,
			.width = 2*btn->r2, .height = 2*btn->r2,
			.angle1 = btn->th, .angle2 = btn->dth,
		},
		{
			.x = btn->cx - btn->r1, .y = btn->cy - btn->r1,
			.width = 2*btn->r1, .height = 2*btn->r1,
			.angle1 = btn->th, .angle2 = btn->dth,
		},
	};
	XDrawArcs(state->dpy, state->win, state->gc,
			arcs, sizeof(arcs)/sizeof(arcs[0]));
	XSegment segs[] = {
		{
			.x1 = btn->cx + btn->r2 * cos(M_PI * btn->th / 11520.0),
			.x2 = btn->cx + btn->r1 * cos(M_PI * btn->th / 11520.0),
			.y1 = btn->cy - btn->r2 * sin(M_PI * btn->th / 11520.0),
			.y2 = btn->cy - btn->r1 * sin(M_PI * btn->th / 11520.0),
		},
		{
			.x1 = btn->cx + btn->r2 * cos(M_PI * (btn->th+btn->dth) / 11520.0),
			.x2 = btn->cx + btn->r1 * cos(M_PI * (btn->th+btn->dth) / 11520.0),
			.y1 = btn->cy - btn->r2 * sin(M_PI * (btn->th+btn->dth) / 11520.0),
			.y2 = btn->cy - btn->r1 * sin(M_PI * (btn->th+btn->dth) / 11520.0),
		},
	};
	XDrawSegments(state->dpy, state->win, state->gc,
			segs, sizeof(segs)/sizeof(segs[0]));
}

/*
 * Highlight buttons according to what is currently pressed
 */
void update_display(struct kbd_state *state)
{
	uint8_t bits = get_pressed_bits(state);
	int i;
	for (i = 0; i < state->nbtns; i++) {
		int on = (bits & state->btns[i].bits) == state->btns[i].bits;
		highlight_win(state, &state->btns[i], on);
	}
}

/*
 * Remember a button as "touched" at the start of a touch event
 */
int add_touch(struct kbd_state *state, struct layout_btn *btn, int touchid)
{
	int i;
	for (i = 0; i < state->ntouches && state->touchids[i]; i++)
		;
	if (i >= state->ntouches) {
		fprintf(stderr, "No open touch slots found\n");
		return 1;
	}

	state->touches[i] = btn;
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
 * Unregister a touched button when the touch is released
 */
int remove_touch(struct kbd_state *state, int index)
{
	state->touches[index] = NULL;
	state->touchids[index] = 0;
	update_display(state);
	return 0;
}

/*
 * Keypress implementation to pass to chorder object
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
	struct layout_btn *btn;
	int idx;

	switch (ev->evtype) {
		case XI_TouchBegin:
			// Claim the touch event
			XIAllowTouchEvents(state->dpy, state->input_dev,
					ev->detail, ev->event, XIAcceptTouch);

			// Find and record which button was touched
			btn = get_layout_btn(state, ev->root_x, ev->root_y);
			if (add_touch(state, btn, ev->detail))
				return 1;

			// Comes after add_touch so we remember which touches
			// were outside a defined button
			if (!btn)
				break;

			state->active = 1;
			break;

		case XI_TouchEnd:
			// Find which touch was released
			idx = get_touch_index(state, ev->detail);
			if (idx < 0) {
				fprintf(stderr, "Released window was not touched\n");
				return 1;
			}

			// Shut down on double-touch outside keyboard
			int misses = 0;
			int i;
			for (i = 0; i < state->ntouches; i++) {
				if (state->touchids[i] && !state->touches[i])
					misses++;
				if (misses >= 2)
					break;
			}
			if (misses >= 2) {
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

		case XI_TouchUpdate:
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

	// Create main window and keyboard buttons
	ret = create_window(&state, default_btns,
			sizeof(default_btns) / sizeof(default_btns[0]));
	if (ret) {
		fprintf(stderr, "Failed to create windows\n");
		goto out_free_cmap;
	}

	// Create a GC to use
	state.gc = XCreateGC(state.dpy, state.win, 0, NULL);

	// Display the window
	map_window(&state);
	update_display(&state);

	ret = event_loop(&state);

	// Clean everything up
	XFreeGC(state.dpy, state.gc);
	destroy_window(&state);
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
