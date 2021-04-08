#include "bablib.h"

#include <functional>

/*
std::bind_front(&Library::lllljlkj, library)

fuse_operation ops {
     = std::bind_front(&FuseLibrary::fuse_getattr, library),
}

std::bind_front(bablib_getattr, library);
*/

Library lib(10, 0);

static int bablib_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    int res;

    if () {
        stbuf->st_mode = S_IFDIR | 0555; // TODO
        // stbuf->st_nlink = 2;
    } else {
        res = -ENOENT;
    }
    return res;
}

