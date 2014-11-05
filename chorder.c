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
	/* XXX Release keys */
	free(kbd->entries);
}
