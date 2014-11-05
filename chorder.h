#ifndef CHORDER_H_
#define CHORDER_H_

#include <X11/Xlib.h>

// Types of actions that can be assigned to a chord
enum chord_type {
	// Unassigned
	TYPE_NONE,
	// Presses a key
	TYPE_KEY,
	// Presses a key and holds it until after the next CHORD_KEY
	TYPE_MOD,
	// Selects the keymap which will be used for the next chord
	TYPE_MAP,
	// Selects the keymap which will be used until lock is released
	TYPE_LOCK,
	// Executes a sequence of chords
	TYPE_MACRO,
};

// Single entry in a keymap
struct chord_entry {
	enum chord_type type;
	unsigned long val;
};

struct chorder {
	// Entries defining the keymap
	struct chord_entry **entries;
	// Number of keymaps and entries per map
	unsigned long maps;
	unsigned long entries_per_map;
};

int chorder_init(struct chorder *kbd, struct chord_entry **map, int maps,
		int entries_per_map);
void chorder_destroy(struct chorder *kbd);

#endif
