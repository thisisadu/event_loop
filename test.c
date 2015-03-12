#include <stdio.h>
#include <unistd.h>
#include "eloop.h"

eloop_t e_demo; //thread local data

void timer_proc(void *data)
{
	static int count = 0;
	ttimer_t *timer = (ttimer_t *)data;

	printf("count:%d\n",count);

	/*delete timer*/	
	if(count++ == 10)
	{
		//only can be called in own thread
		eloop_del_timer(&e_demo,timer);
		/*break eloop,can be called in own or other thread*/
		eloop_cancel(&e_demo);
	}
}

void fd_proc(void *data)
{
	node_t *node = (node_t *)data;

	
	//only can be called in own thread
	eloop_del_node(&e_demo,node);
	close(node->fd);
}


int main(int argc,char**argv)
{
	node_t node;
	ttimer_t timer;

	/*初始化事件循环*/
	eloop_init(&e_demo);

	/*int fd = socket();

	node.fd = fd;
	node.proc = fd_proc;

	//only can be called in own thread
	eloop_add_node(&e_demo,&node);
	*/
	

	timer.secs = 1;//unit: seconds
	timer.proc = timer_proc;
	timer.data = NULL;//extra data

	//only can be called in own thread
	eloop_add_timer(&e_demo,&timer);

	/*run to select*/
	eloop_run(&e_demo);

	return 0;
}




