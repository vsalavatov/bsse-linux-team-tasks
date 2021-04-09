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

using Index = std::size_t;

constexpr Index BOOKS_IN_SHELF = 32;

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

template <Index N, std::string MakeNameFromIndex(Index), ConstructibleFromIndex T>
class BabIndexedDirectoryWithMembers : public BabDirectory {
public:
  /* BabIndexedDirectoryWithMembers(Index idx) : BabIndexedDirectoryWithMembers(idx, std::make_index_sequence<N>{}) {} */
  BabIndexedDirectoryWithMembers(Index index) {
    for (Index i = 0; i < N; ++i) {
      nameToMember.emplace(MakeNameFromIndex(i), LazyMember{index * N + i});
    }
  }

  virtual std::vector<std::string> listMembers() override { 
    auto &&r = std::views::keys(nameToMember) | std::views::common;
    return {r.begin(), r.end()};
  }
  virtual BabEntity *lookupMember(std::string_view memberName) override { 
    if (auto it = nameToMember.find(memberName); it != nameToMember.end()) {
      return &accessMember(it->second);
    } else {
      return nullptr;
    }
  }
protected:
  using LazyMember = std::variant<Index, std::unique_ptr<T>>;

  T& accessMember(LazyMember& member) {
    if (auto idxPtr = std::get_if<Index>(&member)) {
      member = std::make_unique<T>(*idxPtr);
    }
    return *std::get<std::unique_ptr<T>>(member);
  }
private:
  /* template <Index... I> */
  /* BabIndexedDirectoryWithMembers(Index idx, std::index_sequence<I...>) : */
  /*   nameToMember(makeMap(std::initializer_list{std::make_pair(MakeNameFromIndex(I), LazyMember{idx * N + I})...})) { */
  /*   std::pair<std::string, LazyMember> p = {"kek", LazyMember{std::size_t(0)}}; */
  /*   auto p1 = std::move(p); */
  /* } */
    /* nameToMember{std::move(std::make_pair(MakeNameFromIndex(0), std::move(LazyMember{idx * N + 0})))} {} */
    /* nameToMember{} {} */
  /* std::map<std::string, LazyMember, std::less<void>> makeMap(std::initializer_list<std::pair<std::string, LazyMember>>&& initList) { */
    /* auto v = std::vector{std::make_move_iterator(initList.begin()), std::make_move_iterator(initList.end())}; */
    /* std::map<std::string, LazyMember, std::less<void>> m; */
    /* for (auto&& [k, v] : */ 
    /* std::map<std::string, LazyMember, std::less<void>> m(std::make_move_iterator(v.begin()), std::make_move_iterator(v.end())); */
    /* std::map<std::string, LazyMember> m; */
    /* m.emplace(MakeNameFromIndex(0), std::move(LazyMember{0 * N + 0})); */
    /* std::map<std::string, LazyMember> m(std::make_move_iterator(v.begin()), std::make_move_iterator(v.end())); */
    /* auto m = std::map{std::make_move_iterator(initList.begin()), std::make_move_iterator(initList.end())}; */
    /* std::map<std::string, LazyMember> m; */
    /* for (auto& [k, v] : initList) { */
    /*   m.emplace(k, std::move(v)); */
    /* } */
    /* std::ranges::move(initList, m.begin()); */
    /* return std::map<std::string, LazyMember, std::less<void>>{std::make_move_iterator(std::begin(initList)), std::make_move_iterator(std::end(initList))}; */
  /* } */

protected:
  // See https://stackoverflow.com/a/35525806/6508598
  std::map<std::string, LazyMember, std::less<>> nameToMember;
};

std::string nameByPrefix(std::string prefix, Index idx) {
  return prefix + std::to_string(idx);
}

std::string nameBook(Index idx) {
  return nameByPrefix("book", idx);
}

using BabShelf = BabIndexedDirectoryWithMembers<BOOKS_IN_SHELF, nameBook, BabBook>;



