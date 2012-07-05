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
#include <apr_tables.h>
#include <apr_hash.h>
#include <stdlib.h>
#include <syslog.h>

/*
 * Returns the string value assigned to a property at a given path target (or NULL).
 * The return value of this function is volatile, you should copy it if you intend
 * to keep it around for any length of time.
 */
void *svnclient_property_for_path(char *path, char *propname)
{
    svn_opt_revision_t *rev;
    apr_hash_index_t *hi = NULL;
    apr_hash_t *hashmap = NULL;
    void *key;
    void *data;
    rev = malloc(sizeof(rev));

    if ( rev != NULL ) {
      rev->kind = svn_opt_revision_head;
      // Get the ownership information from the svnfs: properties.
      svn_client_propget(&hashmap,
			 propname,
			 path,
			 rev,
			 FALSE,
			 ctx,
			 pool);
      if ( apr_hash_count(hashmap) < 1 )
	return NULL;

      hi = apr_hash_first(pool, hashmap);
      if ( hi == NULL )
	return NULL;
      apr_hash_this(apr_hash_first(pool, hashmap), &key, NULL, &data);
      free(rev);
      return ((svn_string_t *)data)->data;
    }
}

/*
 * Exec a shell command (via /bin/sh) and return its stdout
 */
int svnclient_popen_output(char *cmd, char *buf, int readsize, int nreads)
{
  FILE *fp = popen(cmd, "r");
  int reads = 0;
  if ( fp == NULL )
    return 0;
  reads = fread(buf, readsize, nreads, fp);
  pclose(fp);
  return reads * readsize;
}

/*
 * Places the uid for the given username in 'uid'.
 * If the username is not found, returns 1, meaning 'uid'
 * contains an invalid value (NULL).
 */ 
int svnclient_uid_for_username(char *username, int *uid)
{
  int retcode = 1;
  if ( uid == NULL )
    return 1;
  char *cmdbuf = malloc(1024);
  if ( cmdbuf == NULL ) {
    goto svnclient_uid_for_username_out_of_memory;
  }
  char *buf = malloc(256);
  if ( buf == NULL ) {
    free(cmdbuf);
    goto svnclient_uid_for_username_out_of_memory;
  }

  sprintf(cmdbuf, "getent passwd %s", username);

  if ( svnclient_popen_output(cmdbuf, buf, 1, 512) == 0 ) {
    goto svnclient_uid_for_username_clean_exit;
  }
  

  char *tok = strtok(buf, ":");
  tok = strtok(NULL, ":");
  tok = strtok(NULL, ":");

  *uid = strtol(tok, NULL, 10);
  retcode = 0;

  svnclient_uid_for_username_clean_exit:
      free(buf);
      free(cmdbuf);
      return retcode;
  svnclient_uid_for_username_out_of_memory:
      DEBUG("Out of memory in svnclient_uid_for_username!");
      *uid = 0;
      return retcode;
}

/*
 * Places the gid for the given groupname in 'gid'.
 * If the groupname is not found, returns 1, meaning 'gid'
 * contains an invalid value (NULL).
 */ 
int svnclient_gid_for_groupname(char *groupname, int *gid)
{
  int retcode = 1;
  if ( gid == NULL )
    return 1;
  char *cmdbuf = malloc(1024);
  if ( cmdbuf == NULL ) {
    goto svnclient_gid_for_groupname_out_of_memory;
  }
  char *buf = malloc(256);
  if ( buf == NULL ) {
    free(cmdbuf);
    goto svnclient_gid_for_groupname_out_of_memory;
  }

  sprintf(cmdbuf, "getent group %s", groupname);

  if ( svnclient_popen_output(cmdbuf, buf, 1, 512) == 0 ) {
    goto svnclient_gid_for_groupname_clean_exit;
  }
  
  char *tok = strtok(buf, ":");
  tok = strtok(NULL, ":");
  tok = strtok(NULL, ":");

  *gid = strtol(tok, NULL, 10);
  retcode = 0;

  svnclient_gid_for_groupname_clean_exit:
      free(buf);
      free(cmdbuf);
      return retcode;
  svnclient_gid_for_groupname_out_of_memory:
      DEBUG("Out of memory in svnclient_gid_for_groupname!");
      *gid = 0;
      return retcode;
}

