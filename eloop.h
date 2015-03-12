#ifndef ELOOP_H
#define ELOOP_H
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "list.h"

/*socket 可用时处理函数*/
typedef void (*proc_t)(void* data);

/*a list element never delete by others*/
typedef struct element
{
    struct list_head list;
    void *ptr;
}element_t;

/*fd节点*/
typedef struct node
{
    int fd;
    proc_t proc;
    void *data;
}node_t;

/*定时器*/
typedef struct timer
{
    int secs;
    proc_t proc;
    void *data;
    int left;
}ttimer_t;

typedef struct eloop
{
    fd_set readfd;
    struct timeval timeout;
    struct list_head fds;
    struct list_head timers;
    int max_fd;
    int looping;
}eloop_t;

void eloop_init(eloop_t* el);
void eloop_add_node(eloop_t* el,node_t *node);
void eloop_del_node(eloop_t* el,node_t *node);
void eloop_add_timer(eloop_t* el,ttimer_t *timer);
void eloop_modify_timer(eloop_t* el,ttimer_t *timer);
void eloop_del_timer(eloop_t* el,ttimer_t *timer);
void eloop_run(eloop_t* el);
void eloop_cancel(eloop_t* el);

#endif // ELOOP_H
