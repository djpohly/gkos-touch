BINS = gkos symname chorder_test
OBJS = gkos.o chorder.o chorder_test.o

CFLAGS = -g -Wall -Wextra -Werror -Wno-error=unused-parameter -Wno-error=unused-function
LDFLAGS = -g

all: $(BINS)

clean:
	$(RM) $(BINS) $(OBJS)

gkos: gkos.o chorder.o -lX11 -lXi -lXtst -lm
gkos.o: gkos.h

chorder_test: chorder_test.o chorder.o
chorder_test.o: chorder.h

chorder.o: chorder.h

symname: -lX11
