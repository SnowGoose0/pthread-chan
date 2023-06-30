#include <stdio.h>
#include <stdlib.h>

#include "lock.h"

bool compare_and_swap(Lock* c_lock, Lock expected, Lock new_val) {
  return __sync_bool_compare_and_swap(c_lock, expected, new_val);
}
