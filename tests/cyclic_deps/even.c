#include <stdbool.h>
#include "odd.h"

bool is_even(unsigned int x) {
  if (x == 0) return true;
  return is_odd(x - 1);
}
