#ifndef GKOS_H_
#define GKOS_H_

#include <inttypes.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "chorder.h"

#define GRID_X 130
#define GRID_Y 70
#define TOP_Y 400

#define PRESSED_COLOR 0xd0888a85
#define UNPRESSED_COLOR 0xd0204a87
#define BORDER_COLOR 0xffeeeeec

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
};

/*
 * Main application state structure
 */
struct kbd_state {
	Display *dpy;
	XVisualInfo xvi;
	Colormap cmap;
	Window win;
	GC gc;
	int xi_opcode;
	int input_dev;
	int nwins;
	int ntouches;
	struct layout_win *wins;
	struct layout_win **touches;
	struct chorder chorder;
	unsigned int active : 1;
	unsigned int shutdown : 1;
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

	{-2, 1, 1, 40},
	{-2, 2, 3, 56},

	{-1, 0, 1, 8},
	{-1, 1, 1, 24},
	{-1, 2, 1, 16},
	{-1, 3, 1, 48},
	{-1, 4, 1, 32},
};

#endif
