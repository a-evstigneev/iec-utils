#ifndef _XALLOC_H
#define _XALLOC_H

#include <stddef.h>

void *xmalloc(size_t size);
void xfree(void *ptr);

#endif
