#ifndef LOCK
#define LOCK

#include <stdbool.h>

typedef int Lock; 

bool compare_and_swap(Lock* c_lock, Lock expected, Lock new_val);

#endif
