#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>

#define GRID_X 110
#define GRID_Y 70
#define TOP_Y 400


/*
 * Represents one button in a given layout
 */
struct layout_btn {
	int8_t x, y;
	uint8_t h;
	uint8_t bits;
};

/*
 * Window corresponding to button
 */
struct layout_win {
	Window win;
	uint8_t bits;
	unsigned mapped : 1;
};

/*
 * Default button layout (modeled after Android GKOS app)
 */
static const struct layout_btn default_btns[] = {
	{0, 0, 1, 1},
	{0, 1, 1, 3},
	{0, 2, 1, 2},
	{0, 3, 1, 6},
	{0, 4, 1, 4},

	{1, 1, 1, 5},
	{1, 2, 3, 7},

	{-2, 1, 1, 24},
	{-2, 2, 3, 56},

	{-1, 0, 1, 8},
	{-1, 1, 1, 24},
	{-1, 2, 1, 16},
	{-1, 3, 1, 48},
	{-1, 4, 1, 32},
};

struct kbd_state {
	Display *dpy;
	int xi_opcode;
	int input_dev;
	int nwins;
	int ntouches;
	struct layout_win *wins;
	struct layout_win **touches;
};


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

void destroy_touch_device(struct kbd_state *state)
{
	free(state->touches);
}

struct layout_win *get_layout_win(struct kbd_state *state, Window win)
{
	int i;
	for (i = 0; i < state->nwins; i++)
		if (state->wins[i].win == win)
			return &state->wins[i];
	return NULL;
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

	// Create and map the windows
	Screen *scr = DefaultScreenOfDisplay(state->dpy);
	int swidth = WidthOfScreen(scr);
	XSetWindowAttributes attrs = {
		.background_pixel = BlackPixelOfScreen(scr),
		.border_pixel = WhitePixelOfScreen(scr),
		.override_redirect = True,
	};
	for (i = 0; i < state->nwins; i++) {
		int x = btns[i].x * GRID_X;
		if (x < 0)
			x += swidth;
		int y = btns[i].y * GRID_Y + TOP_Y;
		int h = btns[i].h * GRID_Y;
		state->wins[i].win = XCreateWindow(state->dpy,
				DefaultRootWindow(state->dpy), x, y, GRID_X, h,
				1, CopyFromParent, InputOutput, CopyFromParent,
				CWBackPixel | CWBorderPixel | CWOverrideRedirect,
				&attrs);
		state->wins[i].mapped = 0;
		state->wins[i].bits = btns[i].bits;
		XSelectInput(state->dpy, state->wins[i].win, StructureNotifyMask);
		XMapWindow(state->dpy, state->wins[i].win);
	}

	// Wait for windows to map
	XEvent ev;
	while (i > 0 && XNextEvent(state->dpy, &ev) == Success) {
		if (ev.type != MapNotify) {
			fprintf(stderr, "unexpected event, type %d\n", ev.type);
			return 1;
		}

		struct layout_win *win = get_layout_win(state, ev.xmap.event);
		if (!win) {
			fprintf(stderr, "um, what window?\n");
			return 1;
		}
		if (win->mapped) {
			fprintf(stderr, "mapped again?\n");
			return 1;
		}
		win->mapped = 1;

		// Grab touch events for the new window
		if (grab_touches(state, ev.xmap.event)) {
			fprintf(stderr, "Failed to grab touch event\n");
			return 1;
		}

		i--;
	}
	return 0;
}

void destroy_windows(struct kbd_state *state)
{
	int i;
	for (i = 0; i < state->nwins; i++) {
		ungrab_touches(state, state->wins[i].win);
		XDestroyWindow(state->dpy, state->wins[i].win);
	}
	free(state->wins);
}

int handle_xi_event(struct kbd_state *state, XIDeviceEvent *ev,
		int *touches, uint8_t *bits)
{
	struct layout_win *win;
	switch (ev->evtype) {
		case XI_TouchBegin:
			win = get_layout_win(state, ev->event);
			*bits |= win->bits;
			fprintf(stderr, "bits: %u\n", *bits);
			(*touches)++;
			break;
		case XI_TouchUpdate:
			//fprintf(stderr, "touch update\n");
			break;
		case XI_TouchEnd:
			win = get_layout_win(state, ev->event);
			*bits &= ~win->bits;
			(*touches)--;
			fprintf(stderr, "bits: %u\n", *bits);
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

	int touches = 0;
	uint8_t bits = 0;
	while (XNextEvent(state->dpy, &ev) == Success) {
		if (ev.type == GenericEvent &&
				cookie->extension == state->xi_opcode &&
				XGetEventData(state->dpy, cookie)) {
			// GenericEvent from XInput
			handle_xi_event(state, cookie->data, &touches, &bits);
			XFreeEventData(state->dpy, cookie);
		} else {
			// Regular event type
			fprintf(stderr, "regular event %d\n", ev.type);
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

	// Create and map windows for keyboard
	ret = create_windows(&state, default_btns,
			sizeof(default_btns) / sizeof(default_btns[0]));
	if (ret) {
		fprintf(stderr, "Failed to create windows\n");
		goto out_destroy_touch;
	}

	ret = event_loop(&state);

	destroy_windows(&state);

out_destroy_touch:
	destroy_touch_device(&state);

out_close:
	XCloseDisplay(state.dpy);
	return ret;
}
