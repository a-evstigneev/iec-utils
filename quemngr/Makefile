bindir = ../bin
target = quemngr
src = heapfunc.c logging.c quecontrol.c quemngr.c utils.c xalloc.c
headers = heapfunc.h logging.h quecontrol.h utils.h xalloc.h
CFLAGS="-Wall"

.PHONY: all $(target)

all: $(target)

$(target): $(src) $(headers)
	gcc -o $(bindir)/$(target) $(CFLAGS) -D DEBON $(src) 
