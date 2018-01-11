bindir = ../bin
target = quemngr
src = heapfunc.c logging.c quecontrol.c quemngr.c utils.c xalloc.c
headers = heapfunc.h logging.h quecontrol.h utils.h xalloc.h

.PHONY: all $(target)

all: $(target)

$(target): $(src) $(headers)
	gcc -o $(bindir)/$(target) -D DEBON $(src)
