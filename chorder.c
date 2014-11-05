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
 * Determines whether a modifier is on a mod stack
 */
static int hasmod(struct mod_stack *mods, unsigned long code)
{
	for (/* mods */; mods; mods = mods->next)
		if (mods->code == code)
			return 1;
	return 0;
}

/*
 * Removes the given modifier from a mod stack, returning 0 if successful and
 * nonzero otherwise
 */
static int removemod(struct mod_stack **mods, unsigned long code)
{
	struct mod_stack *node = *mods;
	while (node) {
		if (node->code == code) {
			*mods = node->next;
			free(node);
			return 0;
		}
		mods = &(*mods)->next;
		node = *mods;
	}
	return 1;
}

/*
 * Initializes a chorder
 */
int chorder_init(struct chorder *kbd, struct chord_entry **entries,
		unsigned long maps, unsigned long entries_per_map,
		chorder_handler_t press, void *arg)
{
	kbd->entries = malloc(maps * entries_per_map * sizeof(**entries));
	if (!kbd->entries) {
		perror("malloc");
		return 1;
	}

	unsigned long i;
	for (i = 0; i < maps; i++)
		memcpy(kbd->entries + i * entries_per_map, entries + i,
				entries_per_map * sizeof(**entries));

	kbd->maps = maps;
	kbd->entries_per_map = entries_per_map;
	kbd->current_map = 0;
	kbd->mods = NULL;
	kbd->lockmods = NULL;
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
	// Release any pressed mods
	unsigned long code = popmod(&kbd->mods);
	while (code) {
		kbd->press(kbd->arg, code, 0);
		code = popmod(&kbd->mods);
	}

	// Release any locked mods
	code = popmod(&kbd->lockmods);
	while (code) {
		kbd->press(kbd->arg, code, 0);
		code = popmod(&kbd->lockmods);
	}
	
	free(kbd->entries);
}

/*
 * Gets the given entry from a chorder
 */
struct chord_entry *chorder_get_entry(struct chorder *kbd,
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
int chorder_press(struct chorder *kbd, unsigned long entry)
{
	int rv;

	struct chord_entry *e = chorder_get_entry(kbd, kbd->current_map, entry);
	switch (e->type) {
		case TYPE_NONE:
			fprintf(stderr, "not mapped\n");
			break;
		case TYPE_KEY:
			// Until there's a nice way to handle it, holding
			// regular keys is not supported
			rv = kbd->press(kbd->arg, e->val, 1);
			if (rv)
				return rv;

			rv = kbd->press(kbd->arg, e->val, 0);
			if (rv)
				return rv;

			// Release any pressed mods
			unsigned long code = popmod(&kbd->mods);
			while (code) {
				rv = kbd->press(kbd->arg, code, 0);
				if (rv)
					return rv;
				code = popmod(&kbd->mods);
			}

			//if (!kbd->maplock)
			//	kbd->current_map = 0;
			break;
		case TYPE_MOD:
			// If the mod is locked, unpress it
			if (!removemod(&kbd->lockmods, e->val)) {
				rv = kbd->press(kbd->arg, e->val, 0);
				if (rv)
					return rv;
				break;
			}

			// Otherwise, if it's pressed, lock it down
			if (!removemod(&kbd->mods, e->val)) {
				rv = pushmod(&kbd->lockmods, e->val);
				if (rv)
					return rv;
				break;
			}

			// Otherwise it's not pressed, so press it
			rv = pushmod(&kbd->mods, e->val);
			if (rv)
				return rv;
			rv = kbd->press(kbd->arg, e->val, 1);
			if (rv)
				return rv;
			break;
		case TYPE_MAP:
			fprintf(stderr, "map not implemented\n");
			//kbd->current_map = e->val;
			//kbd->maplock = 0;
			break;
		case TYPE_LOCK:
			fprintf(stderr, "lock not implemented\n");
			//kbd->current_map = e->val;
			//kbd->maplock = 1;
			break;
		case TYPE_MACRO:
			fprintf(stderr, "macro not implemented\n");
			break;
	}
	return 0;
}
