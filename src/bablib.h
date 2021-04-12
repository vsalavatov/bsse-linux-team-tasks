#ifndef BABLIB_H
#define BABLIB_H

#include <memory>
#include <string>
#include <variant>
#include <unordered_map>
#include <vector>
#include <string_view>
#include <functional>
#include <mutex>


#define BOOKCASES_COUNT 4
#define SHELVES_COUNT 5
#define BOOKS_COUNT 32
#define BOOK_SIZE (256ul * 4096)

class BabLibException : public std::runtime_error {
public:
    explicit BabLibException(const std::string &msg);
};

class Book;
class Shelf;
class Bookcase;
class Room;
class Table;
class Basket;
class Note;

using LibraryEntity = std::variant<
    Room*,
    Bookcase*,
    Shelf*,
    Book*,
    Table*,
    Basket*,
    Note*
>;

template<class T>
class LibraryEntityVisitor {
public:
    std::function<T(Room*)> roomHandler;
    std::function<T(Bookcase*)> bookcaseHandler;
    std::function<T(Shelf*)> shelfHandler;
    std::function<T(Book*)> bookHandler;
    std::function<T(Table*)> tableHandler;
    std::function<T(Basket*)> basketHandler;
    std::function<T(Note*)> noteHandler;

    T visit(LibraryEntity entity) {
        /*std::visit([](auto ptr) {
            using T = std::remove_pointer_t<std::decay<decltype(ptr)>>;
            if constexpr (std::is_same_v<T, Room>) {
                return roomHandler(ptr);
            } else if constexpr (std::is_same_v<T, Bookcase>) {
                return bookcaseHandler(ptr);
            } else if constexpr (std::is_same_v<T, Shelf>) {
                return shelfHandler(ptr);
            } else if constexpr (std::is_same_v<T, Book>) {
                return bookHandler(ptr);
            } else if constexpr (std::is_same_v<T, Table>) {
                return tableHandler(ptr);
            } else if constexpr (std::is_same_v<T, Basket>) {
                return basketHandler(ptr);
            } else if constexpr (std::is_same_v<T, Note>) {
                return noteHandler(ptr);
            } else {
                static_assert(false, "Visitor is incomplete");
            }
        }, entity);*/

        if (auto room = std::get_if<Room*>(&entity)) {
            return roomHandler(*room);
        }
        if (auto bc = std::get_if<Bookcase*>(&entity)) {
            return bookcaseHandler(*bc);
        }
        if (auto shelf = std::get_if<Shelf*>(&entity)) {
            return shelfHandler(*shelf);
        }
        if (auto book = std::get_if<Book*>(&entity)) {
            return bookHandler(*book);
        }
        if (auto table = std::get_if<Table*>(&entity)) {
            return tableHandler(*table);
        }
        if (auto basket = std::get_if<Basket*>(&entity)) {
            return basketHandler(*basket);
        }
        if (auto note = std::get_if<Note*>(&entity)) {
            return noteHandler(*note);
        }
        throw BabLibException("o_O? visitor has failed");
    }
};

std::string_view extractFirstToken(std::string_view &path);
std::string_view removeLastToken(std::string_view path);
std::string_view getLastToken(std::string_view path);

bool isContainer(LibraryEntity entity);
std::vector<std::string> getContainedEntities(LibraryEntity entity);


class Library {
public:
    explicit Library(uint64_t roomCount, uint64_t rootId);
    Library(const Library&) = delete;
    Library(Library &&) = delete;

    Room* getRootRoom();

    LibraryEntity resolve(std::string_view path);

private:
    Room* getRoom(uint64_t id);

    class Resolver : public LibraryEntityVisitor<LibraryEntity> {
    public:
        Resolver(std::string_view path);

        std::string_view path_;
    };

    const uint64_t roomCount_;
    const uint64_t rootId_;
    std::unordered_map<uint64_t, std::unique_ptr<Room>> roomStorage_;

    friend class Room;
};

class Room {
public:
    explicit Room(Library* lib, uint64_t id);
    Room(const Room &) = delete;
    Room(Room&&) = delete;

