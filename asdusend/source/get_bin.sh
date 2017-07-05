#!/bin/bash
libeventdir="/opt/iecd/libevent"
gcc server.c iecsock.c -o iecserver -I${libeventdir}/include -L/${libeventdir}/lib -levent -Wl,-rpath,${libeventdir}/lib  
gcc xalloc.c client.c iecsock.c -o iecclient -I${libeventdir}/include -L${libeventdir}/lib -levent -Wl,-rpath,${libeventdir}/lib 
