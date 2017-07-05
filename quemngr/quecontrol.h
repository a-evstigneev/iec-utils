#ifndef ARRAY_H
#define ARRAY_H

#define DESCMAX 255
#define BUFMAX 4096

#define CURDIR "."
#define PARENTDIR ".."
#define FILEMODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)

struct msg {
	ino_t inode;
	struct timeval modtime;
	int	state;
};

struct msglist {
	struct msg **msg;
	int nmax;
	int total;
	int conf;
	int send;
	char *dir;
	int timeout;
};

struct msg *elem_new(ino_t inode, struct timeval modtime, int state);
int	elem_add(struct msglist *list, struct msg *elem);
int elem_find(struct msglist *list, ino_t inode);
int elem_del(struct msglist *list, ino_t inode);

int queue_add(void); 
int queue_restore(struct msglist *list, int index);
int queue_comp(const void *queue1, const void *queue2);
void queue_sort(struct msglist *list);

#endif
