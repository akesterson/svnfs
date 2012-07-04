/*
 * $Id: svnclient.c 31 2007-06-20 19:40:30Z john $
 *
 *     SVN Filesystem
 *     Copyright (C) 2006 John Madden <maddenj@skynet.ie>
 *
 *     This program can be distributed under the terms of the GNU GPL.
 *     See the file COPYING for details.
*/

/* vim "+set tabstop=4 shiftwidth=4 expandtab" */

#include "svnfs.h"
#include "svnclient.h"

int svnclient_setup_ctx() {
    svn_auth_baton_t *auth_baton;
    apr_array_header_t *providers;
    svn_auth_provider_object_t *username_wc_provider;
    svn_error_t *err;
    char errbuf[1024];

    /* Initialise a pool */
    apr_initialize();
    pool = svn_pool_create(pool);

    /* Initialise errbuf */
    memset(&errbuf, '\0', 1024);

    /* Create a client context object */
    if( (err = svn_client_create_context(&ctx, pool)) ) {
        fprintf(stderr, "%s\n", svn_strerror(err->apr_err, errbuf, 1024));
        return(1);
    }

    if( (err = (svn_error_t *)svn_config_get_config(
                    &(ctx->config), NULL, pool)) ) {
        fprintf(stderr, "%s\n", svn_strerror(err->apr_err, errbuf, 1024));
        return(1);
    }

    providers = apr_array_make(pool, 1, sizeof(svn_auth_provider_object_t *));
    username_wc_provider = apr_pcalloc(pool, sizeof(*username_wc_provider));
    svn_client_get_username_provider(&username_wc_provider, pool);
    *(svn_auth_provider_object_t **)apr_array_push(providers) 
        = username_wc_provider;
    svn_auth_open(&auth_baton, providers, pool);
    ctx->auth_baton = auth_baton;

    return(0);
}

static svn_error_t *_svnclient_list_func(void *baton, const char *path,
        const svn_dirent_t *dirent, const svn_lock_t *lock,
        const char *abs_path, apr_pool_t *pool) {
    struct dirbuf *dp;
    char *fullpath;
    int slash_idx;
    struct svnfs_attr *attr = (struct svnfs_attr *)baton;

    /* The first 'path' argument is blank, meaning it's the details for
     * attr->path. If path length is 0 and attr->path length is 1, then
     * we're listing the '/' of the filesystem, for which a static
     * dirent is returned from svnfs_getattr(), so skip it */
    if( (strlen(path) == 0) && (strlen(attr->path) == 1) )
        return(SVN_NO_ERROR);

    /* path is a file under the directory */
    if( (strlen(path) > 0) && (strlen(attr->path) > 1) ) {
        if( (fullpath = malloc((sizeof(char)) * 
                        (strlen(attr->path) + strlen(path) + 2))) == NULL )
            return(svn_error_create(SVN_ERR_FS_GENERAL, NULL, strerror(errno)));
        sprintf(fullpath, "%s/%s", attr->path, path);
    } else {
        if( (fullpath = malloc((sizeof(char)) * 
                        (strlen(attr->path) + strlen(path) + 1))) == NULL )
            return(svn_error_create(SVN_ERR_FS_GENERAL, NULL, strerror(errno)));
        sprintf(fullpath, "%s%s", attr->path, path);
    }

    DEBUG("_svnclient_list_func(): path %s", fullpath);

    slash_idx = (strlen(fullpath) - strlen(rindex(fullpath, '/')));

    /* If this file is already in the dirbuf cache, update st_size */
    dp = first;
    while( dp && dp->name ) {
        if( dp->name[slash_idx] != '/' ) {
            dp = dp->next;
            continue;
        }
        if( !(strncmp(dp->name, fullpath, strlen(fullpath))) ) {
            free(fullpath);
            dp->st.st_size = dirent->size;
            attr->dp = dp;
            return(SVN_NO_ERROR);
        }
        dp = dp->next;
    }

    dp->name = strdup(fullpath);
    if( dirent->kind == svn_node_file )
        dp->st.st_mode = S_IFREG | 0644;
    else
        dp->st.st_mode = S_IFDIR | 0755;

    dp->st.st_size = dirent->size;

    DEBUG("_svnclient_list_func(): added %s", dp->name);

    /* Point attr->dp at the newly created element */
    attr->dp = dp;

    /* Deliberately allocating a new struct here. This is required for
     * subsequent lists so that dp is allocated after finishing the
     * while() loop above */
    if( (dp->next = malloc(sizeof(struct dirbuf))) == NULL )
        return(svn_error_create(SVN_ERR_FS_GENERAL, NULL, strerror(errno)));
    dp = dp->next;
    dp->next = NULL;
    dp->name = NULL;

    free(fullpath);
    return(SVN_NO_ERROR);
}

