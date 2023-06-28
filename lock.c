#include <stdlib.h>

#include "lock.h"

Lock TAS(Lock* l) {
  Lock ret_l = *l;
  *l = 1;
  return ret_l;
}


