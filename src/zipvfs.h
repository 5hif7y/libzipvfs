#ifndef ZIPVFS_H
#define ZIPVFS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct zipvfs_t zipvfs_t;

// VFS Lifecycle
zipvfs_t *zipvfs_open(const char *archive_path, char mode);
void zipvfs_close(zipvfs_t *vfs);

// Mounting
int zipvfs_mount(zipvfs_t *vfs, const char *target_dir);
int zipvfs_umount(zipvfs_t *vfs);

// Direct Virtual File I/O
int zipvfs_read(zipvfs_t *vfs, const char *virtual_path, void **out_data, size_t *out_size);
int zipvfs_write(zipvfs_t *vfs, const char *virtual_path, const void *data, size_t size);
int zipvfs_delete(zipvfs_t *vfs, const char *virtual_path);
int zipvfs_list(zipvfs_t *vfs, const char *virtual_dir, char ***out_names, size_t *out_count);

#ifdef __cplusplus
}
#endif

#endif // ZIPVFS_H
