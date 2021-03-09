#include <stdbool.h>
#include "even.h"

int main() {
  bool pass = is_even(4) && !is_even(5);
  return !pass;
}
