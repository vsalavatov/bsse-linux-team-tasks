#include "bablib.h"

int main() {
  BabShelf shelf(1);
  std::map<int, std::unique_ptr<int>> m;
  m.emplace(1, nullptr);
}
