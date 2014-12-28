#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chorder.h"

/*
 * Pushes a modifier onto a mod stack
 */
static int pushmod(struct mod_stack **mods, unsigned long code)
{
	struct mod_stack *tos = malloc(sizeof(*tos));
	if (!tos)
		return 1;

	tos->code = code;
	tos->next = *mods;
	*mods = tos;
	return 0;
}

/*
 * Pops the top modifier off a mod stack
 */
static unsigned long popmod(struct mod_stack **mods)
{
	if (!*mods)
		return 0;

	struct mod_stack *tos = *mods;
	unsigned long code = tos->code;
	*mods = tos->next;
	free(tos);
	return code;
}

/*
 * Removes the given modifier from a mod stack, returning 1 if found and 0
 * otherwise
 */
static int removemod(struct mod_stack **mods, unsigned long code)
{
	struct mod_stack *node = *mods;
	while (node) {
		if (node->code == code) {
			*mods = node->next;
			free(node);
			return 1;
		}
		mods = &(*mods)->next;
		node = *mods;
	}
	return 0;
}

/*
 * Initializes a chorder
 */
int chorder_init(struct chorder *kbd, const struct chord_entry *entries,
		unsigned long maps, unsigned long entries_per_map,
		chorder_handler_t press, void *arg)
{
	kbd->entries = malloc(maps * entries_per_map * sizeof(*entries));
	if (!kbd->entries) {
		perror("malloc");
		return 1;
	}

	memcpy(kbd->entries, entries, maps * entries_per_map * sizeof(*entries));

	kbd->maps = maps;
	kbd->entries_per_map = entries_per_map;
	kbd->current_map = 0;
	kbd->mods = NULL;
	kbd->lockmods = NULL;
	kbd->macromods = NULL;
	kbd->macrolocks = NULL;
	kbd->press = press;
	kbd->arg = arg;
	kbd->maplock = 0;

	return 0;
}

/*
 * Releases the resources allocated for a chorder
 */
void chorder_destroy(struct chorder *kbd)
{
	unsigned long code;

	// Release any pressed or locked mods from current macro
	while ((code = popmod(&kbd->macromods)))
		kbd->press(kbd->arg, code, 0);
	while ((code = popmod(&kbd->macrolocks)))
		kbd->press(kbd->arg, code, 0);

	// Release any pressed or locked mods
	while ((code = popmod(&kbd->mods)))
		kbd->press(kbd->arg, code, 0);
	while ((code = popmod(&kbd->lockmods)))
		kbd->press(kbd->arg, code, 0);
	
	free(kbd->entries);
}

/*
 * Gets the given entry from a chorder
 */
struct chord_entry *chorder_get_entry(const struct chorder *kbd,
		unsigned long map, unsigned long entry)
{
	// Bounds checking
	if (map >= kbd->maps || entry >= kbd->entries_per_map)
		return NULL;

	return kbd->entries + map * kbd->entries_per_map + entry;
}

/*
 * Handles a chord press on a chorder
 */
static int handle_entry(struct chorder *kbd, struct chord_entry *e, int in_macro)
{
	int rv;
	struct chord_entry *macro;

	switch (e->type) {
		case TYPE_NONE:
			fprintf(stderr, "chorder: not mapped\n");
			break;
		case TYPE_KEY:
			// Until there's a nice way to handle it, holding
			// regular keys is not supported
			kbd->press(kbd->arg, e->arg.code, 1);
			kbd->press(kbd->arg, e->arg.code, 0);

			// Release any pressed mods
			unsigned long code = popmod(&kbd->mods);
			while (code) {
				kbd->press(kbd->arg, code, 0);
				code = popmod(&kbd->mods);
			}
			break;
		case TYPE_MOD:
			// If the mod is locked, unpress it
			if (removemod(&kbd->lockmods, e->arg.code)) {
				kbd->press(kbd->arg, e->arg.code, 0);
				break;
			}

			// Otherwise, if it's pressed, lock it down
			if (removemod(&kbd->mods, e->arg.code)) {
				rv = pushmod(&kbd->lockmods, e->arg.code);
				if (rv)
					return rv;
				break;
			}

			// Otherwise it's not pressed, so press it
			rv = pushmod(&kbd->mods, e->arg.code);
			if (rv)
				return rv;
			kbd->press(kbd->arg, e->arg.code, 1);
			break;
		case TYPE_MODLOCK:
			// Straight to locked mod

			// If already locked, toggle it off and release
			if (removemod(&kbd->lockmods, e->arg.code)) {
				kbd->press(kbd->arg, e->arg.code, 0);
				break;
			}

			// Otherwise we're going to lock it...
			rv = pushmod(&kbd->lockmods, e->arg.code);
			if (rv)
				return rv;

			// and press it if it wasn't already pressed
			if (!removemod(&kbd->mods, e->arg.code))
				kbd->press(kbd->arg, e->arg.code, 1);
			break;
		case TYPE_MAP:
			// XXX Figure this out
			if (in_macro) {
				fprintf(stderr, "chorder: haven't thought about maps in macros yet\n");
				return 1;
			}
			if (kbd->current_map == e->arg.map) {
				// If we're already on this map...
				if (kbd->maplock) {
					// and it's locked, release
					kbd->current_map = 0;
					kbd->maplock = 0;
				} else {
					// and it's not locked, lock it
					kbd->maplock = 1;
				}
			} else {
				// Otherwise, switch to the map
				kbd->current_map = e->arg.map;
				kbd->maplock = 0;
			}
			break;
		case TYPE_MAPLOCK:
			// XXX Figure this out
			if (in_macro) {
				fprintf(stderr, "chorder: haven't thought about maplocks in macros yet\n");
				return 1;
			}
			// Straight to locked map
			kbd->current_map = e->arg.map;
			kbd->maplock = 1;
			break;
		case TYPE_MACRO:
			// Not allowed to nest macros
			if (in_macro) {
				fprintf(stderr, "chorder: nested macros are not supported\n");
				return 1;
			}
			// Handle each entry in turn
			for (macro = e->arg.ptr; macro->type != TYPE_NONE; macro++) {
				rv = handle_entry(kbd, macro, 1);
				if (rv)
					return rv;
			}
			break;
	}

	// Switch back to default map if it wasn't just set and isn't locked
	if (e->type != TYPE_MAP && e->type != TYPE_MAPLOCK && !kbd->maplock)
		kbd->current_map = 0;
	return 0;
}

int chorder_press(struct chorder *kbd, unsigned long entry)
{
	struct chord_entry *e = chorder_get_entry(kbd, kbd->current_map, entry);
	return handle_entry(kbd, e, 0);
}
