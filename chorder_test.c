#include <stdio.h>
#include <assert.h>

#include "chorder.h"

void mypress(void *unused, unsigned long code, int press)
{
	(void) unused;
	printf("%c %s\n", (char) code, press ? "pressed" : "released");
}

struct chord_entry hello[] = {
	{.type = TYPE_KEY, .arg.code = 'h'},
	{.type = TYPE_KEY, .arg.code = 'e'},
	{.type = TYPE_KEY, .arg.code = 'l'},
	{.type = TYPE_MOD, .arg.code = 'l'},
	{.type = TYPE_KEY, .arg.code = 'o'},
	{.type = TYPE_NONE},
};

int main()
{
	int rv;
	struct chord_entry map[2][28];
	int i;
	map[0][0] = (struct chord_entry) {.type = TYPE_NONE, .arg.code = 0};
	map[0][26] = (struct chord_entry) {.type = TYPE_MAPLOCK, .arg.map = 1};
	map[0][27] = (struct chord_entry) {.type = TYPE_MACRO, .arg.ptr = hello};
	for (i = 1; i < 26; i++)
		map[0][i] = (struct chord_entry) {.type = TYPE_KEY, .arg.code = '`' + i};
	map[0][5].type = TYPE_MOD;

	map[1][0] = (struct chord_entry) {.type = TYPE_MAP, .arg.map = 0};
	for (i = 1; i < 26; i++)
		map[1][i] = (struct chord_entry) {.type = TYPE_KEY, .arg.code = '@' + i};
	map[1][5].type = TYPE_MODLOCK;
	map[1][26] = (struct chord_entry) {.type = TYPE_MAP, .arg.map = 1};
	map[1][27] = (struct chord_entry) {.type = TYPE_MACRO, .arg.ptr = hello};

	struct chorder kbd;
	rv = chorder_init(&kbd, &map[0][0], 2, 28, mypress, NULL);
	assert(!rv);
	chorder_press(&kbd, 27);
	chorder_press(&kbd, 26);
	chorder_press(&kbd, 4);
	chorder_press(&kbd, 5);
	chorder_press(&kbd, 22);
	chorder_press(&kbd, 26);
	chorder_press(&kbd, 9);
	chorder_press(&kbd, 14);
	chorder_destroy(&kbd);
	return 0;
}
