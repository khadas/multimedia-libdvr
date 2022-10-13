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
 * sometimes we already know the c_next/c_prev entries and we can
 * generate better code by using them directly rather than
 * using the generic single-entry routines.
 */

struct list_head {
    struct list_head *c_next, *c_prev;
};

#define LIST_HEAD_INIT(name) { &(name), &(name) }

#define LIST_HEAD(name) \
    struct list_head name = LIST_HEAD_INIT(name)

static inline void INIT_LIST_HEAD(struct list_head *list)
{
    list->c_next = list;
    list->c_prev = list;
}

/*
 * Insert a c_new entry between two known consecutive entries.
 *
 * This is only for internal list manipulation where we know
 * the c_prev/c_next entries already!
 */
#ifndef CONFIG_DEBUG_LIST
static inline void __list_add(struct list_head *c_new,
                  struct list_head *c_prev,
                  struct list_head *c_next)
{
    c_next->c_prev = c_new;
    c_new->c_next = c_next;
    c_new->c_prev = c_prev;
    c_prev->c_next = c_new;
}
#else
extern void __list_add(struct list_head *c_new,
                  struct list_head *prev,
                  struct list_head *next);
#endif

/**
 * list_add - add a c_new entry
 * @c_new: c_new entry to be added
 * @head: list head to add it after
 *
 * Insert a c_new entry after the specified head.
 * This is good for implementing stacks.
 */
static inline void list_add(struct list_head *c_new, struct list_head *head)
{
    __list_add((struct list_head*)c_new, head, head->c_next);
}


/**
 * list_add_tail - add a c_new entry
 * @c_new: c_new entry to be added
 * @head: list head to add it before
 *
 * Insert a c_new entry before the specified head.
 * This is useful for implementing queues.
 */
static inline void list_add_tail(struct list_head *c_new, struct list_head *head)
{
    __list_add(c_new, head->c_prev, head);
}

/*
 * Delete a list entry by making the c_prev/c_next entries
 * point to each other.
 *
 * This is only for internal list manipulation where we know
 * the c_prev/c_next entries already!
 */
static inline void __list_del(struct list_head * c_prev, struct list_head * c_next)
{
    c_next->c_prev = c_prev;
    c_prev->c_next = c_next;
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
    __list_del(entry->c_prev, entry->c_next);
    entry->c_next = (struct list_head *)LIST_POISON1;
    entry->c_prev = (struct list_head *)LIST_POISON2;
}
#else
extern void list_del(struct list_head *entry);
#endif

/**
 * list_replace - replace old entry by c_new one
 * @old : the element to be replaced
 * @c_new : the c_new element to insert
 *
 * If @old was empty, it will be overwritten.
 */
static inline void list_replace(struct list_head *old,
                struct list_head *c_new)
{
    c_new->c_next = old->c_next;
    c_new->c_next->c_prev = c_new;
    c_new->c_prev = old->c_prev;
    c_new->c_prev->c_next = c_new;
}

static inline void list_replace_init(struct list_head *old,
                    struct list_head *c_new)
{
    list_replace(old, c_new);
    INIT_LIST_HEAD(old);
}

/**
 * list_del_init - deletes entry from list and reinitialize it.
 * @entry: the element to delete from the list.
 */
static inline void list_del_init(struct list_head *entry)
{
    __list_del(entry->c_prev, entry->c_next);
    INIT_LIST_HEAD(entry);
}

/**
 * list_move - delete from one list and add as another's head
 * @list: the entry to move
 * @head: the head that will precede our entry
 */
static inline void list_move(struct list_head *list, struct list_head *head)
{
    __list_del(list->c_prev, list->c_next);
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
    __list_del(list->c_prev, list->c_next);
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
    return list->c_next == head;
}

/**
 * list_empty - tests whether a list is empty
 * @head: the list to test.
 */
static inline int list_empty(const struct list_head *head)
{
    return head->c_next == head;
}

/**
 * list_empty_careful - tests whether a list is empty and not being modified
 * @head: the list to test
 *
 * Description:
 * tests whether a list is empty _and_ checks that no other CPU might be
 * in the process of modifying either member (c_next or c_prev)
 *
 * NOTE: using list_empty_careful() without synchronization
 * can only be safe if the only activity that can happen
 * to the list entry is list_del_init(). Eg. it cannot be used
 * if another CPU could re-list_add() it.
 */
