#ifndef CHORDER_H_
#define CHORDER_H_

#include <X11/Xlib.h>

typedef int (*chorder_handler_t)(void *arg, unsigned long code, int press);

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

// Stack data structure for mod keys
struct mod_stack {
	unsigned long code;
	struct mod_stack *next;
};

struct chorder {
	// Entries defining the keymap
	struct chord_entry *entries;
	// Number of keymaps and entries per map
	unsigned long maps;
	unsigned long entries_per_map;

	// Currently selected keymap
	unsigned long current_map;

	// Mods currently pressed and/or locked
	struct mod_stack *mods;
	struct mod_stack *lockmods;

	// Function to call when a key is pressed
	chorder_handler_t press;
	// Opaque pointer passed to the press handler
	void *arg;

	// Flags
	unsigned int maplock : 1;
};

int chorder_init(struct chorder *kbd, struct chord_entry **map,
		unsigned long maps, unsigned long entries_per_map,
		chorder_handler_t handle, void *arg);
void chorder_destroy(struct chorder *kbd);

#endif
