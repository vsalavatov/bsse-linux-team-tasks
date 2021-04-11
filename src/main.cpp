#define FUSE_USE_VERSION 31
#include <iostream>
#include <cstring>
#include <fuse3/fuse.h>

#include "bablib.h"

std::shared_ptr<Library> lib = std::make_shared<Library>(10, 0);

const char alphabet[28] = {'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z','.',','};

uint64_t hash(const std::string &str) {
    uint64_t hash = 5381;
    for (char c: str)
        hash = ((hash << 5) + hash) + c;
    return hash;
}

uint64_t xorshift64(uint64_t *state) {
    if (*state == 0) {
        *state = 1;
    }

    uint64_t x = *state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    return *state = x;
}

static void *bablib_init(struct fuse_conn_info *conn, struct fuse_config *cfg) {
    std::cerr << "bablib_init" << std::endl;
	(void) conn;
	cfg->kernel_cache = 0;
	return NULL;
}

static int bablib_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    int res = 0;
    std::cerr << "bablib_getattr path=" << path << std::endl;

    memset(stbuf, 0, sizeof(stbuf));
    try {
        LibraryEntity entity = lib->resolve(path);
        if (isContainer(entity)) {
            stbuf->st_mode = S_IFDIR | 0555;
        } else {
            stbuf->st_mode = S_IFREG | 0555;
            stbuf->st_nlink = 1;
            stbuf->st_size = BOOK_SIZE;
        }
    } catch (std::exception &e) {
        std::cerr << "Something went wrong: " << e.what() << std::endl;
        res = -ENOENT;
    }

    return res;
}

static int bablib_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi,
                         enum fuse_readdir_flags flags)
{
    std::cerr << "bablib_readdir path=" << path << std::endl;
    (void) offset;
    (void) fi;
    (void) flags;
    
    int res = 0;
    try {
        LibraryEntity resolvedEntity = lib->resolve(path);
        if (isContainer(resolvedEntity)) {
            auto contained = getContainedEntities(resolvedEntity);
            if (std::get_if<Room*>(&resolvedEntity)) {
                
            }
            for (const auto& name : contained)
                filler(buf, name.c_str(), NULL, 0, fuse_fill_dir_flags(0));
         }
        
    } catch (std::exception &e) {
        std::cerr << "Something went wrong: " << e.what() << std::endl;
        res = -ENOENT;
    }
    return res;
}

static int bablib_open(const char *path, struct fuse_file_info *fi)
{
    std::cerr << "bablib_open path=" << path << std::endl;
    try {
        LibraryEntity entity = lib->resolve(path);
        if (!isContainer(entity)) {
            if ((fi->flags & O_ACCMODE) != O_RDONLY)
                return -EACCES;
        } else
            return -EISDIR;
    } catch (std::exception &e) {
        std::cerr << "Something went wrong: " << e.what() << std::endl;
        return -ENOENT;
    }
    return 0;
}

static int bablib_read(const char *path, char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi)
{
    std::cerr << "bablib_read path=" << path << std::endl;
    (void) fi;
    try {
        LibraryEntity entity = lib->resolve(path);

        if (!isContainer(entity)) {
            uint64_t state = hash(getName(entity));
            if (offset < BOOK_SIZE) {
                size = std::min(size, BOOK_SIZE - offset);
                // std::cerr << size << std::endl; TODO не понимаю, почему там такой size выводится
                for (int i = 0; i < offset; ++i) {
                    xorshift64(&state);
                }
                for (int i = 0; i < size; ++i) {
                    buf[i] = alphabet[xorshift64(&state) % 28];
                }
            } else {
                size = 0;
            }
        }
    } catch (std::exception &e) {
        std::cerr << "Something went wrong: " << e.what() << std::endl;
        return -ENOENT;
    }
    return size;
}

static const struct fuse_operations bablib_oper = {
	.getattr	= bablib_getattr,
    .open		= bablib_open,
    .read		= bablib_read,
    .readdir	= bablib_readdir,
    .init       = bablib_init,
};

int main(int argc, char *argv[])
{
	int ret;
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

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
