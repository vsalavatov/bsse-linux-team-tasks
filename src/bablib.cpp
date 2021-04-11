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
        },
        .shelfHandler = [](Shelf* shelf) {
            std::vector<std::string> names;
            for (const auto& b : shelf->getBooks())
                names.push_back(b->getName());
            return names;
        }
    }.visit(entity);
}

std::string getName(LibraryEntity entity) {
    return LibraryEntityVisitor<std::string>{
            .bookHandler = [](Book *book) {
                return book->getName();
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
        },
        .shelfHandler = [&path](Shelf* s) -> LibraryEntity {
            if (path.length() == 0) return s;
            auto firstName = extractFirstToken(path);
            for (auto book : s->getBooks()) {
                if (firstName == book->getName())
                    return book;
            }
            throw BabLibException("no such book");
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
    for (std::size_t bcId = 0; bcId < BOOKCASES_COUNT; bcId++) {
        bookcases_[bcId] = std::make_unique<Bookcase>(this, bcId);
    }
}
 
std::string Room::getName() const {
    return "room_" + std::to_string(id_);
}

uint64_t Room::getId() const {
    return id_;
}

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

Bookcase::Bookcase(Room* room, uint64_t id): room_{room}, id_{id} {
    name_ = "bookcase_" + std::to_string(id);
    for (std::size_t sId = 0; sId < SHELVES_COUNT; sId++) {
        shelves_[sId] = std::make_unique<Shelf>(this, sId);
    }
}

std::string Bookcase::getName() const {
    return name_;
}

uint64_t Bookcase::getId() const {
    return id_;
}

std::array<Shelf*, SHELVES_COUNT> Bookcase::getShelves() {
    std::array<Shelf*, SHELVES_COUNT> a{};
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

Shelf::Shelf(Bookcase* bookcase, uint64_t id): bookcase_{bookcase}, id_{id} {
    name_ = "shelf_" + std::to_string(id);
    for (std::size_t i = 0; i < BOOKS_COUNT; i++) {
        uint64_t rId = bookcase->getOwnerRoom()->getId();
        uint64_t bcId = bookcase->getId();
        uint64_t bId = ((rId * BOOKCASES_COUNT + bcId) * SHELVES_COUNT + id_) * BOOKS_COUNT + i;
        books_[i] = std::make_unique<Book>(bId);
    }
}

std::string Shelf::getName() const {
    return name_;
}

std::array<Book*, BOOKS_COUNT> Shelf::getBooks() {
    std::array<Book*, BOOKS_COUNT> a{};
    for (std::size_t i = 0; i < BOOKS_COUNT; i++) {
        a[i] = books_[i].get();
    }
    return a;
}

/**
 * Book
 */

Book::Book(uint64_t id): id_{id} {
}

std::string Book::getName() const {
    return "book_" + std::to_string(id_);
}


