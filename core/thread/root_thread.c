#include "thread.h"

struct thread __root_thread = {
    .list = { .next = &__root_thread.list, .prev = &__root_thread.list },
    .blocked = NULL,
    .prio = 0,
};

struct thread *__current = &__root_thread;
