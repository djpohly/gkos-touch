#ifndef GKOS_H_
#define GKOS_H_

#include <inttypes.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "chorder.h"

#define GRID_X 130
#define GRID_Y 70
#define TOP_Y 400

#define IR 160
#define DR 80
#define DTH (14 * 64)
#define CX 0
#define CY 700

#define TRANSPARENT 0
#define PRESSED_COLOR 0xd0888a85
#define UNPRESSED_COLOR 0xd0204a87
#define BORDER_COLOR 0xffeeeeec

/*
 * Window corresponding to button
 */
struct layout_win {
	int r1, r2;
	// In 1/64ths of degrees (i.e. # degrees * 64)
	int th, dth;
	unsigned int cx, cy;
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
	int *touchids;
	struct chorder chorder;
	unsigned int active : 1;
	unsigned int shutdown : 1;
};


/*
 * Represents one button in a given layout
 */
struct layout_btn {
	uint8_t row;
	uint8_t th, dth;
	uint8_t bits;
};

/*
 * Default button layout
 */
static const struct layout_btn default_btns[] = {
	{1, 0, 1, 4},
	{1, 1, 1, 6},
	{1, 2, 1, 2},
	{1, 3, 1, 3},
	{1, 4, 1, 1},

	{0, 0, 3, 7},
	{0, 3, 2, 5},
};

#endif
