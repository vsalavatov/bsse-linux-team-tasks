#include <iostream>
#include <fuse/fuse.h>

#include "bablib.h"


Library lib(10, 0);

static int bablib_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    int res;
    lib->resolve(path);
    if () {
        stbuf->st_mode = S_IFDIR | 0555; // TODO
        // stbuf->st_nlink = 2;
    } else {
        res = -ENOENT;
    }
    return res;
}


int main() {

}
