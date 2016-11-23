#include <string.h>
#include "eloop.h"

#define F_TIMER	0x01
#define F_READ	0x02
#define F_WRITE	0x04
#define F_ADD	  0x08

typedef struct
{
    struct list_head list;
    void *ptr;
}hold_t;

//for debug
#ifdef ELOOP_DEBUG_OPEN
static void print_count(eloop_t *loop)
{
  hold_t *h;
  int timers = 0,read_fds = 0, write_fds = 0;

  list_for_each_entry(h,&loop->timer_head,list){
      if(h->ptr){
        timers++;
      }
  }

  list_for_each_entry(h,&loop->read_head,list){
      if(h->ptr){
        read_fds++;
      }
  }

  list_for_each_entry(h,&loop->write_head,list){
      if(h->ptr){
        write_fds++;
      }
  }

  printf("^^^^^^^^^^^^^^^^^loop:%p,timers:%d,read_fds:%d,write_fds:%d^^^^^^^^^^^^^^^^^^\n",loop,timers,read_fds,write_fds);
}
#endif

static void recalculate_max_fd(eloop_t *loop)
{
    hold_t *h;
    event_t *e;

    loop->max_fd = 0;
    list_for_each_entry(h,&loop->read_head,list){
      if(h->ptr){
        e = (event_t*)h->ptr;
        if(e->fd > loop->max_fd)
          loop->max_fd = e->fd;
      }
    }

    list_for_each_entry(h,&loop->write_head,list){
      if(h->ptr){
        e = (event_t*)h->ptr;
        if(e->fd > loop->max_fd)
          loop->max_fd = e->fd;
      }
    }
}

static void timer_next(eloop_t *loop,struct timeval *tv)
{
    hold_t *h;
    event_t *e,*p = NULL;
    struct timeval now;

    //get first timer
    list_for_each_entry(h,&loop->timer_head,list){
      //printf("h->ptr:%p\n",h->ptr);
      if(h->ptr){
        p = (event_t*)h->ptr;
        //printf("%d:%d\n",p->timeout.tv_sec,p->timeout.tv_usec);
        break;
      }
    }

    //find minimum timeout timer
    list_for_each_entry(h,&loop->timer_head,list){
      if(h->ptr){
        e = (event_t*)h->ptr;
        if(timercmp(&e->timeout, &p->timeout,<)){
          p = e;
        }
      }
    }

    //if timer list is empty,use default tv(5S)
    if(p == NULL){
      tv->tv_sec = 5;
      tv->tv_usec = 0;
    }else{
      gettimeofday(&now, NULL);
      if(timercmp(&p->timeout, &now, <=)){
        timerclear(tv);
      }else{
        timersub(&p->timeout, &now, tv);
      }
    }
}

static void clean_list(struct list_head *head)
{
    hold_t *h,*t;
    event_t *e;

    list_for_each_entry_safe(h,t,head,list){
        if(h->ptr){
          e = (event_t*)h->ptr;
          //clear F_ADD flag
          e->flag &= ~F_ADD;
        }
        list_del(&h->list);
        free(h);
    }
}

static void process_fds(struct list_head *rw_head,fd_set *fds,fd_set *origin)
{
    hold_t *h,*t;
    event_t *e;

    list_for_each_entry_safe(h,t,rw_head,list){
      /*the hold point to NULL should be delete and free*/
      if(h->ptr == NULL){
          list_del(&h->list);
          free(h);
          continue;
      }

      e = (event_t*)h->ptr;

      if(FD_ISSET(e->fd,fds) && FD_ISSET(e->fd,origin)){
          e->proc(e);
      }
    }
}

static void process_timer(eloop_t *loop)
{
    hold_t *h,*t;
    event_t *e;
    struct timeval now,tv;

    gettimeofday(&now, NULL);

    list_for_each_entry_safe(h,t,&loop->timer_head,list){
      /*the hold point to NULL should be delete and free*/
      if(h->ptr == NULL){
          list_del(&h->list);
          free(h);
          continue;
      }

      e = (event_t*)h->ptr;

      if(timercmp(&now,&e->timeout, >=)){
        //proc timer
        e->proc(e);

        //h->ptr not null indicates the timer still alive otherwise e is not valid
        if(h->ptr){
          //recalculate next expire time
          tv.tv_sec = e->secs;
          tv.tv_usec = e->usecs;
          //printf("%d,%d\n",e->secs,e->usecs);
          timeradd(&now, &tv, &e->timeout);
        }
      }
    }
}

