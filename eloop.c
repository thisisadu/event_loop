/*eloop.c for event loop*/
#include <sys/time.h>
#include <sys/select.h>
#include <stdlib.h>
#include "eloop.h"


/*初始化eloop结构*/
void eloop_init(eloop_t* el)
{
    el->max_fd = 0;
    el->looping = 1;
    el->timeout.tv_sec = 0;
    el->timeout.tv_usec = 500000;/*设置等待时间为0.5秒*/
    FD_ZERO(&el->readfd);/*初始化描述符集*/
    INIT_LIST_HEAD(&el->fds);
    INIT_LIST_HEAD(&el->timers);
}

/*向loop中添加fd节点*/
void eloop_add_node(eloop_t* el,node_t *node)
{      
    element_t *element;
    list_for_each_entry(element,&el->fds,list)
    {
        if(element->ptr == node)
        {
            printf("eloop add node twice\n");
            return;
        }
    }

    element = malloc(sizeof(element_t));
    if(element == NULL)
    {
        perror("malloc error");
        return;
    }

    /*record the max fd*/
    if(node->fd > el->max_fd)
      el->max_fd = node->fd;

    /*把fd加入到描述符集中*/
    FD_SET(node->fd,&el->readfd);

    /*pointer to node*/
    element->ptr = node;

    /*add element to list other than node*/
    list_add(&element->list,&el->fds);
}

/*删除fd节点*/
void eloop_del_node(eloop_t* el,node_t *node)
{
    element_t *element;

    list_for_each_entry(element,&el->fds,list)
    {
        if(element->ptr == node)
        {
            FD_CLR(node->fd,&el->readfd);

            //the element will be delete
            element->ptr = NULL;
            break;
        }
    }
}


/*向loop中添加定时器和处理函数*/
void eloop_add_timer(eloop_t* el,ttimer_t *timer)
{
    element_t *element;

    list_for_each_entry(element,&el->timers,list)
    {
        if(element->ptr == timer)
        {
            printf("eloop add timer twice\n");
            return;
        }
    }

    element = malloc(sizeof(element_t));
    if(element == NULL)
    {
        perror("malloc error");
        return;
    }

    /*assign to left(real counter)*/
    timer->left = timer->secs;

    /*pointer to timer*/
    element->ptr = timer;

    /*add element to list other than timer*/
    list_add(&element->list,&el->timers);
}

/*修改定时器*/
void eloop_modify_timer(eloop_t* el,ttimer_t *timer)
{
    /*do the real delete do not cost much*/
    eloop_del_timer(el,timer);
    eloop_add_timer(el,timer);
}

/*删除定时器*/
void eloop_del_timer(eloop_t* el,ttimer_t *timer)
{
    element_t *element;

    list_for_each_entry(element,&el->timers,list)
    {
        if(element->ptr == timer)
        {
            //the element will be delete
            element->ptr = NULL;
            break;
        }
    }
}

/*进行select选择运行*/
void eloop_run(eloop_t *el)
{
    fd_set canread;
    time_t  last,now;
    node_t *node;
    ttimer_t *timer;
    element_t *element,*element2;
    struct timeval timeout;

    last = time(NULL);

    while(el->looping)
    {      
        timeout = el->timeout;
        canread = el->readfd;

        /*ignore ret*/
        select(el->max_fd + 1,&canread,NULL,NULL,&timeout);

        /*检测是否有文件描述符需要处理*/
        list_for_each_entry_safe(element,element2,&el->fds,list)
        {
            /*the element should be delete and free*/
            if(element->ptr == NULL)
            {
                list_del(&element->list);
                free(element);
                continue;
            }

            node = (node_t*)element->ptr;

            if(FD_ISSET(node->fd,&canread))
            {
                node->proc(node);
            }
        }

        now = time(NULL);
        if(now - last >= 1)
        {
            /*检测是否有超时定时器需要处理*/
            list_for_each_entry_safe(element,element2,&el->timers,list)
            {
                /*the element should be delete and free*/
                if(element->ptr == NULL)
                {
                    list_del(&element->list);
                    free(element);
                    continue;
                }

                timer = (ttimer_t*)element->ptr;

                if(!(--timer->left))
                {
                    timer->proc(timer);
                    timer->left = timer->secs;
                }

            }

            last = now;
        }

    }

    /*when canceled eloop clean node/timer list*/
    list_for_each_entry_safe(element,element2,&el->fds,list)
    {
        list_del(&element->list);
        free(element);
    }

    list_for_each_entry_safe(element,element2,&el->timers,list)
    {
        list_del(&element->list);
        free(element);
    }
}

/*停止eloop循环*/
void eloop_cancel(eloop_t* el)
{
    el->looping = 0;
}


