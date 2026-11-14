// SPDX-License-Identifier: GPL-3.0-or-later
// Windows: <sys/stat.h> plus the two POSIX spellings the shared code uses.
// mkdir() takes no mode there (a folder's access comes from its parent's),
// and there is no lstat(): stat() it is (the app makes no symbolic links).
#pragma once
#include_next <sys/stat.h>
#ifdef __cplusplus
#include <direct.h>
inline int mkdir(const char* path, int /*mode*/) { return ::_mkdir(path); }
inline int lstat(const char* path, struct stat* st) { return ::stat(path, st); }
#endif
