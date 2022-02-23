/*
* Copyright (c) 2014 Amlogic, Inc. All rights reserved.
*
* This source code is subject to the terms and conditions defined in the
* file 'LICENSE' which is part of this source code package. *
* Description:
*/

#ifndef _LINUX_LIST_H
#define _LINUX_LIST_H

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif

#ifndef container_of
/**
 * container_of - cast a member of a structure out to the containing structure
 * @ptr:    the pointer to the member.
 * @type:    the type of the container struct this is embedded in.
 * @member:    the name of the member within the struct.
 *
 */
#define container_of(ptr, type, member) ({            \
    const typeof(((type *)0)->member) * __mptr = (ptr);    \
    (type *)((char *)__mptr - offsetof(type, member)); })
#endif

#define LIST_POISON1  ((void *) 0x00100100)
#define LIST_POISON2  ((void *) 0x00200200)

#define prefetch(x) ((x) = (x))


/*
 * Simple doubly linked list implementation.
 *
 * Some of the internal functions ("__xxx") are useful when
 * manipulating whole lists rather than single entries, as
 * sometimes we already know the cnext/cprev entries and we can
 * generate better code by using them directly rather than
 * using the generic single-entry routines.
 */

struct list_head {
    struct list_head *cnext, *cprev;
};

#define LIST_HEAD_INIT(name) { &(name), &(name) }

#define LIST_HEAD(name) \
    struct list_head name = LIST_HEAD_INIT(name)

static inline void INIT_LIST_HEAD(struct list_head *list)
{
    list->cnext = list;
    list->cprev = list;
}

/*
 * Insert a cnew entry between two known consecutive entries.
 *
 * This is only for internal list manipulation where we know
 * the cprev/cnext entries already!
 */
#ifndef CONFIG_DEBUG_LIST
static inline void __list_add(struct list_head *cnew,
                  struct list_head *cprev,
                  struct list_head *cnext)
{
    cnext->cprev = cnew;
    cnew->cnext = cnext;
    cnew->cprev = cprev;
    cprev->cnext = cnew;
}
#else
extern void __list_add(struct list_head *cnew,
                  struct list_head *prev,
                  struct list_head *next);
#endif

/**
 * list_add - add a cnew entry
 * @cnew: cnew entry to be added
 * @head: list head to add it after
 *
 * Insert a cnew entry after the specified head.
 * This is good for implementing stacks.
 */
static inline void list_add(struct list_head *cnew, struct list_head *head)
{
    __list_add(cnew, head, head->cnext);
}


/**
 * list_add_tail - add a cnew entry
 * @cnew: cnew entry to be added
 * @head: list head to add it before
 *
 * Insert a cnew entry before the specified head.
 * This is useful for implementing queues.
 */
static inline void list_add_tail(struct list_head *cnew, struct list_head *head)
{
    __list_add(cnew, head->cprev, head);
}

/*
 * Delete a list entry by making the cprev/cnext entries
 * point to each other.
 *
 * This is only for internal list manipulation where we know
 * the cprev/cnext entries already!
 */
static inline void __list_del(struct list_head * cprev, struct list_head * cnext)
{
    cnext->cprev = cprev;
    cprev->cnext = cnext;
}

/**
 * list_del - deletes entry from list.
 * @entry: the element to delete from the list.
 * Note: list_empty() on entry does not return true after this, the entry is
 * in an undefined state.
 */
#ifndef CONFIG_DEBUG_LIST
static inline void list_del(struct list_head *entry)
{
    __list_del(entry->cprev, entry->cnext);
    entry->cnext = (struct list_head *)LIST_POISON1;
    entry->cprev = (struct list_head *)LIST_POISON2;
}
#else
extern void list_del(struct list_head *entry);
#endif

/**
 * list_replace - replace old entry by cnew one
 * @old : the element to be replaced
 * @cnew : the cnew element to insert
 *
 * If @old was empty, it will be overwritten.
 */
static inline void list_replace(struct list_head *old,
                struct list_head *cnew)
{
    cnew->cnext = old->cnext;
    cnew->cnext->cprev = cnew;
    cnew->cprev = old->cprev;
    cnew->cprev->cnext = cnew;
}

