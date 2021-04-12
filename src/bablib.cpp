/***
 * ДИСКЛЕЙМЕР:
 * ПРОСМОТР ДАННОГО КОДА МОЖЕТ НАНЕСТИ НЕПОПРАВИМЫЙ ВРЕД ВАШЕМУ ЗДОРОВЬЮ
 */

#include "bablib.h"

#include <algorithm>
#include <ranges>
#include <stdexcept>
#include <iostream>

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
            names.push_back(room->getTable()->getName());
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
            for (const auto& b : shelf->getBooks()) {
                if (std::holds_alternative<Shelf*>(b->getOwner()))
                    names.push_back(b->getName());
            }
            return names;
        },
        .bookHandler = [](auto) {
            throw BabLibException("Unexpected call: getContainedEntities(book)");
            return std::vector<std::string>{};
        },
        .tableHandler = [](Table *table) {
            return table->getContainedEntitiesNames();
        },
        .basketHandler = [](Basket* basket) {
            return basket->getContainedEntitiesNames();
        },
        .noteHandler = [](auto) {
            throw BabLibException("Unexpected call: getContainedEntities(note)");
            return std::vector<std::string>{};
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
    auto pos = path.rfind('/');
    if (pos == std::string::npos) throw BabLibException("expected path to have at least one /");
    return path.substr(0, pos + 1);
}

