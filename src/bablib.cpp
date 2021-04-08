#include "bablib.h"

#include <stdexcept>


/**
 * Library
 */

Library::Library(uint64_t roomCount, uint64_t rootId)
    : roomCount_{roomCount}, rootId_{rootId}, roomStorage_{} {
}

std::shared_ptr<Room> Library::getRootRoom() {
    return getRoom(rootId_);
}

std::shared_ptr<LibraryEntity> Library::resolve(const std::string_view &path) {
    if (path.length() == 0 || path[0] != '/') {
        throw std::runtime_error("path is not absolute");
    }
    return getRootRoom()->resolve(path.substr(1));
}

std::shared_ptr<Room> Library::getRoom(uint64_t id) {
    if (roomStorage_.find(id) != roomStorage_.end())
        return roomStorage_[id];
    return roomStorage_[id] = std::make_shared<Room>(shared_from_this(), id);
}

/**
 * LibraryEntity
 */


std::shared_ptr<LibraryEntity> LibraryEntity::resolve(const std::string_view path) const {
    if (path.length() == 0) {
        return shared_from_this();
    }
    int pos = path.find('/');
    std::string_view firstName = path.substr(0, pos);
    std::string_view subpath = path.substr(pos + 1);
    
    for (auto entity: getEntities()) {
        if (firstName == entity->getName())) {
            return entity->resolve(subpath);
        }   
    }
    throw std::runtime_error("no such container or book");
}

/**
 * Room
 */

Room::Room(std::shared_ptr<Library> lib, uint64_t id): library_{lib}, id_{id} {
    for (std::size_t i = 0; i < BOOKCASES_COUNT; i++) {
        if (!bookcases_[i]) {
            uint64_t bcId = id_ * 123124 + 5232 + i;
            bookcases_[i] = std::make_shared<Bookcase>(shared_from_this(), "bc_" + std::to_string(bcId));
        }
    }
}

std::string Room::getName() const {
    return "room_" + std::to_string(id_);
};

std::shared_ptr<Room> Room::previousRoom() {
    return library_->getRoom((id_ - 1 + library_->roomCount_) % library_-> roomCount_);
}

std::shared_ptr<Room> Room::nextRoom() {
    return library_->getRoom((id_ + 1) % library_->roomCount_);
}

const std::array<std::shared_ptr<Bookcase>, Room::BOOKCASES_COUNT>& Room::getBookcases() const {
    return bookcases_;
}

void Room::renameBookcase(const std::string& prevName, const std::string& newName) {
    for (std::size_t i = 0; i < BOOKCASES_COUNT; i++) {
        if (!bookcases_[i]) {
            uint64_t bcId = id_ * 123124 + 5232 + i;
            bookcases_[i] = std::make_shared<Bookcase>(shared_from_this(), "bc_" + std::to_string(bcId));
        }
    }
}

LibraryEntity Room::resolve(const std::string_view &path) {
    if (path.length() == 0) {
        return shared_from_this();
    }
    int pos = path.find('/');
    std::string_view firstName = path.substr(0, pos);
    std::string_view subpath = path.substr(pos + 1);
    
    if (firstName == previousRoom()->getName()) {
        return previousRoom()->resolve(subpath);
    } else if (firstName == nextRoom()->getName()) {
        return nextRoom()->resolve(subpath);
    } else {
        for (auto bookcase: getBookcases()) {
            if (firstName == bookcase->getName()) {
                return bookcase->resolve(subpath);
            }
        }
    }
    throw std::runtime_error("no such room or shelf");
}

/**
* Bookcase
*/

Bookcase::Bookcase(std::shared_ptr<Room> room, std::string name): room_{room}, name_{name} {
}

std::string Bookcase::getName() const {
    return name_;
};

LibraryEntity Bookcase::resolve(const std::string_view &path) {
    if (path.length() == 0) {
        return shared_from_this();
    }
    int pos = path.find('/');
    std::string_view firstName = path.substr(0, pos);
    std::string_view subpath = path.substr(pos + 1);
    
    for (auto shelf: getShelves()) {
        if (firstName == shelf->getName()) {
            return shelf->resolve(subpath);   
        }
    }
    throw std::runtime_error("no such directory or file");
}

