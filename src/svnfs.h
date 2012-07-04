/*
 * $Id: svnfs.h 18 2007-06-04 15:56:03Z john $
 *
 *     SVN Filesystem
 *     Copyright (C) 2006 John Madden <maddenj@skynet.ie>
 *
 *     This program can be distributed under the terms of the GNU GPL.
 *     See the file COPYING for details.
*/

/* vim "+set tabstop=4 shiftwidth=4 expandtab" */
#ifndef _HAVE_SVNFS_H
#define _HAVE_SVNFS_H 1

#include <fuse.h>

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>

struct svnfs {
    int debug; /* Turn on debugging */
    char *svnpath; /* URL to Subversion repository */
    struct timeval mnttime; /* Mount time */
};
struct svnfs svnfs;

struct dirbuf {
    char *name;
    struct stat st;
    struct dirbuf *next;
};
struct dirbuf *dirbuf;
struct dirbuf *first;

#define SVNCLIENT_NO_ERROR 0

void DEBUG(char *fmt, ...);

#endif /* ifndef _HAVE_SVNFS_H */
