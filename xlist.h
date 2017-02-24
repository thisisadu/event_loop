#ifndef __XLIST_H
#define __XLIST_H

//#if defined(__KERNEL__) || defined(_LVM_H_INCLUDE)

//#include <linux/prefetch.h>

 static void prefetch(const void *x) {;}

 static void prefetchw(const void *x) {;}

/*
 * Simple doubly linked xlist implementation.
 *
 * Some of the internal functions ("__xxx") are useful when
 * manipulating whole xlists rather than single entries, as
 * sometimes we already know the next/prev entries and we can
 * generate better code by using them directly rather than
 * using the generic single-entry routines.
 */

struct xlist_head {
	struct xlist_head *next, *prev;
};

#define XLIST_HEAD_INIT(name) { &(name), &(name) }

#define XLIST_HEAD(name) \
	struct xlist_head name = XLIST_HEAD_INIT(name)

#define INIT_XLIST_HEAD(ptr) do { \
	(ptr)->next = (ptr); (ptr)->prev = (ptr); \
} while (0)

/*
 * Insert a new entry between two known consecutive entries. 
 *
 * This is only for internal xlist manipulation where we know
 * the prev/next entries already!
 */
static  void __xlist_add(struct xlist_head *new,
			      struct xlist_head *prev,
			      struct xlist_head *next)
{
	next->prev = new;
	new->next = next;
	new->prev = prev;
	prev->next = new;
}

/**
 * xlist_add - add a new entry
 * @new: new entry to be added
 * @head: xlist head to add it after
 *
 * Insert a new entry after the specified head.
 * This is good for implementing stacks.
 */
static  void xlist_add(struct xlist_head *new, struct xlist_head *head)
{
	__xlist_add(new, head, head->next);
}

/**
 * xlist_add_tail - add a new entry
 * @new: new entry to be added
 * @head: xlist head to add it before
 *
 * Insert a new entry before the specified head.
 * This is useful for implementing queues.
 */
static  void xlist_add_tail(struct xlist_head *new, struct xlist_head *head)
{
	__xlist_add(new, head->prev, head);
}

/*
 * Delete a xlist entry by making the prev/next entries
 * point to each other.
 *
 * This is only for internal xlist manipulation where we know
 * the prev/next entries already!
 */
static  void __xlist_del(struct xlist_head *prev, struct xlist_head *next)
{
	next->prev = prev;
	prev->next = next;
}

/**
 * xlist_del - deletes entry from xlist.
 * @entry: the element to delete from the xlist.
 * Note: xlist_empty on entry does not return true after this, the entry is in an undefined state.
 */
static  void xlist_del(struct xlist_head *entry)
{
	__xlist_del(entry->prev, entry->next);
	entry->next = (void *) 0;
	entry->prev = (void *) 0;
}

/**
 * xlist_del_init - deletes entry from xlist and reinitialize it.
 * @entry: the element to delete from the xlist.
 */
static  void xlist_del_init(struct xlist_head *entry)
{
	__xlist_del(entry->prev, entry->next);
	INIT_XLIST_HEAD(entry); 
}

/**
 * xlist_move - delete from one xlist and add as another's head
 * @xlist: the entry to move
 * @head: the head that will precede our entry
 */
static  void xlist_move(struct xlist_head *xlist, struct xlist_head *head)
{
        __xlist_del(xlist->prev, xlist->next);
        xlist_add(xlist, head);
}

/**
 * xlist_move_tail - delete from one xlist and add as another's tail
 * @xlist: the entry to move
 * @head: the head that will follow our entry
 */
static  void xlist_move_tail(struct xlist_head *xlist,
				  struct xlist_head *head)
{
        __xlist_del(xlist->prev, xlist->next);
        xlist_add_tail(xlist, head);
}

/**
 * xlist_empty - tests whether a xlist is empty
 * @head: the xlist to test.
 */
static  int xlist_empty(struct xlist_head *head)
{
	return head->next == head;
}

static  void __xlist_splice(struct xlist_head *xlist,
				 struct xlist_head *head)
{
	struct xlist_head *first = xlist->next;
	struct xlist_head *last = xlist->prev;
	struct xlist_head *at = head->next;

