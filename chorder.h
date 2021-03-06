#ifndef CHORDER_H_
#define CHORDER_H_

#include <X11/Xlib.h>

typedef void (*chorder_handler_t)(void *arg, unsigned long code, int press);

// Types of actions that can be assigned to a chord
enum chord_type {
	// Unassigned
	TYPE_NONE,
	// Presses a key
	TYPE_KEY,
	// Presses a modifier and holds it until after the next CHORD_KEY
	TYPE_MOD,
	// Presses a modifier and holds it until pressed again
	TYPE_MODLOCK,
	// Selects the keymap to use for the next chord
	TYPE_MAP,
	// Selects the keymap to use until another is explicitly selected
	TYPE_MAPLOCK,
	// Executes a sequence of chord entries
	TYPE_MACRO,
};

// Single entry in a keymap
struct chord_entry {
	enum chord_type type;
	union {
		unsigned long code;
		unsigned int map;
		void *ptr;
	} arg;
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

	// Additional mods being pressed/locked by a macro
	struct mod_stack *macromods;
	struct mod_stack *macrolocks;

	// Function to call when a key is pressed
	chorder_handler_t press;
	// Opaque pointer passed to the press handler
	void *arg;

	// Flags
	unsigned int maplock : 1;
};

int chorder_init(struct chorder *kbd, const struct chord_entry *map,
		unsigned long maps, unsigned long entries_per_map,
		chorder_handler_t handle, void *arg);
void chorder_destroy(struct chorder *kbd);

struct chord_entry *chorder_get_entry(const struct chorder *kbd,
		unsigned long map, unsigned long entry);

int chorder_press(struct chorder *kbd, unsigned long entry);

#endif