static inline void list_replace_init(struct list_head *old,
                    struct list_head *cnew)
{
    list_replace(old, cnew);
    INIT_LIST_HEAD(old);
}

/**
 * list_del_init - deletes entry from list and reinitialize it.
 * @entry: the element to delete from the list.
 */
static inline void list_del_init(struct list_head *entry)
{
    __list_del(entry->cprev, entry->cnext);
    INIT_LIST_HEAD(entry);
}

/**
 * list_move - delete from one list and add as another's head
 * @list: the entry to move
 * @head: the head that will precede our entry
 */
static inline void list_move(struct list_head *list, struct list_head *head)
{
    __list_del(list->cprev, list->cnext);
    list_add(list, head);
}

/**
 * list_move_tail - delete from one list and add as another's tail
 * @list: the entry to move
 * @head: the head that will follow our entry
 */
static inline void list_move_tail(struct list_head *list,
                  struct list_head *head)
{
    __list_del(list->cprev, list->cnext);
    list_add_tail(list, head);
}

/**
 * list_is_last - tests whether @list is the last entry in list @head
 * @list: the entry to test
 * @head: the head of the list
 */
static inline int list_is_last(const struct list_head *list,
                const struct list_head *head)
{
    return list->cnext == head;
}

/**
 * list_empty - tests whether a list is empty
 * @head: the list to test.
 */
static inline int list_empty(const struct list_head *head)
{
    return head->cnext == head;
}

/**
 * list_empty_careful - tests whether a list is empty and not being modified
 * @head: the list to test
 *
 * Description:
 * tests whether a list is empty _and_ checks that no other CPU might be
 * in the process of modifying either member (cnext or cprev)
 *
 * NOTE: using list_empty_careful() without synchronization
 * can only be safe if the only activity that can happen
 * to the list entry is list_del_init(). Eg. it cannot be used
 * if another CPU could re-list_add() it.
 */
static inline int list_empty_careful(const struct list_head *head)
{
    struct list_head *cnext = head->cnext;
    return (cnext == head) && (cnext == head->cprev);
}

/**
 * list_rotate_left - rotate the list to the left
 * @head: the head of the list
 */
static inline void list_rotate_left(struct list_head *head)
{
    struct list_head *first;

    if (!list_empty(head)) {
        first = head->cnext;
        list_move_tail(first, head);
    }
}

/**
 * list_is_singular - tests whether a list has just one entry.
 * @head: the list to test.
 */
static inline int list_is_singular(const struct list_head *head)
{
    return !list_empty(head) && (head->cnext == head->cprev);
}

static inline void __list_cut_position(struct list_head *list,
        struct list_head *head, struct list_head *entry)
{
    struct list_head *cnew_first = entry->cnext;
    list->cnext = head->cnext;
    list->cnext->cprev = list;
    list->cprev = entry;
    entry->cnext = list;
    head->cnext = cnew_first;
    cnew_first->cprev = head;
}

/**
 * list_cut_position - cut a list into two
 * @list: a cnew list to add all removed entries
 * @head: a list with entries
 * @entry: an entry within head, could be the head itself
 *    and if so we won't cut the list
 *
 * This helper moves the initial part of @head, up to and
 * including @entry, from @head to @list. You should
 * pass on @entry an element you know is on @head. @list
 * should be an empty list or a list you do not care about
 * losing its data.
 *
 */
static inline void list_cut_position(struct list_head *list,
        struct list_head *head, struct list_head *entry)
{
    if (list_empty(head))
        return;
    if (list_is_singular(head) &&
        (head->cnext != entry && head != entry))
        return;
    if (entry == head)
        INIT_LIST_HEAD(list);
    else
        __list_cut_position(list, head, entry);
}

static inline void __list_splice(const struct list_head *list,
                 struct list_head *cprev,
                 struct list_head *cnext)
{
    struct list_head *first = list->cnext;
    struct list_head *last = list->cprev;

    first->cprev = cprev;
    cprev->cnext = first;

