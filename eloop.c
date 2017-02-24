/******************************************************
 date :  2014/08/09
 author: duyahui
******************************************************/
#include "sys/time.h"
#include "sys/select.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "time.h"
#include "errno.h"
#include "eloop.h"
#include "xlist.h"

//select 默认超时时间
#define DEFAULT_TIMEOUT 300 //ms

//flags
#define F_TIMER	0x01
#define F_READ	0x02
#define F_WRITE	0x04
#define F_ADD	0x08

/*thread local only,each thread should has its own loop*/
struct tag_loop
{
  struct xlist_head timer_head;
  struct xlist_head read_head;
  struct xlist_head write_head;
  fd_set read_set;
  fd_set write_set;
  int max_fd;
  int runing;
};

//a event represents READ/WRITE/TIMER
struct tag_event
{
  int flag; //flag
  unsigned int value; //fd or ms
  unsigned int timeout; //expire use
  callback_t proc; //callback function
  void *arg; //point to user data
  void *ptr; //point to list element
};

/*
  list element to hold event_t, add hold_t to list rather than event_t itself
  make it possiable that when you want to delete other event while in a event callback,
  becasue the list is in a xlist_for_each looping
*/
typedef struct
{
  struct xlist_head xlist;
  void *ptr;
} hold_t;

static time_t poweron = 0;
static time_t uptime_ms()
{
  time_t t;
  struct timeval tv;
  if (0 == poweron) poweron = time(NULL);
  gettimeofday(&tv, NULL);
  t = (tv.tv_sec - poweron) * 1000 + tv.tv_usec / 1000;
  return t;
}

static void recalculate_max_fd(eloop_t *loop)
{
  hold_t *h;
  event_t *e;

  loop->max_fd = 0;
  xlist_for_each_entry(h,&loop->read_head,xlist,hold_t) {
    if (h->ptr) {
      e = (event_t*) h->ptr;
      if (e->value > loop->max_fd)
        loop->max_fd = e->value;
    }
  }

  xlist_for_each_entry(h,&loop->write_head,xlist,hold_t) {
    if (h->ptr) {
      e = (event_t*) h->ptr;
      if (e->value > loop->max_fd)
        loop->max_fd = e->value;
    }
  }
}

static void timer_next(eloop_t *loop, struct timeval *tv)
{
  hold_t *h;
  event_t *e, *p = NULL;
  time_t now = uptime_ms();

  //get first timer
  xlist_for_each_entry(h,&loop->timer_head,xlist,hold_t) {
    if (h->ptr) {
      p = (event_t*) h->ptr;
      break;
    }
  }

  //find minimum timeout timer
  xlist_for_each_entry(h,&loop->timer_head,xlist,hold_t) {
    if (h->ptr) {
      e = (event_t*) h->ptr;
      if (e->timeout < p->timeout) {
        p = e;
      }
    }
  }

  //if timer xlist is empty,use default tv
  if (p == NULL) {
    tv->tv_sec = DEFAULT_TIMEOUT / 1000;
    tv->tv_usec = (DEFAULT_TIMEOUT % 1000) * 1000;
    return;
  }

  //timer p is expired
  if (p->timeout <= now) {
    tv->tv_sec = 0;
    tv->tv_usec = 0;
  }
  else {
    unsigned int tmp = p->timeout - now;
    tmp = (tmp > DEFAULT_TIMEOUT) ? DEFAULT_TIMEOUT : tmp;
    tv->tv_sec = tmp / 1000;
    tv->tv_usec = (tmp % 1000) * 1000;
  }
}

static void clean_xlist(struct xlist_head *head)
{
  hold_t *h, *t;
  event_t *e;

  xlist_for_each_entry_safe(h,t,head,xlist,hold_t) {
    if (h->ptr) {
      e = (event_t*) h->ptr;
      e->flag &= ~F_ADD; //clear F_ADD flag
    }
    xlist_del(&h->xlist);
    free(h);
  }
}

static void process_fds(eloop_t *loop, struct xlist_head *rw_head, fd_set *fds, fd_set *origin)
{
  hold_t *h, *t;
  event_t *e;

  xlist_for_each_entry_safe(h,t,rw_head,xlist,hold_t) {
    /*the hold point to NULL should be delete and free*/
    if (h->ptr == NULL) {
      xlist_del(&h->xlist);
      free(h);
      continue;
    }

    e = (event_t*) h->ptr;

    if (FD_ISSET(e->value,fds) && FD_ISSET(e->value, origin)) {
      e->proc(loop, e, e->value, e->arg);
    }
  }
}

static void process_timer(eloop_t *loop)
{
  hold_t *h, *t;
  event_t *e;
  unsigned int now = uptime_ms();

  xlist_for_each_entry_safe(h,t,&loop->timer_head,xlist,hold_t) {
    /*the hold point to NULL should be delete and free*/
    if (h->ptr == NULL) {
      xlist_del(&h->xlist);
      free(h);
      continue;
    }

    e = (event_t*) h->ptr;

    if (now >= e->timeout) {
      //proc timer
      e->proc(loop, e, -1, e->arg);

      //h->ptr not null indicates the timer still alive otherwise e is not valid
      if (h->ptr) {
        //recalculate next expiring time
        e->timeout = now + e->value;
      }
    }
  }
}

