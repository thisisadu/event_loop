#include "main_loop.h"

#define F_TIMER	0x01
#define F_READ	0x02
#define F_WRITE	0x04
#define F_ADD	  0x08

typedef struct
{
    struct list_head list;
    void *ptr;
}hold_t;

static struct list_head g_timer_head;
static struct list_head g_read_head;
static struct list_head g_write_head;
static fd_set g_read_set;
static fd_set g_write_set;
static int g_max_fd = 0;
static int g_loop = 0;

static void recalculate_max_fd()
{
    hold_t *h;
    event_t *e;

    g_max_fd = 0;
    list_for_each_entry(h,&g_read_head,list){
      if(h->ptr){
        e = (event_t*)h->ptr;
        if(e->fd > g_max_fd)
          g_max_fd = e->fd;
      }
    }

    list_for_each_entry(h,&g_write_head,list){
      if(h->ptr){
        e = (event_t*)h->ptr;
        if(e->fd > g_max_fd)
          g_max_fd = e->fd;
      }
    }
}

static void timer_next(struct timeval *tv)
{
    hold_t *h;
    event_t *e,*p = NULL;
    struct timeval now;

    //get first timer
    list_for_each_entry(h,&g_timer_head,list){
      //printf("h->ptr:%p\n",h->ptr);
      if(h->ptr){
        p = (event_t*)h->ptr;
        //printf("%d:%d\n",p->timeout.tv_sec,p->timeout.tv_usec);
        break;
      }
    }

    //find minmum timeout timer
    list_for_each_entry(h,&g_timer_head,list){
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
    list_for_each_entry_safe(h,t,head,list)
    {
        list_del(&h->list);
        free(h);
    }
}

static void process_fds(struct list_head *rw_head,fd_set *fds)
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

      if(FD_ISSET(e->fd,fds)){
          e->proc(e);
      }
    }
}

static void process_timer()
{
    hold_t *h,*t;
    event_t *e;
    struct timeval now,tv;

    gettimeofday(&now, NULL);

    list_for_each_entry_safe(h,t,&g_timer_head,list){
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
        //recalculate next expire time
        tv.tv_sec = e->secs;
        tv.tv_usec = e->usecs;
        //printf("%d,%d\n",e->secs,e->usecs);
        timeradd(&now, &tv, &e->timeout);
      }
    }
}

void ml_init()
{
    INIT_LIST_HEAD(&g_timer_head);
    INIT_LIST_HEAD(&g_read_head);
    INIT_LIST_HEAD(&g_write_head);
    FD_ZERO(&g_read_set);/*初始化描述符集*/
    FD_ZERO(&g_write_set);/*初始化描述符集*/
}

void ml_init_read_event(event_t *e,int fd,callback_t fn,void *arg)
{
    e->fd = fd;
    e->proc = fn;
    e->arg = arg;
    e->flag = F_READ;
}

void ml_init_write_event(event_t *e,int fd,callback_t fn,void *arg)
{
    e->fd = fd;
    e->proc = fn;
    e->arg = arg;
    e->flag = F_WRITE;
}

void ml_init_timer_event(event_t *e,int secs,int usecs,callback_t fn,void *arg)
{
    e->secs = secs;
    e->usecs = usecs;
    e->proc = fn;
    e->arg = arg;
    e->flag = F_TIMER;
}

void ml_mod_timer_event(event_t *e,int secs,int usecs)
{
    e->secs = secs;
    e->usecs = usecs;

    //if the timer is alread added, delete it and add again
    if(e->flag & F_ADD){
      ml_del_event(e);
      ml_add_event(e);
    }
}

int ml_get_event_fd(event_t *e)
{
    return e->fd;
}

void* ml_get_event_arg(event_t *e)
{
    return e->arg;
}

void ml_add_event(event_t *e)
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
        list_add(&h->list,&g_timer_head);
    }else{
      if(e->fd > g_max_fd){
        g_max_fd = e->fd;
      }

      if(e->flag & F_READ){
        FD_SET(e->fd,&g_read_set);
        list_add(&h->list,&g_read_head);
      }
      else{
        FD_SET(e->fd,&g_write_set);
        list_add(&h->list,&g_write_head);
      }
    }

    e->flag |= F_ADD;
}

void ml_del_event(event_t *e)
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
      FD_CLR(e->fd,&g_read_set);
      recalculate_max_fd();
    }else if(e->flag & F_WRITE){
      FD_CLR(e->fd,&g_write_set);
      recalculate_max_fd();
    }

    //clear F_ADD flag
    e->flag &= ~F_ADD;
}

int ml_dispatch_event()
{
    int ret;
    fd_set read_set;
    fd_set write_set;
    struct timeval tv;

    g_loop = 1;

    while(g_loop){
        //assign fds
        read_set = g_read_set;
        write_set = g_write_set;

        //pick next expired time
        timer_next(&tv);
        printf("picked:%d,%d\n",tv.tv_sec,tv.tv_usec);
        if((ret = select(g_max_fd + 1,&read_set,&write_set,NULL,&tv)) < 0){
          if (errno != EINTR) {
            printf("select error");
            return -1;
          }
          continue;
        }

        //Test read fds
        process_fds(&g_read_head,&read_set);

        //Test write fds
        process_fds(&g_write_head,&write_set);

        //Test timer
        process_timer();
    }

    /*when canceled loop clean hold list*/
    clean_list(&g_read_head);
    clean_list(&g_write_head);
    clean_list(&g_timer_head);

    return 0;
}

void ml_dispatch_cancel()
{
    g_loop = 0;
}
