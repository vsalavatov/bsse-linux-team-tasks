#include <functional>
#include <ranges>
#include <variant>
#include <map>
#include <utility>
#include <concepts>
#include <optional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "utils.h"

using Index = std::size_t;

template <typename T>
concept ConstructibleFromIndex = requires() {
  T(std::declval<Index>());
};

class BabEntity {
public:
  virtual ~BabEntity() = default;
};

class BabBook : public BabEntity {
public:
  BabBook(Index index) : index{index} {}
private:
  Index index;
};

class BabDirectory : public BabEntity {
public:
  virtual std::vector<std::string> listMembers() = 0;
  virtual BabEntity *lookupMember(std::string_view memberName) = 0;
  virtual ~BabDirectory() = default;
};

template <Index N, ConstructibleFromIndex T, char... MemberName>
class BabIndexedDirectoryWithMembers : public BabDirectory {
public:
  BabIndexedDirectoryWithMembers(Index index) : index{index} {}

  virtual std::vector<std::string> listMembers() override { 
    auto &&r = std::views::keys(nameToMember()) | std::views::common;
    return {r.begin(), r.end()};
  }

  virtual BabEntity *lookupMember(std::string_view memberName) override { 
    if (auto it = nameToMember().find(memberName); it != nameToMember().end()) {
      return &it->second;
    } else {
      return nullptr;
    }
  }
protected:
  // See https://stackoverflow.com/a/35525806/6508598
  using NameToMemberMap = std::map<std::string, T, std::less<>>;
  NameToMemberMap& nameToMember() {
    if (!maybeNameToMember) {
      initNameToMember();
    }
    return maybeNameToMember.value();
  }

  // do we need virtual here?
  virtual void initNameToMember() {
    maybeNameToMember = NameToMemberMap{};
    auto& map = maybeNameToMember.value();
    for (Index i = 0; i < N; ++i) {
      auto name = std::string{MemberName...} + std::to_string(i);
      map.emplace(name, T(index * N + i));
    }
  }

protected:
  std::optional<NameToMemberMap> maybeNameToMember;
private:
  Index index;
};


constexpr Index ROOMS = 1ULL << 54;
constexpr Index BOOKCASES_IN_ROOM = 4;
constexpr Index SHELVES_IN_BOOKCASE = 5;
constexpr Index BOOKS_IN_SHELF = 32;

using BabShelf = BabIndexedDirectoryWithMembers<BOOKS_IN_SHELF, BabBook, 'b', 'o', 'o', 'k'>;
using BabBookcase = BabIndexedDirectoryWithMembers<SHELVES_IN_BOOKCASE, BabShelf, 's', 'h', 'e', 'l', 'f'>;

using BabRoomBase = BabIndexedDirectoryWithMembers<BOOKCASES_IN_ROOM, BabBookcase, 'b', 'o', 'o', 'k', 'c', 'a', 's', 'e'>;

class BabRoom : public BabRoomBase {
protected:
  virtual void initNameToMember() override;
};
