#include "bablib.h"

#include <unordered_map>

namespace {
  std::unordered_map<Index, BabRoom> rooms;
}

void BabRoom::initNameToMember() {
  BabRoomBase::initNameToMember();
  
}


int main() {
  BabShelf shelf(1);
  std::map<int, std::unique_ptr<int>> m;
  m.emplace(1, nullptr);
}
