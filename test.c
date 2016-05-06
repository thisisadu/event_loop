#include <stdio.h>
#include <unistd.h>
#include "eloop.h"

eloop_t e_demo; //thread local data

void timer_proc(event_t *evt)
{
	static int count = 0;

	printf("count:%d\n",count);

	/*delete timer*/	
	if(count++ == 10)
	{
		e_del_event(&e_demo,evt);
		e_dispatch_cancel(&e_demo);
	}
}

void fd_proc(event_t *evt)
{
	int fd = e_get_event_fd(evt);
	int *p = (int*)e_get_event_arg(evt);
	
	e_del_event(&e_demo,evt);
	close(fd);
}


int main(int argc,char**argv)
{
	event_t node;
	event_t timer;
	int arg;

	/*初始化事件循环*/
	e_init(&e_demo);

	int fd = socket();
	e_init_read_event(&node,fd,fd_proc,&arg);

	e_init_timer_event(&timer,3,0,timer_proc,NULL);

	e_add_event(&e_demo,&node);

	e_add_event(&e_demo,&timer);

	e_dispatch_event(&e_demo);

	return 0;
}




