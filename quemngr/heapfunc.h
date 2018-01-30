#ifndef HEAPFUNC_H
#define HEAPFUNC_H

struct heapnode {
	struct timeval key;
	int indq;
	int inode;
};

struct heap {
	int maxsize;
	int nodecount;
	struct heapnode *nodes;
};

struct heap	*heap_create(int maxsize);
struct heapnode *heap_min(struct heap *h);
int heap_insert(struct heap *h, struct timeval key, int indq, int inode);
struct heapnode heap_extract_min(struct heap *h);

#endif /*HEAPFUNC_H */
