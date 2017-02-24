#ifndef __ELOOP__
#define __ELOOP__

/*
handle of a loop
*/
typedef struct tag_loop eloop_t;

/*
handle of a event
*/
typedef struct tag_event event_t;

/*
callback funtion for events
@loop: the loop the event attached
@evt: the event itself who happened things
@fd:  when the event is read or write event,this is the fd,when the evt is a timer,fd equals -1
@arg: the extra data for evt
 */
typedef void (*callback_t)(eloop_t *loop,event_t *evt,long fd,void *arg);

/*
type for e_event_new
*/
enum{
  E_READ,
  E_WRITE,
  E_TIMER
};

/*
create a new loop handle
*/
eloop_t* e_loop_new(void);

/*
free a loop handle
*/
void e_loop_free(eloop_t *loop);

/*
when call this funtion,the thread will goes into a loop,
when you want to stop the loop,you can call e_loop_cancel
in a event callback or in another thread
*/
int  e_loop_run(eloop_t* loop);

/*
stop a loop's running,this is the only routine
that can be called in another thread
*/
void e_loop_cancel(eloop_t* loop);

/*
create a event with params
@type: E_READ/E_WRITE/E_TIMER
@fd_or_ms: the fd for a read or write event,or the interval(ms) for a timer event
@fn: the callback function for a event
@arg: extra data for user,it will pass to the event callback
*/
event_t* e_event_new(int type,long fd_or_ms,callback_t fn,void *arg);

/*
free a event handle,remember when you added a event to loop,
you must call e_event_del before call e_event_free
*/
void e_event_free(event_t *evt);

/*
add a event to a loop,
this can be called no matter the loop is running or not
*/
void e_event_add(eloop_t* loop,event_t *evt);

/*
delete a event from a loop,
this can be called no matter the loop is running or not
*/
void e_event_del(eloop_t* loop,event_t *evt);

/*
when you want to modify the expiring time of a timer, this function
can be called no matter the loop is runing or not,evt must be a timer event
*/
void e_event_mod(eloop_t* loop,event_t *evt,long ms);

#endif//__ELOOP__
