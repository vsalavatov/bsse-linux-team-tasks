/***
 * ДИСКЛЕЙМЕР:
 * ПРОСМОТР ДАННОГО КОДА МОЖЕТ НАНЕСТИ НЕПОПРАВИМЫЙ ВРЕД ВАШЕМУ ЗДОРОВЬЮ
 */

#define FUSE_USE_VERSION 31
#include <iostream>
#include <cstring>
#include <functional>
#include <fuse3/fuse.h>

#include "bablib.h"

std::shared_ptr<Library> library;

void *bablib_init(struct fuse_conn_info *conn, struct fuse_config *cfg) {
    std::cerr << "bablib_init" << std::endl;
	(void) conn;
	cfg->kernel_cache = 0;
	return NULL;
}

int bablib_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    int res = 0;
    std::cerr << "bablib_getattr path=" << path << std::endl;

    memset(stbuf, 0, sizeof(stbuf));
    try {
        LibraryEntity entity = library->resolve(path);
        auto containerHandler = [&](auto) {
            stbuf->st_mode = S_IFDIR | 0444;
        };
        auto bookHandler = [&](auto) {
            stbuf->st_mode = S_IFREG | 0444;
            stbuf->st_nlink = 1;
            stbuf->st_size = BOOK_SIZE;
        };
        auto noteHandler = [&](Note *note) {
            stbuf->st_mode = S_IFREG | 0666;
            stbuf->st_nlink = 1;
            stbuf->st_size = note->getContent().size();
        };
        LibraryEntityVisitor<void>{
            .roomHandler = containerHandler,
            .bookcaseHandler = containerHandler,
            .shelfHandler = containerHandler,
            .bookHandler = bookHandler,
            .tableHandler = containerHandler,
            .basketHandler = containerHandler,
            .noteHandler = noteHandler
        }.visit(entity);
    } catch (std::exception &e) {
        std::cerr << "Something went wrong: " << e.what() << std::endl;
        res = -ENOENT;
    }

    return res;
}

int bablib_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                   off_t offset, struct fuse_file_info *fi,
                   enum fuse_readdir_flags flags)
{
    std::cerr << "bablib_readdir path=" << path << std::endl;
    (void) offset;
    (void) fi;
    (void) flags;
    
    int res = 0;
    try {
        LibraryEntity resolvedEntity = library->resolve(path);
        if (isContainer(resolvedEntity)) {
            auto contained = getContainedEntities(resolvedEntity);
            if (std::string(path) != "/" && std::get_if<Room*>(&resolvedEntity)) { // remove previous room to reduce clutter
                auto prevEnt = library->resolve(removeLastToken(path));
                if (auto prevRoom = std::get_if<Room*>(&prevEnt)) {
                    auto iter = std::find(contained.begin(), contained.end(), (*prevRoom)->getName());
                    if (iter != contained.end())
                        contained.erase(iter);
                }
            }
            for (const auto& name : contained) {
                filler(buf, name.c_str(), NULL, 0, fuse_fill_dir_flags(0));
            }
         }
    } catch (std::exception &e) {
        std::cerr << "Something went wrong: " << e.what() << std::endl;
        res = -ENOENT;
    }
    return res;
}

int bablib_open(const char *path, struct fuse_file_info *fi)
{
    std::cerr << "bablib_open path=" << path << std::endl;
    try {
        LibraryEntity entity = library->resolve(path);
        if (!isContainer(entity)) {
            if ((fi->flags & O_ACCMODE) != O_RDONLY)
                return -EACCES;
        } else {
            return -EISDIR;
        }
    } catch (std::exception &e) {
        std::cerr << "Something went wrong: " << e.what() << std::endl;
        return -ENOENT;
    }
    return 0;
}

