#ifndef __ELOOP__
#define __ELOOP__
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/select.h>
#include <time.h>
#include <errno.h>
#include "xlist.h"

//#define ELOOP_DEBUG_OPEN 1

/*事件循环，每个线程定义一个*/
typedef struct
{
  struct list_head timer_head;
  struct list_head read_head;
  struct list_head write_head;
  fd_set read_set;
  fd_set write_set;
  int max_fd;
  int runing;
}eloop_t;

typedef struct tag_event event_t;

typedef void (*callback_t)(event_t *evt);

//该结构体字段仅内部用，不要直接访问
struct tag_event
{
    int flag;//flag
    int fd;//fd
    int secs;//seconds
    int usecs;//useconds
    struct timeval timeout;//expire use
    callback_t proc;//callback function
    void *arg;//point to user data
    void *ptr;//point to list element
};

void e_init_read_event(event_t *evt,int fd,callback_t fn,void *arg);

void e_init_write_event(event_t *evt,int fd,callback_t fn,void *arg);

void e_init_timer_event(event_t *evt,int secs,int usecs,callback_t fn,void *arg);

void* e_get_event_arg(event_t *evt);

int  e_get_event_fd(event_t *evt);

void e_init(eloop_t* loop);

void e_mod_timer_event(eloop_t* loop,event_t *evt,int secs,int usecs);

void e_add_event(eloop_t* loop,event_t *evt);

void e_del_event(eloop_t* loop,event_t *evt);

int  e_dispatch_event(eloop_t* loop);

void e_dispatch_cancel(eloop_t* loop);

#endif//__ELOOP__