    std::string getName() const;
    std::uint64_t getId() const;

    Room* previousRoom();
    Room* nextRoom();

    Table* getTable();
    std::array<Bookcase*, BOOKCASES_COUNT> getBookcases();
    void renameBookcase(std::string_view prevName, std::string_view newName);

private:
    Library* library_;
    uint64_t id_;
    std::array<std::unique_ptr<Bookcase>, BOOKCASES_COUNT> bookcases_;
    std::unique_ptr<Table> table_;
};

class Bookcase {
public:
    Bookcase(Room* room, uint64_t id);
    Bookcase(const Bookcase &) = delete;
    Bookcase(Bookcase&&) = delete;

    std::string getName() const;
    void setName(const std::string &newName);

    uint64_t getId() const;

    std::array<Shelf*, SHELVES_COUNT> getShelves();
    void renameShelf(std::string_view prevName, std::string_view newName);

    Room* getOwnerRoom();

private:
    Room* room_;
    std::string name_;
    uint64_t id_;
    std::array<std::unique_ptr<Shelf>, SHELVES_COUNT> shelves_;

    friend class Room;
};

class Shelf {
public:
    Shelf(Bookcase* bookcase, uint64_t id);
    Shelf(const Shelf &) = delete;
    Shelf(Shelf&&) = delete;

    std::string getName() const;
    void setName(const std::string &newName);   

    std::array<Book*, BOOKS_COUNT> getBooks();
private:
    Bookcase* bookcase_;
    std::string name_;
    uint64_t id_;

    std::array<std::unique_ptr<Book>, BOOKS_COUNT> books_;
};

class Book {
public:
    explicit Book(Shelf* shelf, uint64_t id);
    Book(const Book &) = delete;
    Book(Book&&) = delete;

    std::string getName() const;

    std::string getContent() const;

    Shelf* getOrigin() const;
    LibraryEntity getOwner() const;
    void setOwner(LibraryEntity);
private:
    uint64_t id_;
    Shelf* origin_;
    LibraryEntity currentOwner_;
};

class Container {
    virtual std::vector<std::string> getContainedEntitiesNames() = 0;
};

class BasketContainer : virtual public Container {
public:
    virtual std::vector<Basket*> getBaskets();
    virtual void createBasket(std::string_view name);
    virtual void renameBasket(std::string_view oldName, std::string_view newName);
    virtual void deleteBasket(std::string_view name);
    virtual std::vector<std::string> getContainedEntitiesNames() override;
protected:
    std::vector<std::unique_ptr<Basket>> baskets_;
};

class NoteContainer : virtual public Container {
public:
    virtual std::vector<Note*> getNotes();
    virtual void createNote(std::string_view name);
    virtual void renameNote(std::string_view oldName, std::string_view newName);
    virtual void deleteNote(std::string_view name);
    virtual std::vector<std::string> getContainedEntitiesNames() override;
protected:
    std::vector<std::unique_ptr<Note>> notes_;
};

class Table : virtual public BasketContainer, virtual public NoteContainer {
public:
    explicit Table(Room* room, std::string name);
    Table(const Table&) = delete;
    Table(Table&&) = delete;    

    std::string getName() const;

    virtual std::vector<std::string> getContainedEntitiesNames() override;

    std::vector<Book*> getBooks();

private:
    Room* room_;
    std::string name_;
    std::vector<std::unique_ptr<Basket>> baskets_;
    std::vector<std::unique_ptr<Note>> notes_;
    std::vector<Book*> books_;
};

class Basket : virtual public NoteContainer {
public:
    explicit Basket(Table* table, std::string name);
    Basket(const Basket&) = delete;
    Basket(Basket&&) = delete;    

    std::string getName() const;
    void setName(const std::string &newName);
private:
    Table *table_;
    std::string name_;
};

class Note {
public:
    explicit Note(std::string name);
    Note(const Note&) = delete;
    Note(Note&&) = delete;    

    std::string getName() const;
    void setName(const std::string& newName);

    std::string getContent() const;
    void setContent(const std::string &c);
private:
    std::string name_;
    std::string content_;
};

#endif