int bablib_read(const char *path, char *buf, size_t size, off_t offset,
                struct fuse_file_info *fi)
{
    std::cerr << "bablib_read path=" << path << std::endl;
    (void) fi;
    try {
        LibraryEntity entity = library->resolve(path);
        auto cantRead = [&](std::string kind) -> auto {
            return [&, kind](auto) {
                std::cerr << "Cannot read " << kind << "!" << std::endl;
                size = 0;
            };
        };
        auto doRead = [&](std::string content) -> auto {
            if (offset < content.length()) {
                size = std::min(size, content.length() - offset);
                memcpy(buf, (char*)content.c_str() + offset, size);
            } else {
                size = 0;
            }
        };
        LibraryEntityVisitor<void>{
            .roomHandler = cantRead("room"),
            .bookcaseHandler = cantRead("bookcase"),
            .shelfHandler = cantRead("shelf"),
            .bookHandler = [&](auto book) {
                doRead(book->getContent());
            },
            .tableHandler = cantRead("table"),
            .basketHandler = cantRead("basket"),
            .noteHandler = [&](auto note) {
                doRead(note->getContent());
            }
        }.visit(entity);
    } catch (std::exception &e) {
        std::cerr << "Something went wrong: " << e.what() << std::endl;
        return -ENOENT;
    }
    return size;
}

