#include "main_loop.h"

#define F_TIMER	0x01
#define F_READ	0x02
#define F_WRITE	0x04
#define F_ADD	  0x08
#define F_INIT	0x80

typedef struct
{
    struct list_head list;
    void *ptr;
}hold_t;

static struct list_head g_timers;
static struct list_head g_readfds;
static struct list_head g_writefds;
static int g_max_fd = 0;

void ml_init()
{
    INIT_LIST_HEAD(&g_timers);
    INIT_LIST_HEAD(&g_readfds);
    INIT_LIST_HEAD(&g_writefds);
}

void ml_init_read_event(event_t *e,int fd,callback_t fn,void *arg)
{
    e->fd = fd;
    e->proc = fn;
    e->arg = arg;
    e->flag = F_INIT | F_READ;
}

void ml_init_write_event(event_t *e,int fd,callback_t fn,void *arg)
{
    e->fd = fd;
    e->proc = fn;
    e->arg = arg;
    e->flag = F_INIT | F_WRITE;
}

void ml_init_timer_event(event_t *e,int secs,int usecs,callback_t fn,void *arg)
{
    e->secs = secs;
    e->usecs = usecs;
    e->proc = fn;
    e->arg = arg;
    e->flag = F_INIT | F_TIMER;
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
    element_t *element;

    //alread added,return
    if(e->flag & F_ADD){
      printf("alread added\n");
      return;
    }

    hold = malloc(sizeof(hold_t));
    if(hold == NULL){
        printf("malloc error\n");
        return;
    }
    hold->ptr = e;

    if(e->flag & F_TIMER){

    }else if(e->flag & F_READ){

      if(e->fd > g_max_fd){
        g_max_fd = e->fd;
      }

      list_add(&hold->list,&g_readfds);
    }else if(e->flag & F_WRITE){

      if(e->fd > g_max_fd){
        g_max_fd = e->fd;
      }

      list_add(&hold->list,&g_writefds);
    }
}

void ml_del_event(event_t *e)
{
    //never added,return
    if(!(e->flag & F_ADD)){
      printf("never added\n");
      return;
    }
}

int ml_dispatch_event()
{

}
