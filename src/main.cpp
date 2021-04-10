#define FUSE_USE_VERSION 31
#include <iostream>
#include <cstring>
#include <fuse3/fuse.h>

#include "bablib.h"

std::shared_ptr<Library> lib = std::make_shared<Library>(10, 0);

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
            stbuf->st_mode = S_IFREG | 0444;
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


static const struct fuse_operations bablib_oper = {
	.getattr	= bablib_getattr,
    .open		= NULL,
    .read		= NULL,
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