    last->cnext = cnext;
    cnext->cprev = last;
}

/**
 * list_splice - join two lists, this is designed for stacks
 * @list: the cnew list to add.
 * @head: the place to add it in the first list.
 */
static inline void list_splice(const struct list_head *list,
                struct list_head *head)
{
    if (!list_empty(list))
        __list_splice(list, head, head->cnext);
}

/**
 * list_splice_tail - join two lists, each list being a queue
 * @list: the cnew list to add.
 * @head: the place to add it in the first list.
 */
static inline void list_splice_tail(struct list_head *list,
                struct list_head *head)
{
    if (!list_empty(list))
        __list_splice(list, head->cprev, head);
}

/**
 * list_splice_init - join two lists and reinitialise the emptied list.
 * @list: the cnew list to add.
 * @head: the place to add it in the first list.
 *
 * The list at @list is reinitialised
 */
static inline void list_splice_init(struct list_head *list,
                    struct list_head *head)
{
    if (!list_empty(list)) {
        __list_splice(list, head, head->cnext);
        INIT_LIST_HEAD(list);
    }
}

/**
 * list_splice_tail_init - join two lists and reinitialise the emptied list
 * @list: the cnew list to add.
 * @head: the place to add it in the first list.
 *
 * Each of the lists is a queue.
 * The list at @list is reinitialised
 */
static inline void list_splice_tail_init(struct list_head *list,
                     struct list_head *head)
{
    if (!list_empty(list)) {
        __list_splice(list, head->cprev, head);
        INIT_LIST_HEAD(list);
    }
}

/**
 * list_entry - get the struct for this entry
 * @ptr:    the &struct list_head pointer.
 * @type:    the type of the struct this is embedded in.
 * @member:    the name of the list_struct within the struct.
 */
#define list_entry(ptr, type, member) \
    container_of(ptr, type, member)

/**
 * list_first_entry - get the first element from a list
 * @ptr:    the list head to take the element from.
 * @type:    the type of the struct this is embedded in.
 * @member:    the name of the list_struct within the struct.
 *
 * Note, that list is expected to be not empty.
 */
#define list_first_entry(ptr, type, member) \
    list_entry((ptr)->cnext, type, member)

/**
 * list_last_entry - get the last element from a list
 * @ptr:    the list head to take the element from.
 * @type:    the type of the struct this is embedded in.
 * @member:    the name of the list_head within the struct.
 *
 * Note, that list is expected to be not empty.
 */
#define list_last_entry(ptr, type, member) \
    list_entry((ptr)->cprev, type, member)

/**
 * list_next_entry - get the cnext element in list
 * @pos:    the type * to cursor
 * @member:    the name of the list_head within the struct.
 */
#define list_next_entry(pos, member) \
    list_entry((pos)->member.cnext, typeof(*(pos)), member)

/**
 * list_prev_entry - get the cprev element in list
 * @pos:    the type * to cursor
 * @member:    the name of the list_head within the struct.
 */
#define list_prev_entry(pos, member) \
    list_entry((pos)->member.cprev, typeof(*(pos)), member)

/**
 * list_for_each    -    iterate over a list
 * @pos:    the &struct list_head to use as a loop cursor.
 * @head:    the head for your list.
 */
#define list_for_each(pos, head) \
    for (pos = (head)->cnext; prefetch(pos->cnext), pos != (head); \
            pos = pos->cnext)

/**
 * __list_for_each    -    iterate over a list
 * @pos:    the &struct list_head to use as a loop cursor.
 * @head:    the head for your list.
 *
 * This variant differs from list_for_each() in that it's the
 * simplest possible list iteration code, no prefetching is done.
 * Use this for code that knows the list to be very short (empty
 * or 1 entry) most of the time.
 */
#define __list_for_each(pos, head) \
    for (pos = (head)->cnext; pos != (head); pos = pos->cnext)

/**
 * list_for_each_prev    -    iterate over a list backwards
 * @pos:    the &struct list_head to use as a loop cursor.
 * @head:    the head for your list.
 */
#define list_for_each_prev(pos, head) \
    for (pos = (head)->cprev; prefetch(pos->cprev), pos != (head); \
            pos = pos->cprev)

