#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

#include "logging.h"

void *
xmalloc(size_t size)
{
	void *ptr;
	ptr = malloc(size); 
	if (ptr == NULL) {
		ERR_SYS("error xmalloc");
	}
	return ptr;
}

void *xresize(void *vp, size_t size)
{
	void *p;

	p = realloc(vp, size);
	if (p == NULL)
		ERR_SYS("error xresize");
	
	return p;
}

void 
xfree(void *ptr)
{
	free(ptr);
}
