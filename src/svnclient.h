/*
 * $Id: svnclient.h 30 2007-06-18 21:45:21Z john $
 *
 *     SVN Filesystem
 *     Copyright (C) 2006 John Madden <maddenj@skynet.ie>
 *
 *     This program can be distributed under the terms of the GNU GPL.
 *     See the file COPYING for details.
*/

/* vim "+set tabstop=4 shiftwidth=4 expandtab" */
#ifndef _HAVE_SVNCLIENT_H
#define _HAVE_SVNCLIENT_H 1

#include <apr.h>
#include <apr_pools.h>

#include <svn_types.h>
#include <svn_client.h>
#include <svn_pools.h>
#include <svn_cmdline.h>
#include <svn_config.h>
#include <svn_io.h>
#include <svn_error.h>

#include "svnfs.h"

struct svnfs_attr {
    char *path;
    struct dirbuf *dp;
};

svn_client_ctx_t *ctx;
apr_pool_t *pool;

int svnclient_setup_ctx(void);

int svnclient_list(const char *path, struct dirbuf *dp);

int svnclient_read(struct dirbuf *dp, char *buf, size_t *size, off_t offset);

#endif /* ifndef _HAVE_SVNCLIENT_H */