/**
 * list_for_each_safe - iterate over a list safe against removal of list entry
 * @pos:    the &struct list_head to use as a loop cursor.
 * @n:        another &struct list_head to use as temporary storage
 * @head:    the head for your list.
 */
#define list_for_each_safe(pos, n, head) \
    for (pos = (head)->cnext, n = pos->cnext; pos != (head); \
        pos = n, n = pos->cnext)

/**
 * list_for_each_prev_safe - iterate over a list backwards safe against removal of list entry
 * @pos:    the &struct list_head to use as a loop cursor.
 * @n:        another &struct list_head to use as temporary storage
 * @head:    the head for your list.
 */
#define list_for_each_prev_safe(pos, n, head) \
    for (pos = (head)->cprev, n = pos->cprev; \
         prefetch(pos->cprev), pos != (head); \
         pos = n, n = pos->cprev)

/**
 * list_for_each_entry    -    iterate over list of given type
 * @pos:    the type * to use as a loop cursor.
 * @head:    the head for your list.
 * @member:    the name of the list_struct within the struct.
 */
#define list_for_each_entry(pos, head, member)                \
    for (pos = list_entry((head)->cnext, typeof(*pos), member);    \
         prefetch(pos->member.cnext), &pos->member != (head);     \
         pos = list_entry(pos->member.cnext, typeof(*pos), member))

/**
 * list_for_each_entry_reverse - iterate backwards over list of given type.
 * @pos:    the type * to use as a loop cursor.
 * @head:    the head for your list.
 * @member:    the name of the list_struct within the struct.
 */
#define list_for_each_entry_reverse(pos, head, member)            \
    for (pos = list_entry((head)->cprev, typeof(*pos), member);    \
         prefetch(pos->member.cprev), &pos->member != (head);     \
         pos = list_entry(pos->member.cprev, typeof(*pos), member))

/**
 * list_prepare_entry - prepare a pos entry for use in list_for_each_entry_continue()
 * @pos:    the type * to use as a start point
 * @head:    the head of the list
 * @member:    the name of the list_struct within the struct.
 *
 * Prepares a pos entry for use as a start point in list_for_each_entry_continue().
 */
#define list_prepare_entry(pos, head, member) \
    ((pos) ? : list_entry(head, typeof(*pos), member))

/**
 * list_for_each_entry_continue - continue iteration over list of given type
 * @pos:    the type * to use as a loop cursor.
 * @head:    the head for your list.
 * @member:    the name of the list_struct within the struct.
 *
 * Continue to iterate over list of given type, continuing after
 * the current position.
 */
#define list_for_each_entry_continue(pos, head, member)         \
    for (pos = list_entry(pos->member.cnext, typeof(*pos), member);    \
         prefetch(pos->member.cnext), &pos->member != (head);    \
         pos = list_entry(pos->member.cnext, typeof(*pos), member))

/**
 * list_for_each_entry_continue_reverse - iterate backwards from the given point
 * @pos:    the type * to use as a loop cursor.
 * @head:    the head for your list.
 * @member:    the name of the list_struct within the struct.
 *
 * Start to iterate over list of given type backwards, continuing after
 * the current position.
 */
#define list_for_each_entry_continue_reverse(pos, head, member)        \
    for (pos = list_entry(pos->member.cprev, typeof(*pos), member);    \
         prefetch(pos->member.cprev), &pos->member != (head);    \
         pos = list_entry(pos->member.cprev, typeof(*pos), member))

/**
 * list_for_each_entry_from - iterate over list of given type from the current point
 * @pos:    the type * to use as a loop cursor.
 * @head:    the head for your list.
 * @member:    the name of the list_struct within the struct.
 *
 * Iterate over list of given type, continuing from current position.
 */
#define list_for_each_entry_from(pos, head, member)             \
    for (; prefetch(pos->member.cnext), &pos->member != (head);    \
         pos = list_entry(pos->member.cnext, typeof(*pos), member))

