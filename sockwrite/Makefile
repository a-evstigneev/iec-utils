bindir = ../bin
target = sockwrite
src = sockwrite.c logging.c
headers = logging.h
CFLAGS="-Wall"

.PHONY: all $(target)

all: $(target)

$(target): $(src) $(headers)
	gcc -o $(bindir)/$(target) $(CFLAGS) -D DEBON $(src)