int bablib_rename(const char *old_path, const char *new_path, unsigned int flags) {
    std::cerr << "bablib_rename old_path=" << old_path << " new_path=" << new_path << " flags=" << flags << std::endl;
    (void) flags;
    std::string oldPath(old_path);
    std::string newPath(new_path);
    try {
        if (oldPath == newPath)
            return 0;
        LibraryEntity entity = library->resolve(oldPath);
        auto assertSameContainer = [&oldPath, &newPath](auto f) -> auto {
            LibraryEntity parentEntity = library->resolve(removeLastToken(oldPath));
            LibraryEntity newPathParentEntity;
            try {
                newPathParentEntity = library->resolve(removeLastToken(newPath));
            } catch (...) {
                std::cerr << "Bad new name!" << std::endl;
                return -EINVAL;
            }
            if (parentEntity != newPathParentEntity) {
                std::cerr << "Entity cannot be moved outside the container!" << std::endl;
                return -EINVAL;
            }
            return f(getLastToken(oldPath), getLastToken(newPath), parentEntity);
        };

        return LibraryEntityVisitor<int>{
            .roomHandler = [](auto) {
                std::cerr << "Renaming a room is forbidden!" << std::endl;
                return -EINVAL;
            },
            .bookcaseHandler = [&assertSameContainer](auto bookcase) {
                return assertSameContainer([](auto oldName, auto newName, LibraryEntity parent) {
                    if (auto room = std::get_if<Room*>(&parent)) {
                        try {
                            (*room)->renameBookcase(oldName, newName);
                        } catch (std::exception &e) {
                            std::cerr << "Name conflict: " << e.what() << std::endl;
                            return -EINVAL;
                        }
                    } else {
                        throw BabLibException("unexpected bookcase owner");
                    }
                    return 0;
                });
            },
            .shelfHandler = [&assertSameContainer](auto shelf) {
                return assertSameContainer([](auto oldName, auto newName, LibraryEntity parent) {
                    if (auto bookcase = std::get_if<Bookcase*>(&parent)) {
                        try {
                            (*bookcase)->renameShelf(oldName, newName);
                        } catch (std::exception &e) {
                            std::cerr << "Name conflict: " << e.what() << std::endl;
                            return -EINVAL;
                        }
                    } else {
                        throw BabLibException("unexpected shelf owner");
                    }
                    return 0;
                });
            },
            .bookHandler = [&oldPath, &newPath](Book* book) {
                LibraryEntity parentEntity = library->resolve(removeLastToken(oldPath));
                LibraryEntity newPathParentEntity;
                try {
                    newPathParentEntity = library->resolve(removeLastToken(newPath));
                } catch (...) {
                    std::cerr << "Bad new name!" << std::endl;
                    return -EINVAL;
                }
                if (!std::holds_alternative<Table*>(newPathParentEntity) && !std::holds_alternative<Shelf*>(newPathParentEntity)) {
                    std::cerr << "Cannot move book there!" << std::endl;
                    return -EINVAL;
                }
                auto oldName = getLastToken(oldPath);
                auto newName = getLastToken(newPath);
                if (oldName != newName) {
                    std::cerr << "Book renaming is forbidden!" << std::endl;
                    return -EINVAL;
                }
                if (parentEntity == newPathParentEntity) {
                    return 0;
                } else {
                    try {
                        auto newParentContained = getContainedEntities(newPathParentEntity);
                        if (std::find(newParentContained.begin(), newParentContained.end(), std::string(newName)) != newParentContained.end())
                            throw BabLibException("name is already taken");
                        LibraryEntityVisitor<void>{
                            .shelfHandler = [&](Shelf* s) {
                                if (book->getOrigin() != s)
                                    throw BabLibException("cannot move book to a shelf it wasn't originated from");
                            },
                            .tableHandler = [&](Table* t) {
                                t->addBook(book);
                            },
                        }.visit(newPathParentEntity);
                        LibraryEntityVisitor<void>{
                            .shelfHandler = [&](Shelf* s) {
                            },
                            .tableHandler = [&](Table* t) {
                                t->removeBook(book->getName());
                            },
                        }.visit(parentEntity);
                        book->setOwner(newPathParentEntity);                        
                    } catch (std::exception &e) {
                        std::cerr << "Incorrect move op: " << e.what() << std::endl;
                        return -EINVAL;
                    }
                    return 0;
                }
                return 0;
            },
            .tableHandler = [](auto table) {
                std::cerr << "Renaming a table is forbidden!" << std::endl;
                return -EINVAL;
            },
            .basketHandler = [&assertSameContainer](auto basket) {
                return assertSameContainer([](auto oldName, auto newName, LibraryEntity parent) {
                    if (auto table = std::get_if<Table*>(&parent)) {
                        try {
                            (*table)->renameBasket(oldName, newName);
                        } catch (std::exception &e) {
                            std::cerr << "Name conflict: " << e.what() << std::endl;
                            return -EINVAL;
                        }
                    } else {
                        throw BabLibException("unexpected basket owner");
                    }
                    return 0;
                });
            },
            .noteHandler = [&oldPath, &newPath](Note* note) {
                LibraryEntity parentEntity = library->resolve(removeLastToken(oldPath));
                LibraryEntity newPathParentEntity;
                try {
                    newPathParentEntity = library->resolve(removeLastToken(newPath));
                } catch (...) {
                    std::cerr << "Bad new name!" << std::endl;
                    return -EINVAL;
                }
                if (!std::holds_alternative<Table*>(newPathParentEntity) && !std::holds_alternative<Basket*>(newPathParentEntity)) {
                    std::cerr << "Cannot move note there!" << std::endl;
                    return -EINVAL;
                }
                if (parentEntity == newPathParentEntity) {
                    auto renamer = [&](auto ent) {
                        try {
                            (*ent)->renameNote(getLastToken(oldPath), getLastToken(newPath));
                        } catch (std::exception &e) {
                            std::cerr << "Name conflict: " << e.what() << std::endl;
                        }
                    };
                    if (auto table = std::get_if<Table*>(&parentEntity)) {
                        renamer(table);
                    } else if (auto basket = std::get_if<Basket*>(&parentEntity)) {
                        renamer(basket);
                    } else {
                        throw BabLibException("unexpected note owner");
                    }
                    return 0;
                } else {
                    try {
                        auto oldName = getLastToken(oldPath);
                        auto newName = getLastToken(newPath);
                        auto newParentContained = getContainedEntities(newPathParentEntity);
                        if (std::find(newParentContained.begin(), newParentContained.end(), std::string(newName)) != newParentContained.end())
                            throw BabLibException("name is already taken");
                        auto adder = [&](auto ent) {
                            ent->createNote(newName);
                            ent->getNotes().back()->setContent(note->getContent());
                        };
                        auto deleter = [&](auto ent) {
                            ent->deleteNote(oldName);
                        };
                        LibraryEntityVisitor<void>{
                            .tableHandler = adder,
                            .basketHandler = adder
                        }.visit(newPathParentEntity);
                        LibraryEntityVisitor<void>{
                            .tableHandler = deleter,
                            .basketHandler = deleter
                        }.visit(parentEntity);
                    } catch (std::exception &e) {
                        std::cerr << "Incorrect move op: " << e.what() << std::endl;
                        return -EINVAL;
                    }
                    return 0;
                }
            }
        }.visit(entity);
    } catch (std::exception &e) {
        std::cerr << "Something went wrong: " << e.what() << std::endl;
        return -ENOENT;
    }
    return 0;
}

