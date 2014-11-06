#include <stdio.h>
#include <assert.h>

#include "chorder.h"

void mypress(void *unused, unsigned long code, int press)
{
	(void) unused;
	printf("%c %s\n", (char) code, press ? "pressed" : "released");
}

int main()
{
	int rv;
	struct chord_entry map[1][26];
	int i;
	map[0][0] = (struct chord_entry) {.type = TYPE_NONE, .val = 0};
	for (i = 1; i < 26; i++)
		map[0][i] = (struct chord_entry) {.type = TYPE_KEY, .val = '@' + i};
	map[0][4].type = TYPE_MOD;
	map[0][5].type = TYPE_MOD;

	struct chorder kbd;
	rv = chorder_init(&kbd, &map[0][0], 1, 26, mypress, NULL);
	assert(!rv);
	rv = chorder_press(&kbd, 4);
	rv = chorder_press(&kbd, 5);
	rv = chorder_press(&kbd, 5);
	rv = chorder_press(&kbd, 22);
	rv = chorder_press(&kbd, 9);
	rv = chorder_press(&kbd, 14);
	chorder_destroy(&kbd);
	return 0;
}
