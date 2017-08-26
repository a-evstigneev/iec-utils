#!/bin/bash
libeventdir="/opt/iecd/source/libevent"
gcc server.c iecsock.c -o iecserver -I${libeventdir}/include -L/${libeventdir}/lib -levent -Wl,-rpath,${libeventdir}/lib  
gcc client.c iecsock.c xalloc.c -o iecclient -I${libeventdir}/include -L${libeventdir}/lib -levent -Wl,-rpath,${libeventdir}/lib 