/**
 * list_for_each_entry_safe - iterate over list of given type safe against removal of list entry
 * @pos:    the type * to use as a loop cursor.
 * @n:        another type * to use as temporary storage
 * @head:    the head for your list.
 * @member:    the name of the list_struct within the struct.
 */
#define list_for_each_entry_safe(pos, n, head, member)            \
    for (pos = list_entry((head)->cnext, typeof(*pos), member),    \
        n = list_entry(pos->member.cnext, typeof(*pos), member);    \
         &pos->member != (head);                     \
         pos = n, n = list_entry(n->member.cnext, typeof(*n), member))

/**
 * list_for_each_entry_safe_continue - continue list iteration safe against removal
 * @pos:    the type * to use as a loop cursor.
 * @n:        another type * to use as temporary storage
 * @head:    the head for your list.
 * @member:    the name of the list_struct within the struct.
 *
 * Iterate over list of given type, continuing after current point,
 * safe against removal of list entry.
 */
#define list_for_each_entry_safe_continue(pos, n, head, member)         \
    for (pos = list_entry(pos->member.cnext, typeof(*pos), member),         \
        n = list_entry(pos->member.cnext, typeof(*pos), member);        \
         &pos->member != (head);                        \
         pos = n, n = list_entry(n->member.cnext, typeof(*n), member))

/**
 * list_for_each_entry_safe_from - iterate over list from current point safe against removal
 * @pos:    the type * to use as a loop cursor.
 * @n:        another type * to use as temporary storage
 * @head:    the head for your list.
 * @member:    the name of the list_struct within the struct.
 *
 * Iterate over list of given type from current point, safe against
 * removal of list entry.
 */
#define list_for_each_entry_safe_from(pos, n, head, member)             \
    for (n = list_entry(pos->member.cnext, typeof(*pos), member);        \
         &pos->member != (head);                        \
         pos = n, n = list_entry(n->member.cnext, typeof(*n), member))

/**
 * list_for_each_entry_safe_reverse - iterate backwards over list safe against removal
 * @pos:    the type * to use as a loop cursor.
 * @n:        another type * to use as temporary storage
 * @head:    the head for your list.
 * @member:    the name of the list_struct within the struct.
 *
 * Iterate backwards over list of given type, safe against removal
 * of list entry.
 */
#define list_for_each_entry_safe_reverse(pos, n, head, member)        \
    for (pos = list_entry((head)->cprev, typeof(*pos), member),    \
        n = list_entry(pos->member.cprev, typeof(*pos), member);    \
         &pos->member != (head);                     \
         pos = n, n = list_entry(n->member.cprev, typeof(*n), member))

/*
 * Double linked lists with a single pointer list head.
 * Mostly useful for hash tables where the two pointer list head is
 * too wasteful.
 * You lose the ability to access the tail in O(1).
 */

struct hlist_head {
    struct hlist_node *first;
};

struct hlist_node {
    struct hlist_node *cnext, **pcprev;
};

#define HLIST_HEAD_INIT { .first = NULL }
#define HLIST_HEAD(name) struct hlist_head name = {  .first = NULL }
#define INIT_HLIST_HEAD(ptr) ((ptr)->first = NULL)
static inline void INIT_HLIST_NODE(struct hlist_node *h)
{
    h->cnext = NULL;
    h->pcprev = NULL;
}

static inline int hlist_unhashed(const struct hlist_node *h)
{
    return !h->pcprev;
}

static inline int hlist_empty(const struct hlist_head *h)
{
    return !h->first;
}

static inline void __hlist_del(struct hlist_node *n)
{
    struct hlist_node *cnext = n->cnext;
    struct hlist_node **pcprev = n->pcprev;
    *pcprev = cnext;
    if (cnext)
        cnext->pcprev = pcprev;
}

static inline void hlist_del(struct hlist_node *n)
{
    __hlist_del(n);
    n->cnext = (struct hlist_node *)LIST_POISON1;
    n->pcprev = (struct hlist_node **)LIST_POISON2;
}

static inline void hlist_del_init(struct hlist_node *n)
{
    if (!hlist_unhashed(n)) {
        __hlist_del(n);
        INIT_HLIST_NODE(n);
    }
}

