#ifndef LOCK
#define LOCK

#include <stdbool.h>

typedef int Lock;

void lock(Lock* c_lock);

void unlock(Lock* c_lock);

#endif
