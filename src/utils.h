#include <string>

template <char... Chars>
class HasString {
public:
  static std::string getString() {
    return {Chars...};
  }
};
