/* This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* QA shim: the shape of devkitARM's newlib <sys/iosupport.h> (stock arm-none-eabi newlib has none), syntax checks only. */
#pragma once
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/reent.h>
#ifdef __cplusplus
extern "C" {
#endif
struct statvfs; struct timeval;
typedef struct { void *dirStruct; } DIR_ITER;
typedef struct {
	const char *name;
	size_t structSize;
	int (*open_r)(struct _reent *r, void *fileStruct, const char *path, int flags, int mode);
	int (*close_r)(struct _reent *r, void *fd);
	ssize_t (*write_r)(struct _reent *r, void *fd, const char *ptr, size_t len);
	ssize_t (*read_r)(struct _reent *r, void *fd, char *ptr, size_t len);
	off_t (*seek_r)(struct _reent *r, void *fd, off_t pos, int dir);
	int (*fstat_r)(struct _reent *r, void *fd, struct stat *st);
	int (*stat_r)(struct _reent *r, const char *file, struct stat *st);
	int (*link_r)(struct _reent *r, const char *existing, const char *newLink);
	int (*unlink_r)(struct _reent *r, const char *name);
	int (*chdir_r)(struct _reent *r, const char *name);
	int (*rename_r)(struct _reent *r, const char *oldName, const char *newName);
	int (*mkdir_r)(struct _reent *r, const char *path, int mode);
	size_t dirStateSize;
	DIR_ITER *(*diropen_r)(struct _reent *r, DIR_ITER *dirState, const char *path);
	int (*dirreset_r)(struct _reent *r, DIR_ITER *dirState);
	int (*dirnext_r)(struct _reent *r, DIR_ITER *dirState, char *filename, struct stat *filestat);
	int (*dirclose_r)(struct _reent *r, DIR_ITER *dirState);
	int (*statvfs_r)(struct _reent *r, const char *path, struct statvfs *buf);
	int (*ftruncate_r)(struct _reent *r, void *fd, off_t len);
	int (*fsync_r)(struct _reent *r, void *fd);
	void *deviceData;
} devoptab_t;
const devoptab_t *GetDeviceOpTab(const char *name);
#ifdef __cplusplus
}
#endif
