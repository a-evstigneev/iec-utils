bindir = ../bin
target = ieclink
libeventdir = "./libevent"

src = ieclink.c iecsock.c xalloc.c
headers = iec104.h iec104_types.h iecsock.h iecsock_internal.h xalloc.h

.PHONY: all $(target)

all: $(target)

$(target): $(src) $(headers)
	gcc -o $(bindir)/$(target) $(src) -I$(libeventdir)/include -L$(libeventdir)/lib -levent -Wl,-rpath,$(libeventdir)/lib 
