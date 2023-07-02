#include <stdio.h>
#include <stdlib.h>

#include "lock.h"

void lock(Lock* c_lock) {
  while (__sync_lock_test_and_set(&c_lock, 1))
    ;
}

void unlock(Lock* c_lock) {
  __sync_lock_release(c_lock);
}
