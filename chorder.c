#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chorder.h"

int chorder_init(struct chorder *kbd, struct chordentry **entries, int maps,
		int entries_per_map)
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

	return 0;
}

void chorder_destroy(struct chorder *kbd)
{
	/* XXX Release keys */
	free(kbd->entries);
}