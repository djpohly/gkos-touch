BINS = gkos symname

CFLAGS = -Wall -Wextra -Werror -Wno-error=unused-parameter

all: $(BINS)

clean:
	$(RM) $(BINS)

gkos: gkos.h -lX11 -lXi

symname: -lX11
