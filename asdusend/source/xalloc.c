#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>

#include "xalloc.h"

void *
xmalloc(size_t size)
{
	void *p;
	p =  malloc(size); 
	if (p == NULL) {
		perror("xmalloc");
		exit(EXIT_FAILURE);
	}
	return p;
}


void 
xfree(void *ptr)
{
	free(ptr);
}