event_t* e_event_new(int type, long fd_or_ms, callback_t fn, void *arg)
{
  if (fd_or_ms < 0 || !(type == E_READ || type == E_WRITE || type == E_TIMER)) {
    printf("params error\n");
    return NULL;
  }

  event_t *evt = malloc(sizeof(event_t));
  if (evt == NULL) {
    printf("malloc error\n");
    return NULL;
  }

  memset(evt,0,sizeof(event_t));
  evt->value = fd_or_ms;
  evt->proc = fn;
  evt->arg = arg;

  if (type == E_READ) {
    evt->flag = F_READ;
  }
  else if (type == E_WRITE) {
    evt->flag = F_WRITE;
  }
  else {
    evt->flag = F_TIMER;
  }

  return evt;
}

void e_event_free(event_t *evt)
{
  free(evt);
}

eloop_t* e_loop_new(void)
{
  eloop_t *loop = malloc(sizeof(eloop_t));
  if (loop == NULL) {
    printf("malloc error\n");
    return NULL;
  }

  memset(loop,0,sizeof(eloop_t));
  INIT_XLIST_HEAD(&loop->timer_head);
  INIT_XLIST_HEAD(&loop->read_head);
  INIT_XLIST_HEAD(&loop->write_head);
  FD_ZERO(&loop->read_set);
  FD_ZERO(&loop->write_set);
  return loop;
}

void e_loop_free(eloop_t *loop)
{
  free(loop);
}

void e_event_add(eloop_t* loop, event_t *e)
{
  hold_t *h;

  //alread added,return
  if (e->flag & F_ADD) {
    printf("alread added\n");
    return;
  }

  h = malloc(sizeof(hold_t));
  if (h == NULL) {
    printf("malloc error\n");
    return;
  }

  //point to each other
  h->ptr = e;
  e->ptr = h;

  if (e->flag & F_TIMER) {
    //caculate next expire time
    unsigned int now = uptime_ms();
    e->timeout = now + e->value;
    xlist_add(&h->xlist, &loop->timer_head);
  }
  else {
    if (e->value > loop->max_fd) {
      loop->max_fd = e->value;
    }

    if (e->flag & F_READ) {
      FD_SET(e->value, &loop->read_set);
      xlist_add(&h->xlist, &loop->read_head);
    }
    else {
      FD_SET(e->value, &loop->write_set);
      xlist_add(&h->xlist, &loop->write_head);
    }
  }

  e->flag |= F_ADD;
}

void e_event_del(eloop_t* loop, event_t *e)
{
  hold_t *h;

  //never added,return
  if (!(e->flag & F_ADD)) {
    printf("never added\n");
    return;
  }

  //detach from hold
  h = (hold_t*) e->ptr;
  h->ptr = NULL;

  if (e->flag & F_READ) {
    FD_CLR(e->value, &loop->read_set);
    recalculate_max_fd(loop);
  }
  else if (e->flag & F_WRITE) {
    FD_CLR(e->value, &loop->write_set);
    recalculate_max_fd(loop);
  }

  //clear F_ADD flag
  e->flag &= ~F_ADD;
}

void e_event_mod(eloop_t* loop, event_t *e, long ms)
{
  e->value = ms;

  //if the timer is alread added, delete it and add again
  if (e->flag & F_ADD) {
    e_event_del(loop, e);
    e_event_add(loop, e);
  }
}

int e_loop_run(eloop_t* loop)
{
  int ret = 0;
  fd_set read_set;
  fd_set write_set;
  struct timeval tv;

  loop->runing = 1;

  while (loop->runing) {
    //assign fds
    read_set = loop->read_set;
    write_set = loop->write_set;

    //pick next expired time
    timer_next(loop, &tv);

    //printf("picked:%d,%d\n",tv.tv_sec,tv.tv_usec);
    if ((ret = select(loop->max_fd + 1, &read_set, &write_set, NULL,&tv)) < 0){
      if(errno != EINTR){
        printf("****************select error**********************\n");
        printf("max_fd:%d,sec:%ld,usec:%ld\n",loop->max_fd,tv.tv_sec,tv.tv_usec);
        goto end;
      }
      continue;
    }

    //Test read fds
    process_fds(loop, &loop->read_head, &read_set, &loop->read_set);

    //Test write fds
    process_fds(loop, &loop->write_head, &write_set, &loop->write_set);

    //Test timer
    process_timer(loop);
  }

 end:
  /*when canceled loop clean hold xlist*/
  clean_xlist(&loop->read_head);
  clean_xlist(&loop->write_head);
  clean_xlist(&loop->timer_head);
  return ret;
}

void e_loop_cancel(eloop_t* loop)
{
  loop->runing = 0;
}
