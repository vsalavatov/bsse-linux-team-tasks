#include <stdbool.h>
#include "even.h"

bool is_odd(unsigned int x) {
  if (x == 0) return false;
  return is_even(x - 1);
}
