#include <stdio.h>
#include <X11/Xlib.h>

int main(int argc, char *argv[])
{
	if (argc < 2)
		return 1;

	KeySym sym = XStringToKeysym(argv[1]);
	if (sym == NoSymbol)
		return 1;

	printf("XK_%s", XKeysymToString(sym));

	return 0;
}
