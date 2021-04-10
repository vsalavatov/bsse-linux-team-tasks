#include "bablib.h"

#include <algorithm>
#include <ranges>
#include <stdexcept>

BabLibException::BabLibException(const std::string &msg) : std::runtime_error(msg) {}


/**
 * visitors and utils
 */

static constexpr auto lTRUE = [](auto) { return true; };
static constexpr auto lFALSE = [](auto) { return false; };

bool isContainer(LibraryEntity entity) {
    return LibraryEntityVisitor<bool>{
        .roomHandler = lTRUE,
        .bookcaseHandler = lTRUE,
        .shelfHandler = lTRUE,
        .bookHandler = lFALSE,
        .tableHandler = lTRUE,
        .basketHandler = lTRUE,
        .noteHandler = lFALSE
    }.visit(entity);
}

std::vector<std::string> getContainedEntities(LibraryEntity entity) {
    return LibraryEntityVisitor<std::vector<std::string>>{
        .roomHandler = [](Room* room) {
            std::vector<std::string> names;
            names.push_back(room->previousRoom()->getName());
            names.push_back(room->nextRoom()->getName());
            for (auto bookcase: room->getBookcases()) {
                names.push_back(bookcase->getName());
            }
            return names;
        },
        .bookcaseHandler = [](Bookcase* bookcase) {
            std::vector<std::string> names;
            for (const auto& s : bookcase->getShelves())
                names.push_back(s->getName());
            return names;
        }
    }.visit(entity);
}

std::string_view extractFirstToken(std::string_view &path) {
    auto pos = path.find('/');
    if (pos == std::string::npos) pos = path.length();
    auto token = path.substr(0, pos);
    path = path.substr(std::min(path.length(), pos + 1));
    return token;
}

std::string_view removeLastToken(std::string_view path) {
    if (path == "/") return path;
    auto pos = path.rfind('/');
    if (pos == std::string::npos) throw BabLibException("expected path to have at least one /");
    return path.substr(0, pos);
}


/**
 * Library
 */

Library::Library(uint64_t roomCount, uint64_t rootId)
    : roomCount_{roomCount}, rootId_{rootId}, roomStorage_{} {
}

Room* Library::getRootRoom() {
    return getRoom(rootId_);
}

LibraryEntity Library::resolve(std::string_view path) {
    if (path.length() == 0 || path[0] != '/') {
        throw BabLibException("expected path to be absolute");
    }
    path = path.substr(1);

    auto resolver = LibraryEntityVisitor<LibraryEntity>{
        .roomHandler = [&path](Room* room) -> LibraryEntity {
            if (path.length() == 0) return room;
            auto firstName = extractFirstToken(path);
            if (firstName == room->previousRoom()->getName()) return room->previousRoom();
            if (firstName == room->nextRoom()->getName()) return room->nextRoom();
            for (auto bc : room->getBookcases()) {
                if (firstName == bc->getName())
                    return bc;
            }
            throw BabLibException("no such room or bookcase");
        },
        .bookcaseHandler = [&path](Bookcase* bc) -> LibraryEntity {
            if (path.length() == 0) return bc;
            auto firstName = extractFirstToken(path);
            for (auto shelf : bc->getShelves()) {
                if (firstName == shelf->getName())
                    return shelf;
            }
            throw BabLibException("no such room or shelf");
        }
    };

    LibraryEntity current = getRootRoom();
    while (path.length() > 0) {
        current = resolver.visit(current);
    }
    return current;
}

Room* Library::getRoom(uint64_t id) {
    if (!roomStorage_.contains(id))
        roomStorage_[id] = std::make_unique<Room>(this, id);
    return roomStorage_[id].get();
}

/**
 * Room
 */

Room::Room(Library* lib, uint64_t id): library_{lib}, id_{id} {
    for (std::size_t i = 0; i < BOOKCASES_COUNT; i++) {
        uint64_t bcId = id_ * 123124 + 5232 + i;
        bookcases_[i] = std::make_unique<Bookcase>(this, "bookcase_" + std::to_string(bcId));
    }
}
 
std::string Room::getName() const {
    return "room_" + std::to_string(id_);
};

Room* Room::previousRoom() {
    return library_->getRoom((id_ - 1 + library_->roomCount_) % library_-> roomCount_);
}

Room* Room::nextRoom() {
    return library_->getRoom((id_ + 1) % library_->roomCount_);
}

std::array<Bookcase*, BOOKCASES_COUNT> Room::getBookcases() {
    std::array<Bookcase*, BOOKCASES_COUNT> a;
    for (std::size_t i = 0; i < BOOKCASES_COUNT; i++) {
        a[i] = bookcases_[i].get();
    }
    return a;
}

void Room::renameBookcase(const std::string& prevName, const std::string& newName) {
    throw std::runtime_error("not implemented");
}

/**
* Bookcase
*/

Bookcase::Bookcase(Room* room, std::string name): room_{room}, name_{name} {
    for (std::size_t i = 0; i < SHELVES_COUNT; i++) {
        shelves_[i] = std::make_unique<Shelf>(this, "shelf_" + std::to_string(i));
    }
}

std::string Bookcase::getName() const {
    return name_;
}

std::array<Shelf*, SHELVES_COUNT> Bookcase::getShelves() {
    std::array<Shelf*, SHELVES_COUNT> a;
    for (std::size_t i = 0; i < SHELVES_COUNT; i++) {
        a[i] = shelves_[i].get();
    }
    return a;
}

Room* Bookcase::getOwnerRoom() {
    return room_;
}

/**
 * Shelf
 */

Shelf::Shelf(Bookcase* bookcase, const std::string& name): bookcase_{bookcase}, name_{name} {
}

std::string Shelf::getName() const {
    return name_;
}



/**
 * Book
 */
/*
bool Book::isContainer() const {
    return false;
}*/


