#ifndef CHORDER_H_
#define CHORDER_H_

#include <X11/Xlib.h>

enum chordtype {
	NONE,
	PRESS,
	MOD,
	MAP,
};

struct chordentry {
	enum chordtype type;
	union {
		KeySym sym;
		KeyCode code;
		int map;
	} val;
};

struct chorder {
	// Entries defining the keymap
	struct chord_entry **entries;
	// Number of keymaps and entries per map
	unsigned long maps;
	unsigned long entries_per_map;
};

int chorder_init(struct chorder *kbd, struct chordentry **map, int maps,
		int entries_per_map);
void chorder_destroy(struct chorder *kbd);

#endif
