/*
 * $Id: svnfs.c 30 2007-06-18 21:45:21Z john $
 *
 *     SVN Filesystem
 *     Copyright (C) 2006 John Madden <maddenj@skynet.ie>
 *
 *     This program can be distributed under the terms of the GNU GPL.
 *     See the file COPYING for details.
*/

/* vim "+set tabstop=4 shiftwidth=4 expandtab" */

#ifdef linux
#define _XOPEN_SOURCE 500
#endif

#define FUSE_USE_VERSION 25

#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif

#include <sys/time.h>

#include "svnfs.h"
#include "svnclient.h"

/* Debug logging */
void DEBUG(char *fmt, ...) {
    if( svnfs.debug ) {
        va_list ap;

        va_start(ap, fmt);
        printf("DEBUG : ");
        vprintf(fmt, ap);
        printf("\n");
    }
}
/* svnfs specific options */

#define SVNFS_OPT(a, b, c) { a, offsetof(struct svnfs, b), c }

static struct fuse_opt svnfs_opts[] = { 
    SVNFS_OPT( "debug", debug, 1 ),
    FUSE_OPT_END
};

/* Filesystem functions */

static int svnfs_getattr(const char *path, struct stat *buf) {
    struct dirbuf *dp = first;
    int err;

    DEBUG("svnfs_getattr(): path : '%s'", path);

    memset(buf, 0, sizeof(struct stat));

    if( strcmp(path, "/") == 0 ) {
        buf->st_mode = S_IFDIR | 0755; /* 34236 */
        buf->st_nlink = 2;
        return(0);
    } else {
        while( dp && dp->name ) {
            if( !(strncmp(path, dp->name, strlen(path))) ) {
                buf->st_mode = dp->st.st_mode;
                buf->st_size = dp->st.st_size;
                return(0);
            }
            dp = dp->next;
        }
    }

    /* Need to check the repository - not in the cache */
    if( (err = svnclient_list(path, dp)) ) {
        return(-err);
    }

    buf->st_mode = dp->st.st_mode;
    buf->st_size = dp->st.st_size;
    return(0);
}

static int svnfs_open(const char *path, struct fuse_file_info *fi) {
    struct dirbuf *dp = first;

    (void) fi;

    DEBUG("svnfs_open(): path : %s", path);

    while( dp && dp->name ) {
        if( !(strcmp(dp->name, path)) ) {
            DEBUG("svnfs_open(): %s", dp->name);
            return(0);
        }
        dp = dp->next;
    }

    return(-ENOENT);
}

static int svnfs_read(const char *path, char *buf, size_t size, 
       off_t offset, struct fuse_file_info *fi) {
    struct dirbuf *dp = first;
    int err;
    (void) fi;

    DEBUG("svnfs_read(): %d from %s, offset %d", size, path, offset);
    while( dp && dp->name ) {
        if( !(strcmp(dp->name, path)) ) {
            if( offset < dp->st.st_size ) {
                if( offset + size > dp->st.st_size )
                    size = dp->st.st_size - offset;
            } else {
                size = 0;
            }
            break;
        }
        dp = dp->next;
    }

    if( (err = svnclient_read(dp, buf, &size, offset)) ) {
        return(-err);
    }

    DEBUG("svnfs_read(): size %d", size);
    return(size);
}

static int svnfs_readdir(const char *path, void *buf, 
       fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    struct dirbuf *dp = NULL;
    int err;

    (void)fi;

    DEBUG("svnfs_readdir(): path : '%s'", path);

    if( (err = svnclient_list(path, dp)) ) {
        return(-err);
    }

    /* The dirbuf linked list only gets populated by svnclient_list() */
    dp = first; 

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    while( dp && dp->name ) {
        if( strlen(path) > 1 ) { /* path is other than '/' */

            /* If the start of the dirbuf path matches the requested path 
             *
             * And the length of the dirbuf path is greater than the 
             * requested path 
             *
             * And the length of the requested path matches the length of 
             * the dirbuf path up to the last '/' character (prevents 
             * returning subdirectory contents) */
            if( (strncmp(dp->name, path, strlen(path)) == 0) &&
                    (strlen(dp->name) > strlen(path)) &&
                    ((strlen(path)) == 
                     (strlen(dp->name) - strlen(rindex(dp->name, '/')))) ) {
                if( filler(buf, (rindex(dp->name, '/') + 1), &(dp->st), 0) )
                    break;
            }
        } else { /* path is '/' */
            if( strstr((dp->name + 1), "/") == NULL ) {
                if( filler(buf, (dp->name + 1), &(dp->st), 0) )
                    break;
            }
        }
        dp = dp->next;
    }

    return(0);
}

/* End filesystem functions */

static struct fuse_operations svnfs_oper = {
    /*
    .mknod = svnfs_mknod,
    .mkdir = svnfs_mkdir,
    .unlink = svnfs_unlink,
    .rmdir = svnfs_rmdir,
    .symlink = svnfs_symlink,
    .rename = svnfs_rename,
    .link = svnfs_link,
    .chmod = svnfs_chmod,
    .truncate = svnfs_truncate,
    .write = svnfs_write,
    .readlink = svnfs_readlink,
    .chown = svnfs_chown,
    .utime = svnfs_utime,
    .statfs = svnfs_statfs,
    */
    .getattr = svnfs_getattr,
    .open = svnfs_open,
    .read = svnfs_read,
    .readdir = svnfs_readdir
};

int svnfs_parse_opts(void *data, const char *arg, int key, 
       struct fuse_args *outargs) {

    switch(key) {
        case FUSE_OPT_KEY_OPT:
            /* svnfs specific options */
            break;
       
        case FUSE_OPT_KEY_NONOPT:
            if( !svnfs.svnpath ) {
                svnfs.svnpath = strdup(arg);
                return(0);
            }
            return(1);

        default :
            fprintf(stderr, "Error, unknown option %s\n", arg);
            return(1);
    }

    /* Silence compiler warning about reaching end of non-void function */
    return(1);
}

int main(int argc, char *argv[]) {

    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

    svnfs.debug = 0;

    if( fuse_opt_parse(&args, &svnfs, svnfs_opts, svnfs_parse_opts) == -1 ) {
        fprintf(stderr, "Error parsing command line arguments\n");
        exit(1);
    }

    if( !svnfs.svnpath ) {
        fprintf(stderr, "No Subversion repository given\n");
        exit(1);
    }
    while( svnfs.svnpath[strlen(svnfs.svnpath)-1] == '/' )
        svnfs.svnpath[strlen(svnfs.svnpath)-1] = '\0';

    gettimeofday(&svnfs.mnttime, NULL);

    DEBUG("struct svnfs = {");
    DEBUG("\tdebug = %d", svnfs.debug);
    DEBUG("\tsvnpath = %s", svnfs.svnpath);
    DEBUG("\tmnttime.tv_sec = %d", svnfs.mnttime.tv_sec);
    DEBUG("}");

    if( svnclient_setup_ctx() ) {
        exit(1);
    }

    if( (first = malloc(sizeof(struct dirbuf))) == NULL ) {
        fprintf(stderr, "Error allocating memory - %s\n", strerror(errno));
        exit(1);
    }
    first->name = NULL;
    first->next = NULL;

    return(fuse_main(args.argc, args.argv, &svnfs_oper));
}
