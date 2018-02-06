#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <sys/time.h>
#include <assert.h>

#include "heapfunc.h"
#include "quecontrol.h"
#include "logging.h"
#include "xalloc.h"

struct heap *
heap_create(int maxsize)
{
	struct heap *h;
	
	h = xmalloc(sizeof(*h));
	if (h != NULL) {
		h->maxsize = maxsize;
		h->nodecount = 0;
		h->nodes = xmalloc(sizeof(*h->nodes) * (maxsize + 1));
		if (h->nodes == NULL) {
			xfree(h);
			return NULL;
		}
	}
	return h;
}

static void
heapnode_swap(struct heapnode *a, struct heapnode *b)
{
	struct heapnode temp;

	temp = *a;
	*a = *b;
	*b = temp;
}

struct heapnode *
heap_min(struct heap *h)
{
	if (h->nodecount == 0)
		return NULL;
	
	return &h->nodes[1];
}

int
heap_insert(struct heap *h, struct timeval key, int indque, int inode)
{
	int i;

	if (h->nodecount + 1 >= h->maxsize) {
		h->maxsize *= 2;
		h->nodes = xresize(h->nodes, h->maxsize * sizeof(*h->nodes));
	}
	
	h->nodecount++;
	h->nodes[h->nodecount].key = key;
	h->nodes[h->nodecount].indque = indque;
	h->nodes[h->nodecount].inode = inode;
	
	for (i = h->nodecount; i > 1 && timercmp(&h->nodes[i].key, &h->nodes[i/2].key, <) ; i = i/2) {
		heapnode_swap(&h->nodes[i], &h->nodes[i/2]);
	}
	
	return 0;
}

static void 
heap_heapify(struct heap *h, int index)
{
	for (;;) {
		int left = 2 * index;
		int right = 2 * index + 1;
		int least = index;
		
		if (left <= h->nodecount && timercmp(&h->nodes[left].key, &h->nodes[index].key, <)) 
			least = left; 
		if (right <= h->nodecount && timercmp(&h->nodes[right].key, &h->nodes[least].key, <)) 
			least = right;
		if (least == index)
			break;
		heapnode_swap(&h->nodes[index], &h->nodes[least]);
		index = least;
	}
}

struct heapnode 
heap_extract_min(struct heap *h)
{
	if (h->nodecount == 0)
		return (struct heapnode){0, 0};
	
	struct heapnode minnode = h->nodes[1];
	h->nodes[1] = h->nodes[h->nodecount];
	h->nodecount--;
	heap_heapify(h, 1);
	
	return minnode;
}
