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
	struct chord_entry map[2][29];
	int i;
	map[0][0] = (struct chord_entry) {.type = TYPE_NONE, .arg.code = 0};
	map[0][26] = (struct chord_entry) {.type = TYPE_MAPLOCK, .arg.map = 1};
	map[0][27] = (struct chord_entry) {.type = TYPE_MACRO, .arg.ptr = hello};
	for (i = 1; i < 26; i++)
		map[0][i] = (struct chord_entry) {.type = TYPE_KEY, .arg.code = '`' + i};
	map[0][5].type = TYPE_MOD;
	map[0][28] = (struct chord_entry) {.type = TYPE_MODLOCK, .arg.code = 'e'};

	map[1][0] = (struct chord_entry) {.type = TYPE_MAP, .arg.map = 0};
	for (i = 1; i < 26; i++)
		map[1][i] = (struct chord_entry) {.type = TYPE_KEY, .arg.code = '@' + i};
	map[1][5].type = TYPE_MOD;
	map[1][26] = (struct chord_entry) {.type = TYPE_MAP, .arg.map = 1};
	map[1][27] = (struct chord_entry) {.type = TYPE_MACRO, .arg.ptr = hello};
	map[1][28] = (struct chord_entry) {.type = TYPE_MODLOCK, .arg.code = 'E'};

	struct chorder kbd;
	rv = chorder_init(&kbd, &map[0][0],
			sizeof(map)/sizeof(map[0]),
			sizeof(map[0])/sizeof(map[0][0]),
			mypress, NULL);
	assert(!rv);
	// (map 0) macro: h e l MOD(l) o
	chorder_press(&kbd, 27);
	// (map 0) maplock: 1
	chorder_press(&kbd, 26);
	// (map 1) key: D
	chorder_press(&kbd, 4);
	// (map 1) mod: E
	chorder_press(&kbd, 5);
	// (map 1) modlock: E
	chorder_press(&kbd, 28);
	// (map 1) key: V
	chorder_press(&kbd, 22);
	// (map 1) map: 1 (lock released)
	chorder_press(&kbd, 26);
	// (map 0) key: i
	chorder_press(&kbd, 9);
	// (map 0) key: n
	chorder_press(&kbd, 14);
	// releases locked mod E
	chorder_destroy(&kbd);
	return 0;
}