int bablib_unlink(const char *path) {
    std::cerr << "bablib_unlink path=" << path << std::endl;
    try {
        LibraryEntity entity = library->resolve(path);
        auto cantDelete = [&](std::string kind) {
            return [&, kind](auto) {
                std::cerr << "Cannot delete " << kind << "!" << std::endl;
                if (isContainer(entity))
                    return -EISDIR;
                else
                    return -EACCES;
            };
        };
        return LibraryEntityVisitor<int> {
                .roomHandler = cantDelete("room"),
                .bookcaseHandler = cantDelete("bookcase"),
                .shelfHandler = cantDelete("shelf"),
                .bookHandler = cantDelete("book"),
                .tableHandler = cantDelete("table"),
                .basketHandler = cantDelete("basket"),
                .noteHandler = [&](auto note) {
                    NoteContainer *owner = note->getOwner();
                    owner->deleteNote(note->getName());
                    return 0;
                }
        }.visit(entity);
    } catch (std::exception &e) {
        std::cerr << "Something went wrong: " << e.what() << std::endl;
        return -ENOENT;
    }
}

int bablib_rmdir(const char *path) {
    std::cerr << "bablib_unlink path=" << path << std::endl;
    try {
        LibraryEntity entity = library->resolve(path);
        auto cantDelete = [&](std::string kind)  {
            return [&, kind](auto) {
                std::cerr << "Cannot delete " << kind << "!" << std::endl;
                if (!isContainer(entity))
                    return -ENOTDIR;
                else
                    return -EACCES;
            };
        };
        return LibraryEntityVisitor<int> {
                .roomHandler = cantDelete("room"),
                .bookcaseHandler = cantDelete("bookcase"),
                .shelfHandler = cantDelete("shelf"),
                .bookHandler = cantDelete("book"),
                .tableHandler = cantDelete("table"),
                .basketHandler = [&](auto basket) {
                    Table *table = basket->getOwnerTable();
                    table->deleteBasket(basket->getName());
                    return 0;
                },
                .noteHandler = cantDelete("note")
        }.visit(entity);
    } catch (std::exception &e) {
        std::cerr << "Something went wrong: " << e.what() << std::endl;
        return -ENOENT;
    }
}

int bablib_mkdir(const char * path, mode_t) {
    std::cerr << "bablib_mkdir path=" << path << std::endl;
    try {
        auto spath = std::string(path);
        auto parent = library->resolve(removeLastToken(spath));
        std::cerr << spath << ' ' << removeLastToken(spath) << ' ' << getLastToken(spath) << '\n';
        auto name = getLastToken(spath);
        if (auto table = std::get_if<Table*>(&parent)) {
            auto contained = getContainedEntities(*table);
            if (std::find(contained.begin(), contained.end(), name) != contained.end()) {
                std::cerr << "name is already taken" << std::endl;
                return -EINVAL;
            }
            (*table)->createBasket(name);
        } else {
            std::cerr << "You may only create baskets inside a table" << std::endl;
            return -EINVAL;
        }
    } catch (std::exception &e) {
        std::cerr << "Something went wrong: " << e.what() << std::endl;
        return -ENOENT;
    }
    return 0;
}

int bablib_create(const char *path, mode_t, struct fuse_file_info *) {
    std::cerr << "bablib_create path=" << path << std::endl;
    try {
        auto spath = std::string(path);
        auto parent = library->resolve(removeLastToken(spath));
        auto name = getLastToken(spath);
        auto creator = [&](auto noteContainer) -> int {
            auto contained = getContainedEntities(*noteContainer);
            if (std::find(contained.begin(), contained.end(), name) != contained.end()) {
                std::cerr << "name is already taken" << std::endl;
                return -EINVAL;
            }
            (*noteContainer)->createNote(name);
            return 0;
        };
        if (auto table = std::get_if<Table*>(&parent)) {
            return creator(table);
        } else if (auto basket = std::get_if<Basket*>(&parent)) {
            return creator(basket);
        } else {
            std::cerr << "You may only create baskets inside a table" << std::endl;
            return -EINVAL;
        }
    } catch (std::exception &e) {
        std::cerr << "Something went wrong: " << e.what() << std::endl;
        return -ENOENT;
    }
    return 0;
}

