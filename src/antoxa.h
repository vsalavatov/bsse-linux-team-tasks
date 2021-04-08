#include <string>
#include <sstream>

using namespace std::string_literals;

constexpr std::array LEVEL_NAMES = { "room"s, "bookcase"s, "shelf"s, "book"s };
constexpr auto LEVELS = std::size(LEVEL_NAMES);
constexpr std::array<size_t, LEVELS> LEVEL_SIZES = { 1 << 54, 4, 5, 32 };

constexpr auto DESK_NAME = "desk"s;

// constexpr size_t ROOMS = 1 << 54;

class BabEntity {
public:
    virtual BabEntity& resolvePart(std::string_view) = 0;
    virtual ~BabEntity() {}
private:
} 

struct Index {
    int level;
    size_t index;
};

class Library {
public:
private:
    Index pathToIndex(std::string_view path) {
        int currentLevel = -1;
        std::stringstream ss{path};
        std::getline(ss, )
    }
};

class Room {
public:
private:
};

