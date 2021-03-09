#include <stdbool.h>
#include "odd.h"

int main() {
  bool pass = is_odd(5) && !is_odd(6);
  return !pass;
}