/*
 * Returns the file mode for the given path
 */
int svnclient_mode_for_path(char *path)
{
  char *mode = NULL;
  if ( (mode = svnclient_property_for_path(path, "svnfs:mode")) != NULL)
    return strtol(mode, NULL, 8);
  return 0775;
}

/*
 * Returns the uid of the owner for the given path according to svnfs:owner_user
 */
int svnclient_uid_for_path(char *path)
{
  char *username = NULL;
  int uid = 0;
  if ( (username = svnclient_property_for_path(path, "svnfs:owner_user")) != NULL ) {
    svnclient_uid_for_username(username, &uid);
    return uid;
  }
  return 0;
}

/*
 * Returns the gid of the owner for the given path according to svnfs:owner_group
 */
int svnclient_gid_for_path(char *path)
{
  char *groupname = NULL;
  int gid = 0;
  if ( (groupname = svnclient_property_for_path(path, "svnfs:owner_group")) != NULL) {
    svnclient_gid_for_groupname(groupname, &gid);
    return gid;
  }
  return 0;
}

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
    svn_error_t *retval = SVN_NO_ERROR;
    struct dirbuf *dp;
    char *abspath;
    char *fullpath;
    int slash_idx;
    char found = 0;
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

    if( (abspath = malloc(strlen(svnfs.svnpath) + strlen(fullpath) + 1)) == NULL )
        return(EIO);
    sprintf(abspath, "%s%s", svnfs.svnpath, fullpath);
    while( abspath[strlen(abspath)-1] == '/' )
        abspath[strlen(abspath)-1] = '\0';

    DEBUG("_svnclient_list_func(): abspath %s from %s", abspath, fullpath);

    slash_idx = (strlen(fullpath) - strlen(rindex(fullpath, '/')));

    /* If this file is already in the dirbuf cache, update stats */
    dp = first;
    // FIXME: Refactor this loop out into a generic function that returns
    // dp when it's already in the cache, NULL otherwise before this becomes 
    // a bad pattern.
    while( dp && dp->name ) {
        if( dp->name[slash_idx] != '/' ) {
            dp = dp->next;
            continue;
        }
        if( !(strncmp(dp->name, fullpath, strlen(fullpath))) ) {
            dp->st.st_size = dirent->size;
	    dp->st.st_mtime = apr_to_time_t(dirent->time);
            attr->dp = dp;
	    found = 1;
	    break;
        }
        dp = dp->next;
    }
    if ( found == 0 ) {
      dp->name = strdup(fullpath);

      dp->st.st_size = dirent->size;
      dp->st.st_mtime = apr_to_time_t(dirent->time);
      DEBUG("_svnclient_list_func(): st_mtime = %d dirent->time = %ld", dp->st.st_mtime, dirent->time);

      DEBUG("_svnclient_list_func(): added %s", dp->name);

      /* Point attr->dp at the newly created element */
      attr->dp = dp;

      /* Deliberately allocating a new struct here. This is required for
       * subsequent lists so that dp is allocated after finishing the
       * while() loop above */
      if( (dp->next = malloc(sizeof(struct dirbuf))) == NULL ) {
	retval = svn_error_create(SVN_ERR_FS_GENERAL, NULL, strerror(errno));
	goto _svnclient_list_func_early_exit;
      }
    }

    dp->st.st_uid = svnclient_uid_for_path(abspath);
    dp->st.st_gid = svnclient_gid_for_path(abspath);
    if( dirent->kind == svn_node_file )
      dp->st.st_mode = S_IFREG | svnclient_mode_for_path(abspath);
    else
      dp->st.st_mode = S_IFDIR | svnclient_mode_for_path(abspath);

_svnclient_list_func_early_exit:
    if (dp->next) {
      dp = dp->next;
      dp->next = NULL;
      dp->name = NULL;
    }
    
    if (abspath)
      free(abspath);
    if ( fullpath )
      free(fullpath);
    return retval;
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