void e_init_read_event(event_t *e,int fd,callback_t fn,void *arg)
{
    e->fd = fd;
    e->proc = fn;
    e->arg = arg;
    e->flag = F_READ;
}

void e_init_write_event(event_t *e,int fd,callback_t fn,void *arg)
{
    e->fd = fd;
    e->proc = fn;
    e->arg = arg;
    e->flag = F_WRITE;
}

void e_init_timer_event(event_t *e,int secs,int usecs,callback_t fn,void *arg)
{
    e->secs = secs;
    e->usecs = usecs;
    e->proc = fn;
    e->arg = arg;
    e->flag = F_TIMER;
}

int e_get_event_fd(event_t *e)
{
    return e->fd;
}

void* e_get_event_arg(event_t *e)
{
    return e->arg;
}

void e_init(eloop_t* loop)
{
    memset(loop,0,sizeof(*loop));
    INIT_LIST_HEAD(&loop->timer_head);
    INIT_LIST_HEAD(&loop->read_head);
    INIT_LIST_HEAD(&loop->write_head);
    FD_ZERO(&loop->read_set);/*初始化描述符集*/
    FD_ZERO(&loop->write_set);/*初始化描述符集*/
}

void e_add_event(eloop_t* loop,event_t *e)
{
    hold_t *h;

    //alread added,return
    if(e->flag & F_ADD){
      printf("alread added\n");
      return;
    }

    h = malloc(sizeof(hold_t));
    if(h == NULL)
    {
        printf("malloc error");
        return;
    }

    //point to each other
    h->ptr = e;
    e->ptr = h;

    if(e->flag & F_TIMER){
        //caculate next expire time
        struct timeval now,tv;
        tv.tv_sec = e->secs;
        tv.tv_usec = e->usecs;
        gettimeofday(&now, NULL);
        timeradd(&now, &tv, &e->timeout);
        //add to hold list
        list_add(&h->list,&loop->timer_head);
    }else{
      if(e->fd > loop->max_fd){
        loop->max_fd = e->fd;
      }

      if(e->flag & F_READ){
        FD_SET(e->fd,&loop->read_set);
        list_add(&h->list,&loop->read_head);
      }else{
        FD_SET(e->fd,&loop->write_set);
        list_add(&h->list,&loop->write_head);
      }
    }

    e->flag |= F_ADD;
}

void e_del_event(eloop_t* loop,event_t *e)
{
    hold_t *h;

    //never added,return
    if(!(e->flag & F_ADD)){
      printf("never added\n");
      return;
    }

    //detach from hold
    h = (hold_t*)e->ptr;
    h->ptr = NULL;

    if(e->flag & F_READ){
      FD_CLR(e->fd,&loop->read_set);
      recalculate_max_fd(loop);
    }else if(e->flag & F_WRITE){
      FD_CLR(e->fd,&loop->write_set);
      recalculate_max_fd(loop);
    }

    //clear F_ADD flag
    e->flag &= ~F_ADD;
}

void e_mod_timer_event(eloop_t* loop,event_t *e,int secs,int usecs)
{
    e->secs = secs;
    e->usecs = usecs;

    //if the timer is alread added, delete it and add again
    if(e->flag & F_ADD){
      e_del_event(loop,e);
      e_add_event(loop,e);
    }
}

int e_dispatch_event(eloop_t* loop)
{
    int ret = 0;
    fd_set read_set;
    fd_set write_set;
    struct timeval tv;

    loop->runing = 1;

    while(loop->runing){
        //assign fds
        read_set = loop->read_set;
        write_set = loop->write_set;

#ifdef ELOOP_DEBUG_OPEN
        print_count(loop);
#endif
        //pick next expired time
        timer_next(loop,&tv);
        //printf("picked:%d,%d\n",tv.tv_sec,tv.tv_usec);
        if((ret = select(loop->max_fd + 1,&read_set,&write_set,NULL,&tv)) < 0){
          if (errno != EINTR) {
            printf("****************select error**********************max_fd:%d,sec:%ld,usec:%ld\n",loop->max_fd,tv.tv_sec,tv.tv_usec);
            ret = -1;
            goto end;
          }
          continue;
        }

        //Test read fds
        process_fds(&loop->read_head,&read_set,&loop->read_set);

        //Test write fds
        process_fds(&loop->write_head,&write_set,&loop->write_set);

        //Test timer
        process_timer(loop);
    }

end:
    /*when canceled loop clean hold list*/
    clean_list(&loop->read_head);
    clean_list(&loop->write_head);
    clean_list(&loop->timer_head);
    return ret;
}

void e_dispatch_cancel(eloop_t* loop)
{
    loop->runing = 0;
}
