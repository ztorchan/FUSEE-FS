#ifndef FUSEEFS_CLIENT_POSIXWRAPPER_INCLUDE_
#define FUSEEFS_CLIENT_POSIXWRAPPER_INCLUDE_

#ifdef __cplusplus
extern "C" {
#endif

void InitWrapper();

void DestroyWrapper();

int fuseefs_mkdir(const char *path, mode_t mode);

int fuseefs_rmdir(const char *path);

int fuseefs_creat(const char *path, mode_t mode);

int fuseefs_unlink(const char *path);

int fuseefs_stat(const char* path, struct stat* st);

int fuseefs_open64(const char* path, int flags, mode_t mode);

// int fuseefs_rename();

#ifdef __cplusplus
}
#endif

#endif // FUSEEFS_CLIENT_POSIXWRAPPER_INCLUDE_