/* This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* QA shim: the shape of devkitARM's newlib <sys/dirent.h> (stock arm-none-eabi newlib has none). */
#pragma once
#include <sys/types.h>
#include <sys/syslimits.h>
#ifdef __cplusplus
extern "C" {
#endif
#define DT_UNKNOWN 0
#define DT_FIFO 1
#define DT_CHR 2
#define DT_DIR 4
#define DT_BLK 6
#define DT_REG 8
#define DT_LNK 10
#define DT_SOCK 12
#define DT_WHT 14
struct dirent { ino_t d_ino; unsigned char d_type; char d_name[NAME_MAX + 1]; };
typedef struct { long position; void* dirData; struct dirent fileData; } DIR;
DIR* opendir(const char*); struct dirent* readdir(DIR*); int closedir(DIR*); void rewinddir(DIR*);
int readdir_r(DIR*, struct dirent*, struct dirent**); long telldir(DIR*); void seekdir(DIR*, long);
#ifdef __cplusplus
}
#endif