int bablib_write(const char *path, const char *buf, size_t size, off_t offset,
		      struct fuse_file_info *) {
    std::cerr << "bablib_write path=" << path << std::endl;
    try {
        LibraryEntity entity = library->resolve(path);
        auto cantWrite = [&](std::string kind) -> auto {
            return [&, kind](auto) {
                std::cerr << "Cannot write " << kind << "!" << std::endl;
                size = 0;
            };
        };
        LibraryEntityVisitor<void>{
            .roomHandler = cantWrite("room"),
            .bookcaseHandler = cantWrite("bookcase"),
            .shelfHandler = cantWrite("shelf"),
            .bookHandler = cantWrite("book"),
            .tableHandler = cantWrite("table"),
            .basketHandler = cantWrite("basket"),
            .noteHandler = [&](Note *note) -> auto {
                auto content = note->getContent();
                if (content.size() < offset + size)
                    content.resize(size + offset);
                for (size_t i = 0; i < size; ++i)
                    content[offset + i] = buf[i];
                note->setContent(content);
            },
        }.visit(entity);
    } catch (std::exception &e) {
        std::cerr << "Something went wrong: " << e.what() << std::endl;
        return -ENOENT;
    }
    return size;
}

int bablib_truncate(const char *path, off_t offset, struct fuse_file_info *fi) {
    std::cerr << "bablib_truncate path=" << path << std::endl;
    try {
        LibraryEntity entity = library->resolve(path);
        auto cantTruncate = [&](std::string kind)  {
            return [&, kind](auto) {
                std::cerr << "Cannot truncate " << kind << "!" << std::endl;
                if (isContainer(entity))
                    return -EISDIR;
                else
                    return -EACCES;
            };
        };
        LibraryEntityVisitor<void> {
                .roomHandler = cantTruncate("room"),
                .bookcaseHandler = cantTruncate("bookcase"),
                .shelfHandler = cantTruncate("shelf"),
                .bookHandler = cantTruncate("book"),
                .tableHandler = cantTruncate("table"),
                .basketHandler = cantTruncate("basket"),
                .noteHandler = [&](Note* note) {
                    if (offset < note->getContent().length()) {
                        std::string oldContent = note->getContent();
                        note->setContent(oldContent.substr(0, offset));
                    }
                }
        }.visit(entity);
    } catch (std::exception &e) {
        std::cerr << "Something went wrong: " << e.what() << std::endl;
        return -ENOENT;
    }
    return 0;   
    
}

int main(int argc, char *argv[])
{
	int ret;
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

    library = std::make_shared<Library>(10, 0);

    const struct fuse_operations bablib_oper = {
        .getattr	= bablib_getattr,
        .mkdir      = bablib_mkdir,
        .unlink     = bablib_unlink,
        .rmdir      = bablib_rmdir,
        .rename     = bablib_rename,
        .truncate   = bablib_truncate,
        // .open		= bablib_open,
        .read		= bablib_read,
        .write      = bablib_write,
        .readdir	= bablib_readdir,
        .init       = bablib_init,
        .create     = bablib_create,
    };

	/* Parse options 
	if (fuse_opt_parse(&args, &options, option_spec, NULL) == -1)
		return 1;*/

	/* When --help is specified, first print our own file-system
	   specific help text, then signal fuse_main to show
	   additional help (by adding `--help` to the options again)
	   without usage: line (by setting argv[0] to the empty
	   string)
	if (options.show_help) {
		show_help(argv[0]);
		assert(fuse_opt_add_arg(&args, "--help") == 0);
		args.argv[0][0] = '\0';
	}*/

	ret = fuse_main(args.argc, args.argv, &bablib_oper, NULL);
	fuse_opt_free_args(&args);
	return ret;
}