std::string_view getLastToken(std::string_view path) {
    auto pos = path.rfind('/');
    if (pos == std::string::npos) throw BabLibException("expected path to have at least one /");
    return path.substr(pos + 1);
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
            if (firstName == room->getTable()->getName()) return room->getTable();
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
                if (std::holds_alternative<Shelf*>(book->getOwner()) && firstName == book->getName())
                    return book;
            }
            throw BabLibException("no such book");
        },
        .bookHandler = [&path](auto book) -> LibraryEntity {
            if (path.length() == 0) return book;
            throw BabLibException("book is not a container!");
        },
        .tableHandler = [&path](Table *table) -> LibraryEntity {
            if (path.length() == 0) return table;
            auto firstName = extractFirstToken(path);
            for (const auto &basket : table->getBaskets()) {
                if (basket->getName() == firstName)
                    return basket;
            }
            for (const auto& note : table->getNotes()) {
                if (note->getName() == firstName)
                    return note;
            }
            for (const auto& book : table->getBooks()) {
                if (book->getName() == firstName)
                    return book;
            }
            throw BabLibException("no such book");
        },
        .basketHandler = [&path](Basket* basket) -> LibraryEntity {
            if (path.length() == 0) return basket;
            auto firstName = extractFirstToken(path);
            for (const auto& note : basket->getNotes()) {
                if (note->getName() == firstName)
                    return note;
            }
            throw BabLibException("no such note");
        },
        .noteHandler = [&path](auto note) -> LibraryEntity {
            if (path.length() == 0) return note;
            throw BabLibException("note is not a container!");
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
    table_ = std::make_unique<Table>(this, "desk");
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

Table* Room::getTable() {
    return table_.get();
}

std::array<Bookcase*, BOOKCASES_COUNT> Room::getBookcases() {
    std::array<Bookcase*, BOOKCASES_COUNT> a;
    for (std::size_t i = 0; i < BOOKCASES_COUNT; i++) {
        a[i] = bookcases_[i].get();
    }
    return a;
}

void Room::renameBookcase(std::string_view prevName, std::string_view newName) {
    int idx;
    for (idx = 0; idx < BOOKCASES_COUNT; idx++) {
        if (bookcases_[idx]->getName() == prevName)
            break;
    }
    if (idx == BOOKCASES_COUNT)
        throw BabLibException("no bookcase named " + std::string(prevName));
    // check conflicts
    if (newName == previousRoom()->getName() || newName == nextRoom()->getName())
        throw BabLibException("name is taken by a room");
    if (newName == table_->getName())
        throw BabLibException("name is taken by a table");
    for (int i = 0; i < BOOKCASES_COUNT; i++) {
        if (i != idx && bookcases_[i]->getName() == newName)
            throw BabLibException("name is already taken by a bookcase");
    }
    bookcases_[idx]->setName(std::string(newName));
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

void Bookcase::setName(const std::string &name) {
    name_ = name;
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

void Bookcase::renameShelf(std::string_view prevName, std::string_view newName) {
    int idx;
    for (idx = 0; idx < SHELVES_COUNT; idx++) {
        if (shelves_[idx]->getName() == prevName)
            break;
    }
    if (idx == SHELVES_COUNT)
        throw BabLibException("no shelf named " + std::string(prevName));
    // check conflicts
    for (int i = 0; i < SHELVES_COUNT; i++) {
        if (i != idx && shelves_[i]->getName() == newName)
            throw BabLibException("name already is taken by a shelf");
    }
    shelves_[idx]->setName(std::string(newName));
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
        books_[i] = std::make_unique<Book>(this, bId);
    }
}

std::string Shelf::getName() const {
    return name_;
}

void Shelf::setName(const std::string &name) {
    name_ = name;
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

Book::Book(Shelf* shelf, uint64_t id): id_{id}, currentOwner_{shelf}, origin_{shelf} {
}

std::string Book::getName() const {
    return "book_" + std::to_string(id_);
}

static const char alphabet[28] = {'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z','.',','};

static uint64_t hash(const std::string &str) {
    uint64_t hash = 5381;
    for (char c: str)
        hash = ((hash << 5) + hash) + c;
    return hash;
}

static uint64_t xorshift64(uint64_t *state) {
    if (*state == 0) {
        *state = 1;
    }

    uint64_t x = *state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    return *state = x;
}

std::string Book::getContent() const {
    uint64_t state = hash(std::to_string(id_));
    std::string content;
    content.reserve(BOOK_SIZE);
    for (int i = 0; i < BOOK_SIZE; ++i) {
        content.push_back(alphabet[xorshift64(&state) % 28]);
    }
    return content;
}

Shelf* Book::getOrigin() const {
    return origin_;
}

LibraryEntity Book::getOwner() const {
    return currentOwner_;
}

void Book::setOwner(LibraryEntity entity) {
    currentOwner_ = entity;
}

/**
 * Containers
 */

std::vector<Basket*> BasketContainer::getBaskets() {
    std::vector<Basket*> result;
    for (const auto &b : baskets_)
        result.push_back(b.get());
    return result;
}
void BasketContainer::createBasket(std::string_view name) {
    auto contained = this->getContainedEntitiesNames();
    if (std::ranges::find(contained, name) != contained.end())
        throw BabLibException("name is already taken");
    baskets_.emplace_back(std::make_unique<Basket>(reinterpret_cast<Table*>(this), std::string(name)));
}
void BasketContainer::renameBasket(std::string_view oldName, std::string_view newName) {
    if (oldName == newName) return;
    for (auto &basket : baskets_) {
        if (basket->getName() == oldName) {
            auto contained = this->getContainedEntitiesNames();
            if (std::ranges::find(contained, newName) != contained.end())
                throw BabLibException("name is already taken");
            basket->setName(std::string(newName));
            return;
        }
    }
    throw BabLibException("no basket named " + std::string(oldName) + " found");
}
void BasketContainer::deleteBasket(std::string_view name) {
    for (std::size_t i = 0; i < baskets_.size(); i++) {
        if (baskets_[i]->getName() == name) {
            baskets_[i].reset();
            swap(baskets_[i], baskets_.back());
            baskets_.pop_back();
            return;
        }
    }
    throw BabLibException("no basket named " + std::string(name) + " found");
}
std::vector<std::string> BasketContainer::getContainedEntitiesNames() {
    std::vector<std::string> result;
    for (const auto &b : baskets_) {
        result.push_back(b->getName());
    }
    return result;
};

std::vector<Note*> NoteContainer::getNotes() {
    std::vector<Note*> result;
    for (const auto &b : notes_)
        result.push_back(b.get());
    return result;
}
void NoteContainer::createNote(std::string_view name) {
    auto contained = this->getContainedEntitiesNames();
    if (std::ranges::find(contained, name) != contained.end())
        throw BabLibException("name is already taken");
    notes_.emplace_back(std::make_unique<Note>(this, std::string(name)));
}
void NoteContainer::renameNote(std::string_view oldName, std::string_view newName) {
    if (oldName == newName) return;
    for (auto &note : notes_) {
        if (note->getName() == oldName) {
            auto contained = this->getContainedEntitiesNames();
            if (std::ranges::find(contained, newName) != contained.end())
                throw BabLibException("name is already taken");
            note->setName(std::string(newName));
            return;
        }
    }
    throw BabLibException("no note named " + std::string(oldName) + " found");
}
void NoteContainer::deleteNote(std::string_view name) {
    for (std::size_t i = 0; i < notes_.size(); i++) {
        if (notes_[i]->getName() == name) {
            notes_[i].reset();
            swap(notes_[i], notes_.back());
            notes_.pop_back();
            return;
        }
    }
    throw BabLibException("no note named " + std::string(name) + " found");
}
std::vector<std::string> NoteContainer::getContainedEntitiesNames() {
    std::vector<std::string> result;
    for (const auto &b : notes_) {
        result.push_back(b->getName());
    }
    return result;
};


/**
 * Table
 */

Table::Table(Room* room, std::string name): room_{room}, name_{name} {}

std::string Table::getName() const {
    return name_;
}

std::vector<std::string> Table::getContainedEntitiesNames() {
    std::vector<std::string> result;
    for (const auto &b : books_) {
        result.push_back(b->getName());
    }
    for (const auto &b : baskets_) {
        result.push_back(b->getName());
    }
    for (const auto &b : notes_) {
        result.push_back(b->getName());
    }
    return result;
};

std::vector<Book*> Table::getBooks() {
    return books_;
}

void Table::addBook(Book* b) {
    books_.push_back(b);
}

void Table::removeBook(std::string_view name) {
    for (std::size_t i = 0; i < books_.size(); i++) {
        if (books_[i]->getName() == name) {
            std::swap(books_[i], books_.back());
            books_.pop_back();
            return;
        }
    }
    throw BabLibException("no book named " + std::string(name) + " found");
}

/**
 * Basket
 */

Basket::Basket(Table* table, std::string name): table_{table}, name_{name} {}

Table* Basket::getOwnerTable() const {
    return table_;
}

std::string Basket::getName() const {
    return name_;
}
void Basket::setName(const std::string &newName) {
    name_ = newName;
}


/**
 * Note
 */

Note::Note(NoteContainer *container, std::string name): container_{container}, name_{name} {
}

NoteContainer* Note::getOwner() const {
    return container_;
}

std::string Note::getName() const {
    return name_;
}
void Note::setName(const std::string &newName) {
    name_ = newName;
}

std::string Note::getContent() const {
    return content_;
}
void Note::setContent(const std::string &content) {
    content_ = content;
}
