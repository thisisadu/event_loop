#include <stdio.h>
#include <unistd.h>
#include "main_loop.h"


void timer_proc(event_t *evt)
{
  static int count = 0;
  event_t *timer = ml_get_event_arg(evt);

  if(count == 5)
	{
      ml_mod_timer_event(timer,3,0);
	}else if(count == 10)
	{
      ml_del_event(timer);
	}

  count++;
}

void fd_proc(event_t *evt)
{
		  int fd = ml_get_event_fd(evt);
      char buf[48] = {0};

      read(fd,buf,48);

      printf("fd_proc:%s\n",buf);
}


int main(int argc,char**argv)
{
	event_t read;
	event_t timer;

	ml_init();

  ml_init_read_event(&read,0,fd_proc,NULL);
  ml_init_timer_event(&timer,0,400000,timer_proc,&timer);


  ml_add_event(&read);
  ml_add_event(&timer);

	ml_dispatch_event();

	return 0;
}