/* If a file isn't contained in the dirbuf cache, this will get called. 
 * Return the files result in *dp, and also append it to the dirbuf
 * cache */
int svnclient_list(const char *path, struct dirbuf *dp) {
    svn_opt_revision_t *rev;
    apr_uint32_t dirent_fields = SVN_DIRENT_KIND | SVN_DIRENT_SIZE;
    svn_error_t *err = NULL;
    struct svnfs_attr *attr;

    char *fullpath;

    attr = malloc(sizeof(struct svnfs_attr));
    attr->path = strdup(path);

    rev = malloc(sizeof(rev));
    rev->kind = svn_opt_revision_head;

    if( (fullpath = malloc(strlen(svnfs.svnpath) + strlen(path) + 1)) == NULL )
        return(EIO);
    sprintf(fullpath, "%s%s", svnfs.svnpath, path);
    while( fullpath[strlen(fullpath)-1] == '/' )
        fullpath[strlen(fullpath)-1] = '\0';

    DEBUG("svnclient_list(): '%s'", fullpath);

    if( (err = svn_client_list(fullpath, rev, rev, FALSE, dirent_fields, FALSE,
                _svnclient_list_func, (void *)attr, ctx, pool)) 
            != SVN_NO_ERROR ) {
        switch(err->apr_err) {
            case SVN_ERR_FS_NOT_FOUND:
                return(ENOENT);
            default:
                return(EIO);
        }
    }
    /* Point at the last created dirbuf element. This is primarily for
     * getattr() */
    dp = attr->dp;

    return(0);
}

int svnclient_read(struct dirbuf *dp, char *buf, size_t *size, off_t offset) {
    svn_opt_revision_t *rev;
    svn_stream_t *out;
    svn_stringbuf_t *sbuf;
    char *path;
    apr_pool_t *subpool = svn_pool_create(pool);
    svn_error_t *err = NULL;
    char *errbuf;

    sbuf = svn_stringbuf_create("", subpool);
    out = svn_stream_from_stringbuf(sbuf, subpool);

    path = malloc(strlen(svnfs.svnpath) + strlen(dp->name) + 1);
    sprintf(path, "%s%s", svnfs.svnpath, dp->name);

    rev = malloc(sizeof(rev));
    rev->kind = svn_opt_revision_head;

    if( (err = svn_client_cat2(out, path, rev, rev, ctx, subpool)) 
            == SVN_NO_ERROR ) {
        if( sbuf->len < *size ) {
            memcpy(buf, sbuf->data + offset, sbuf->len);
            *size = sbuf->len;
        } else {
            memcpy(buf, sbuf->data + offset, *size);
        }
        DEBUG("svnclient_read(): size %ld", sbuf->len);
    } else {
        errbuf = malloc(1024);
        DEBUG("svnclient_read(): svn_client_cat() - %s", 
                svn_strerror(err->apr_err, errbuf, 1024));
        free(errbuf);

        switch(err->apr_err) {
            case SVN_ERR_UNVERSIONED_RESOURCE:
            case SVN_ERR_ENTRY_NOT_FOUND:
                return(EEXIST);
            case SVN_ERR_CLIENT_IS_DIRECTORY:
                return(EISDIR);
            default:
                return(EIO);
        }
    }

    apr_pool_destroy(subpool);
    return(0);
}
