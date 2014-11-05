BINS = gkos symname
OBJS = gkos.o chorder.o

CFLAGS = -Wall -Wextra -Werror -Wno-error=unused-parameter -Wno-error=unused-function

all: $(BINS)

clean:
	$(RM) $(BINS) $(OBJS)

gkos: gkos.o chorder.o -lX11 -lXi

gkos.o: gkos.h
chorder.o: chorder.h

symname: -lX11
