#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chorder.h"

int chorder_init(struct chorder *kbd, struct chordentry **map, int maps,
		int entries_per_map)
{
	int i;

	kbd->map = malloc(maps * entries_per_map * sizeof(**map));
	if (!kbd->map) {
		perror("malloc");
		return 1;
	}

	for (i = 0; i < maps; i++)
		memcpy(kbd->map + i * entries_per_map, map + i,
				entries_per_map * sizeof(**map));

	kbd->maps = maps;
	kbd->entries_per_map = entries_per_map;

	return 0;
}

void chorder_destroy(struct chorder *kbd)
{
	free(kbd->map);
}
