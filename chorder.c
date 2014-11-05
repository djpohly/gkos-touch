#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chorder.h"

/*
 * Initializes a chorder
 */
int chorder_init(struct chorder *kbd, struct chord_entry **entries, int maps,
		int entries_per_map, chorder_handler_t press, void *arg)
{
	int i;

	kbd->entries = malloc(maps * entries_per_map * sizeof(**entries));
	if (!kbd->entries) {
		perror("malloc");
		return 1;
	}

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