static inline int list_empty_careful(const struct list_head *head)
{
    struct list_head *c_next = head->c_next;
    return (c_next == head) && (c_next == head->c_prev);
}

/**
 * list_rotate_left - rotate the list to the left
 * @head: the head of the list
 */
static inline void list_rotate_left(struct list_head *head)
{
    struct list_head *first;

    if (!list_empty(head)) {
        first = head->c_next;
        list_move_tail(first, head);
    }
}

/**
 * list_is_singular - tests whether a list has just one entry.
 * @head: the list to test.
 */
static inline int list_is_singular(const struct list_head *head)
{
    return !list_empty(head) && (head->c_next == head->c_prev);
}

static inline void __list_cut_position(struct list_head *list,
        struct list_head *head, struct list_head *entry)
{
    struct list_head *c_new_first = entry->c_next;
    list->c_next = head->c_next;
    list->c_next->c_prev = list;
    list->c_prev = entry;
    entry->c_next = list;
    head->c_next = c_new_first;
    c_new_first->c_prev = head;
}

/**
 * list_cut_position - cut a list into two
 * @list: a c_new list to add all removed entries
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
        (head->c_next != entry && head != entry))
        return;
    if (entry == head)
        INIT_LIST_HEAD(list);
    else
        __list_cut_position(list, head, entry);
}

static inline void __list_splice(const struct list_head *list,
                 struct list_head *c_prev,
                 struct list_head *c_next)
{
    struct list_head *first = list->c_next;
    struct list_head *last = list->c_prev;

    first->c_prev = c_prev;
    c_prev->c_next = first;

    last->c_next = c_next;
    c_next->c_prev = last;
}

/**
 * list_splice - join two lists, this is designed for stacks
 * @list: the c_new list to add.
 * @head: the place to add it in the first list.
 */
static inline void list_splice(const struct list_head *list,
                struct list_head *head)
{
    if (!list_empty(list))
        __list_splice(list, head, head->c_next);
}

/**
 * list_splice_tail - join two lists, each list being a queue
 * @list: the c_new list to add.
 * @head: the place to add it in the first list.
 */
static inline void list_splice_tail(struct list_head *list,
                struct list_head *head)
{
    if (!list_empty(list))
        __list_splice(list, head->c_prev, head);
}

/**
 * list_splice_init - join two lists and reinitialise the emptied list.
 * @list: the c_new list to add.
 * @head: the place to add it in the first list.
 *
 * The list at @list is reinitialised
 */
static inline void list_splice_init(struct list_head *list,
                    struct list_head *head)
{
    if (!list_empty(list)) {
        __list_splice(list, head, head->c_next);
        INIT_LIST_HEAD(list);
    }
}

/**
 * list_splice_tail_init - join two lists and reinitialise the emptied list
 * @list: the c_new list to add.
 * @head: the place to add it in the first list.
 *
 * Each of the lists is a queue.
 * The list at @list is reinitialised
 */
