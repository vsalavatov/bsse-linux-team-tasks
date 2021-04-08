#ifndef BABLIB_H
#define BABLIB_H

#include <memory>
#include <string>
#include <variant>
#include <unordered_map>

class LibraryEntity;
class Book;
class Shelf;
class Bookcase;
class Room;
class Table;
class Bucket;

class Library : public std::enable_shared_from_this<Library> {
public:
    explicit Library(uint64_t roomCount, uint64_t rootId);
    Library(const Library&) = delete;
    Library(Library &&) = delete;

    std::shared_ptr<Room> getRootRoom();

    std::shared_ptr<LibraryEntity> resolve(const std::string_view &path);

private:
    std::shared_ptr<Room> getRoom(uint64_t id);

    const uint64_t roomCount_;
    const uint64_t rootId_;
    std::unordered_map<uint64_t, std::shared_ptr<Room>> roomStorage_;

    friend class Room;
};

class LibraryEntity: public std::enable_shared_from_this<LibraryEntity> {
public:
    virtual ~LibraryEntity() {};
    virtual bool isContainer() const = 0;
    virtual std::vector<std::shared_ptr<LibraryEntity>> getEntities() const = 0;
    virtual std::string getName() const = 0;
    virtual std::shared_ptr<LibraryEntity> resolve(const std::string_view path) const;
};

class Room : public LibraryEntity {
public:
    const static std::size_t BOOKCASES_COUNT = 4;

    explicit Room(std::shared_ptr<Library> lib, uint64_t id);
    Room(const Room &) = delete;
    Room(Room&&) = delete;
    ~Room() = default;

    std::string getName() const override;

    std::shared_ptr<Room> previousRoom();
    std::shared_ptr<Room> nextRoom();

    const std::array<std::shared_ptr<Bookcase>, BOOKCASES_COUNT> &getBookcases() const;
    void renameBookcase(const std::string &prevName, const std::string& newName);

    LibraryEntity resolve(const std::string_view &path);
private:
    uint64_t id_;
    std::array<std::shared_ptr<Bookcase>, BOOKCASES_COUNT> bookcases_;
    
    std::shared_ptr<Library> library_;
    std::shared_ptr<Room> nextRoom_, prevRoom_;
};

class Bookcase : public LibraryEntity {
public:
    const static std::size_t SHELVES_COUNT = 5;

    Bookcase(std::shared_ptr<Room> room, std::string name);
    LibraryEntity resolve(const std::string_view &path);

    std::string getName() const;

    const std::array<const std::shared_ptr<Shelf>, SHELVES_COUNT> getShelves() const;
    void renameShelf(const std::string& prevName, const std::string &newName);

private:
    void setName(const std::string &newName);

    std::string name_;
    std::shared_ptr<Room> room_;
    std::array<std::shared_ptr<Shelf>, SHELVES_COUNT> shelves_;

    friend class Room;
};

class Shelf : public LibraryEntity {

};

class Book : public LibraryEntity {};

class Table : public LibraryEntity {};

class Bucket : public LibraryEntity {};

#endif