static inline void hlist_add_head(struct hlist_node *n, struct hlist_head *h)
{
    struct hlist_node *first = h->first;
    n->cnext = first;
    if (first)
        first->pcprev = &n->cnext;
    h->first = n;
    n->pcprev = &h->first;
}

/* cnext must be != NULL */
static inline void hlist_add_before(struct hlist_node *n,
                    struct hlist_node *cnext)
{
    n->pcprev = cnext->pcprev;
    n->cnext = cnext;
    cnext->pcprev = &n->cnext;
    *(n->pcprev) = n;
}

static inline void hlist_add_after(struct hlist_node *n,
                    struct hlist_node *cnext)
{
    cnext->cnext = n->cnext;
    n->cnext = cnext;
    cnext->pcprev = &n->cnext;

    if(cnext->cnext)
        cnext->cnext->pcprev  = &cnext->cnext;
}

/*
 * Move a list from one list head to another. Fixup the pcprev
 * reference of the first entry if it exists.
 */
static inline void hlist_move_list(struct hlist_head *old,
                   struct hlist_head *cnew)
{
    cnew->first = old->first;
    if (cnew->first)
        cnew->first->pcprev = &cnew->first;
    old->first = NULL;
}

#define hlist_entry(ptr, type, member) container_of(ptr,type,member)

#define hlist_for_each(pos, head) \
    for (pos = (head)->first; pos && ({ prefetch(pos->cnext); 1; }); \
         pos = pos->cnext)

#define hlist_for_each_safe(pos, n, head) \
    for (pos = (head)->first; pos && ({ n = pos->cnext; 1; }); \
         pos = n)

/**
 * hlist_for_each_entry    - iterate over list of given type
 * @tpos:    the type * to use as a loop cursor.
 * @pos:    the &struct hlist_node to use as a loop cursor.
 * @head:    the head for your list.
 * @member:    the name of the hlist_node within the struct.
 */
#define hlist_for_each_entry(tpos, pos, head, member)             \
    for (pos = (head)->first;                     \
         pos && ({ prefetch(pos->cnext); 1;}) &&             \
        ({ tpos = hlist_entry(pos, typeof(*tpos), member); 1;}); \
         pos = pos->cnext)

/**
 * hlist_for_each_entry_continue - iterate over a hlist continuing after current point
 * @tpos:    the type * to use as a loop cursor.
 * @pos:    the &struct hlist_node to use as a loop cursor.
 * @member:    the name of the hlist_node within the struct.
 */
#define hlist_for_each_entry_continue(tpos, pos, member)         \
    for (pos = (pos)->cnext;                         \
         pos && ({ prefetch(pos->cnext); 1;}) &&             \
        ({ tpos = hlist_entry(pos, typeof(*tpos), member); 1;}); \
         pos = pos->cnext)

/**
 * hlist_for_each_entry_from - iterate over a hlist continuing from current point
 * @tpos:    the type * to use as a loop cursor.
 * @pos:    the &struct hlist_node to use as a loop cursor.
 * @member:    the name of the hlist_node within the struct.
 */
#define hlist_for_each_entry_from(tpos, pos, member)             \
    for (; pos && ({ prefetch(pos->cnext); 1;}) &&             \
        ({ tpos = hlist_entry(pos, typeof(*tpos), member); 1;}); \
         pos = pos->cnext)

/**
 * hlist_for_each_entry_safe - iterate over list of given type safe against removal of list entry
 * @tpos:    the type * to use as a loop cursor.
 * @pos:    the &struct hlist_node to use as a loop cursor.
 * @n:        another &struct hlist_node to use as temporary storage
 * @head:    the head for your list.
 * @member:    the name of the hlist_node within the struct.
 */
#define hlist_for_each_entry_safe(tpos, pos, n, head, member)          \
    for (pos = (head)->first;                     \
         pos && ({ n = pos->cnext; 1; }) &&                  \
        ({ tpos = hlist_entry(pos, typeof(*tpos), member); 1;}); \
         pos = n)

#ifdef __cplusplus
}
#endif


#endif

