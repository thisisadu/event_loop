#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>
#include "eloop.h"

void timer_proc(eloop_t *loop,event_t *evt,long fd,void* arg)
{
	static int count = 0;
	struct timeval tv;
	gettimeofday(&tv,NULL);

	printf("count:%d,%d,%d\n",count,tv.tv_sec,tv.tv_usec);

	/*delete timer*/
	if(count++ == 10){
    e_event_del(loop,evt);
    e_event_free(evt);
    e_loop_cancel(loop);
  }
}

void fd_proc(eloop_t *loop,event_t *evt,long fd,void* arg)
{
	printf("fd_proc\n");
	e_event_del(loop,evt);
	e_event_free(evt);
	close(fd);
}


int main(int argc,char**argv)
{
	eloop_t *loop = e_loop_new();

	int fd = socket(AF_INET,SOCK_STREAM,0);

	event_t *node = e_event_new(E_READ,fd,fd_proc,NULL);

	event_t *timer = e_event_new(E_TIMER,3000,timer_proc,NULL);

	e_event_add(loop,node);

	e_event_add(loop,timer);

	e_loop_run(loop);

  e_loop_free(loop);

	return 0;
}