	first->prev = head;
	head->next = first;

	last->next = at;
	at->prev = last;
}

/**
 * xlist_splice - join two xlists
 * @xlist: the new xlist to add.
 * @head: the place to add it in the first xlist.
 */
static  void xlist_splice(struct xlist_head *xlist, struct xlist_head *head)
{
	if (!xlist_empty(xlist))
		__xlist_splice(xlist, head);
}

/**
 * xlist_splice_init - join two xlists and reinitialise the emptied xlist.
 * @xlist: the new xlist to add.
 * @head: the place to add it in the first xlist.
 *
 * The xlist at @xlist is reinitialised
 */
static  void xlist_splice_init(struct xlist_head *xlist,
				    struct xlist_head *head)
{
	if (!xlist_empty(xlist)) {
		__xlist_splice(xlist, head);
		INIT_XLIST_HEAD(xlist);
	}
}

/**
 * xlist_entry - get the struct for this entry
 * @ptr:	the &struct xlist_head pointer.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the xlist_struct within the struct.
 */
#define xlist_entry(ptr, type, member) \
	((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))

/**
 * xlist_for_each	-	iterate over a xlist
 * @pos:	the &struct xlist_head to use as a loop counter.
 * @head:	the head for your xlist.
 */
#define xlist_for_each(pos, head) \
	for (pos = (head)->next, prefetch(pos->next); pos != (head); \
        	pos = pos->next, prefetch(pos->next))
/**
 * xlist_for_each_prev	-	iterate over a xlist backwards
 * @pos:	the &struct xlist_head to use as a loop counter.
 * @head:	the head for your xlist.
 */
#define xlist_for_each_prev(pos, head) \
	for (pos = (head)->prev, prefetch(pos->prev); pos != (head); \
        	pos = pos->prev, prefetch(pos->prev))
        	
/**
 * xlist_for_each_safe	-	iterate over a xlist safe against removal of xlist entry
 * @pos:	the &struct xlist_head to use as a loop counter.
 * @n:		another &struct xlist_head to use as temporary storage
 * @head:	the head for your xlist.
 */
#define xlist_for_each_safe(pos, n, head) \
	for (pos = (head)->next, n = pos->next; pos != (head); \
		pos = n, n = pos->next)

/**
 * xlist_for_each_entry	-	iterate over xlist of given type
 * @pos:	the type * to use as a loop counter.
 * @head:	the head for your xlist.
 * @member:	the name of the xlist_struct within the struct.
 */
#define xlist_for_each_entry(pos, head, member,type)              \
	for (pos = xlist_entry((head)->next, type, member),	\
		     prefetch(pos->member.next);			\
	     &pos->member != (head); 					\
	     pos = xlist_entry(pos->member.next, type, member),	\
		     prefetch(pos->member.next))

/**
 * xlist_for_each_entry_safe - iterate over xlist of given type safe against removal of xlist entry
 * @pos:	the type * to use as a loop counter.
 * @n:		another type * to use as temporary storage
 * @head:	the head for your xlist.
 * @member:	the name of the xlist_struct within the struct.
 */
#define xlist_for_each_entry_safe(pos, n, head, member,type)			\
	for (pos = xlist_entry((head)->next, type, member),	\
		n = xlist_entry(pos->member.next, type, member);	\
	     &pos->member != (head); 					\
	     pos = n, n = xlist_entry(n->member.next, type, member))

/**
 * xlist_for_each_entry_continue -       iterate over xlist of given type
 *                      continuing after existing point
 * @pos:        the type * to use as a loop counter.
 * @head:       the head for your xlist.
 * @member:     the name of the xlist_struct within the struct.
 */
#define xlist_for_each_entry_continue(pos, head, member,type)         \
	for (pos = xlist_entry(pos->member.next, type, member),	\
		     prefetch(pos->member.next);			\
	     &pos->member != (head);					\
	     pos = xlist_entry(pos->member.next, type, member),	\
		     prefetch(pos->member.next))

//#endif /* __KERNEL__ || _LVM_H_INCLUDE */

#endif