static inline void list_splice_tail_init(struct list_head *list,
                     struct list_head *head)
{
    if (!list_empty(list)) {
        __list_splice(list, head->c_prev, head);
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
    list_entry((ptr)->c_next, type, member)

/**
 * list_last_entry - get the last element from a list
 * @ptr:    the list head to take the element from.
 * @type:    the type of the struct this is embedded in.
 * @member:    the name of the list_head within the struct.
 *
 * Note, that list is expected to be not empty.
 */
#define list_last_entry(ptr, type, member) \
    list_entry((ptr)->c_prev, type, member)

/**
 * list_next_entry - get the c_next element in list
 * @pos:    the type * to cursor
 * @member:    the name of the list_head within the struct.
 */
#define list_next_entry(pos, member) \
    list_entry((pos)->member.c_next, typeof(*(pos)), member)

/**
 * list_prev_entry - get the c_prev element in list
 * @pos:    the type * to cursor
 * @member:    the name of the list_head within the struct.
 */
#define list_prev_entry(pos, member) \
    list_entry((pos)->member.c_prev, typeof(*(pos)), member)

/**
 * list_for_each    -    iterate over a list
 * @pos:    the &struct list_head to use as a loop cursor.
 * @head:    the head for your list.
 */
#define list_for_each(pos, head) \
    for (pos = (head)->c_next; prefetch(pos->c_next), pos != (head); \
            pos = pos->c_next)

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
    for (pos = (head)->c_next; pos != (head); pos = pos->c_next)

/**
 * list_for_each_prev    -    iterate over a list backwards
 * @pos:    the &struct list_head to use as a loop cursor.
 * @head:    the head for your list.
 */
#define list_for_each_prev(pos, head) \
    for (pos = (head)->c_prev; prefetch(pos->c_prev), pos != (head); \
            pos = pos->c_prev)

/**
 * list_for_each_safe - iterate over a list safe against removal of list entry
 * @pos:    the &struct list_head to use as a loop cursor.
 * @n:        another &struct list_head to use as temporary storage
 * @head:    the head for your list.
 */
#define list_for_each_safe(pos, n, head) \
    for (pos = (head)->c_next, n = pos->c_next; pos != (head); \
        pos = n, n = pos->c_next)

/**
 * list_for_each_prev_safe - iterate over a list backwards safe against removal of list entry
 * @pos:    the &struct list_head to use as a loop cursor.
 * @n:        another &struct list_head to use as temporary storage
 * @head:    the head for your list.
 */
#define list_for_each_prev_safe(pos, n, head) \
    for (pos = (head)->c_prev, n = pos->c_prev; \
         prefetch(pos->c_prev), pos != (head); \
         pos = n, n = pos->c_prev)

/**
 * list_for_each_entry    -    iterate over list of given type
 * @pos:    the type * to use as a loop cursor.
 * @head:    the head for your list.
 * @member:    the name of the list_struct within the struct.
 */
#define list_for_each_entry(pos, head, member)                \
    for (pos = list_entry((head)->c_next, typeof(*pos), member);    \
         prefetch(pos->member.c_next), &pos->member != (head);     \
         pos = list_entry(pos->member.c_next, typeof(*pos), member))

/**
 * list_for_each_entry_reverse - iterate backwards over list of given type.
 * @pos:    the type * to use as a loop cursor.
 * @head:    the head for your list.
 * @member:    the name of the list_struct within the struct.
 */
#define list_for_each_entry_reverse(pos, head, member)            \
    for (pos = list_entry((head)->c_prev, typeof(*pos), member);    \
         prefetch(pos->member.c_prev), &pos->member != (head);     \
         pos = list_entry(pos->member.c_prev, typeof(*pos), member))

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
    for (pos = list_entry(pos->member.c_next, typeof(*pos), member);    \
         prefetch(pos->member.c_next), &pos->member != (head);    \
         pos = list_entry(pos->member.c_next, typeof(*pos), member))

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
    for (pos = list_entry(pos->member.c_prev, typeof(*pos), member);    \
         prefetch(pos->member.c_prev), &pos->member != (head);    \
         pos = list_entry(pos->member.c_prev, typeof(*pos), member))

/**
 * list_for_each_entry_from - iterate over list of given type from the current point
 * @pos:    the type * to use as a loop cursor.
 * @head:    the head for your list.
 * @member:    the name of the list_struct within the struct.
 *
 * Iterate over list of given type, continuing from current position.
 */
#define list_for_each_entry_from(pos, head, member)             \
    for (; prefetch(pos->member.c_next), &pos->member != (head);    \
         pos = list_entry(pos->member.c_next, typeof(*pos), member))

/**
 * list_for_each_entry_safe - iterate over list of given type safe against removal of list entry
 * @pos:    the type * to use as a loop cursor.
 * @n:        another type * to use as temporary storage
 * @head:    the head for your list.
 * @member:    the name of the list_struct within the struct.
 */
#define list_for_each_entry_safe(pos, n, head, member)            \
    for (pos = list_entry((head)->c_next, typeof(*pos), member),    \
        n = list_entry(pos->member.c_next, typeof(*pos), member);    \
         &pos->member != (head);                     \
         pos = n, n = list_entry(n->member.c_next, typeof(*n), member))

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
    for (pos = list_entry(pos->member.c_next, typeof(*pos), member),         \
        n = list_entry(pos->member.c_next, typeof(*pos), member);        \
         &pos->member != (head);                        \
         pos = n, n = list_entry(n->member.c_next, typeof(*n), member))

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
    for (n = list_entry(pos->member.c_next, typeof(*pos), member);        \
         &pos->member != (head);                        \
         pos = n, n = list_entry(n->member.c_next, typeof(*n), member))

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
    for (pos = list_entry((head)->c_prev, typeof(*pos), member),    \
        n = list_entry(pos->member.c_prev, typeof(*pos), member);    \
         &pos->member != (head);                     \
         pos = n, n = list_entry(n->member.c_prev, typeof(*n), member))

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
    struct hlist_node *c_next, **pc_prev;
};

#define HLIST_HEAD_INIT { .first = NULL }
#define HLIST_HEAD(name) struct hlist_head name = {  .first = NULL }
#define INIT_HLIST_HEAD(ptr) ((ptr)->first = NULL)
static inline void INIT_HLIST_NODE(struct hlist_node *h)
{
    h->c_next = NULL;
    h->pc_prev = NULL;
}

static inline int hlist_unhashed(const struct hlist_node *h)
{
    return !h->pc_prev;
}

static inline int hlist_empty(const struct hlist_head *h)
{
    return !h->first;
}

static inline void __hlist_del(struct hlist_node *n)
{
    struct hlist_node *c_next = n->c_next;
    struct hlist_node **pc_prev = n->pc_prev;
    *pc_prev = c_next;
    if (c_next)
        c_next->pc_prev = pc_prev;
}

static inline void hlist_del(struct hlist_node *n)
{
    __hlist_del(n);
    n->c_next = (struct hlist_node *)LIST_POISON1;
    n->pc_prev = (struct hlist_node **)LIST_POISON2;
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
    n->c_next = first;
    if (first)
        first->pc_prev = &n->c_next;
    h->first = n;
    n->pc_prev = &h->first;
}

/* c_next must be != NULL */
static inline void hlist_add_before(struct hlist_node *n,
                    struct hlist_node *c_next)
{
    n->pc_prev = c_next->pc_prev;
    n->c_next = c_next;
    c_next->pc_prev = &n->c_next;
    *(n->pc_prev) = n;
}

static inline void hlist_add_after(struct hlist_node *n,
                    struct hlist_node *c_next)
{
    c_next->c_next = n->c_next;
    n->c_next = c_next;
    c_next->pc_prev = &n->c_next;

