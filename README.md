# libzipvfs (VFS API Middleware)

A lightweight, portable Virtual File System (VFS) API middleware written in C, built on top of the awesome [kuba--/zip](https://github.com/kuba--/zip) library.

It allows you to virtually mount a `.zip` archive into the operating system's filesystem, perform abstract read, write, and delete operations on it, and unmount it—automatically consolidating all modifications back into the `.zip` archive.

## License

Like the underlying `kuba--/zip` library, `libzipvfs` is licensed under the **MIT License** (see `LICENSE.txt`).

## Submodules & Dependencies

The library references the original `kuba--/zip` codebase as a Git submodule under `kuba-zip`.

## VFS Middleware API Reference

```c
// Open and initialize a VFS handle associated with a .zip archive file path
zipvfs_t *zipvfs_open(const char *zip_path);

// Virtually mount the .zip archive into the specified directory
int zipvfs_mount(zipvfs_t *vfs, const char *mount_dir);

// Unmount the VFS and synchronize all virtual modifications back into the .zip archive
int zipvfs_umount(zipvfs_t *vfs);

// Read a file's content from the virtual mount or the associated zip file
int zipvfs_read(zipvfs_t *vfs, const char *path, void **out_buf, size_t *out_size);

// Write data from a buffer to a specific file path within the VFS virtual mount
int zipvfs_write(zipvfs_t *vfs, const char *path, const void *buf, size_t size);

// Delete a virtual file from the VFS mount
int zipvfs_delete(zipvfs_t *vfs, const char *path);

// List all files contained in the VFS mount
int zipvfs_list(zipvfs_t *vfs, char ***out_paths, int *out_count);

// Free all resources and close the VFS handle
void zipvfs_close(zipvfs_t *vfs);
```

## How to build (CMake)

Include this directory in your CMake configuration, and link the `zipvfs` target to your application.
```cmake
add_subdirectory(libzipvfs)
target_link_libraries(your_project PRIVATE zipvfs)
```
