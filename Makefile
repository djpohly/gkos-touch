BINS = gkos

CFLAGS = -Wall -Wextra -Werror -Wno-error=unused-parameter

all: $(BINS)

clean:
	$(RM) $(BINS)

gkos: -lX11 -lXi -lfakekey