    if(c_next->c_next)
        c_next->c_next->pc_prev  = &c_next->c_next;
}

/*
 * Move a list from one list head to another. Fixup the pc_prev
 * reference of the first entry if it exists.
 */
static inline void hlist_move_list(struct hlist_head *old,
                   struct hlist_head *c_new)
{
    c_new->first = old->first;
    if (c_new->first)
        c_new->first->pc_prev = &c_new->first;
    old->first = NULL;
}

#define hlist_entry(ptr, type, member) container_of(ptr,type,member)

#define hlist_for_each(pos, head) \
    for (pos = (head)->first; pos && ({ prefetch(pos->c_next); 1; }); \
         pos = pos->c_next)

#define hlist_for_each_safe(pos, n, head) \
    for (pos = (head)->first; pos && ({ n = pos->c_next; 1; }); \
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
         pos && ({ prefetch(pos->c_next); 1;}) &&             \
        ({ tpos = hlist_entry(pos, typeof(*tpos), member); 1;}); \
         pos = pos->c_next)

/**
 * hlist_for_each_entry_continue - iterate over a hlist continuing after current point
 * @tpos:    the type * to use as a loop cursor.
 * @pos:    the &struct hlist_node to use as a loop cursor.
 * @member:    the name of the hlist_node within the struct.
 */
#define hlist_for_each_entry_continue(tpos, pos, member)         \
    for (pos = (pos)->c_next;                         \
         pos && ({ prefetch(pos->c_next); 1;}) &&             \
        ({ tpos = hlist_entry(pos, typeof(*tpos), member); 1;}); \
         pos = pos->c_next)

/**
 * hlist_for_each_entry_from - iterate over a hlist continuing from current point
 * @tpos:    the type * to use as a loop cursor.
 * @pos:    the &struct hlist_node to use as a loop cursor.
 * @member:    the name of the hlist_node within the struct.
 */
#define hlist_for_each_entry_from(tpos, pos, member)             \
    for (; pos && ({ prefetch(pos->c_next); 1;}) &&             \
        ({ tpos = hlist_entry(pos, typeof(*tpos), member); 1;}); \
         pos = pos->c_next)

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
         pos && ({ n = pos->c_next; 1; }) &&                  \
        ({ tpos = hlist_entry(pos, typeof(*tpos), member); 1;}); \
         pos = n)

#ifdef __cplusplus
}
#endif


#endif

