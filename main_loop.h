#ifndef __MAIN_LOOP__
#define __MAIN_LOOP__
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include "list.h"

typedef struct tag_event event_t;

typedef void (*callback_t)(event_t *evt);

//该结构体字段仅内部用，不要直接访问
struct tag_event
{
    int flag;
    int fd;//fd
    int secs;//seconds
    int usecs;//useconds
    struct timeval timeout;//expire use
    callback_t proc;//callback function
    void *arg;
    void *ptr;
};

void ml_init();

void ml_init_read_event(event_t *evt,int fd,callback_t fn,void *arg);

void ml_init_write_event(event_t *evt,int fd,callback_t fn,void *arg);

void ml_init_timer_event(event_t *evt,int secs,int usecs,callback_t fn,void *arg);

void ml_mod_timer_event(event_t *evt,int secs,int usecs);

void* ml_get_event_arg(event_t *evt);

int ml_get_event_fd(event_t *evt);

void ml_add_event(event_t *evt);

void ml_del_event(event_t *evt);

int ml_dispatch_event();

void ml_dispatch_cancel();

#endif//__MAIN_LOOP__
