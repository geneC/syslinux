#undef container_of
/*
 * The container_of construct: if p is a pointer to member m of
 * container class c, then return a pointer to the container of which
 * *p is a member.
 */
#define container_of(p, c, m) ((c *)((char *)(p) - offsetof(c,m)))

#include <../../../com32/include/linux/list.h>
