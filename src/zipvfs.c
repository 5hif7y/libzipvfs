#include "zipvfs.h"
#include "zip.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
  #include <windows.h>
  #include <direct.h>
#else
  #include <unistd.h>
  #include <sys/stat.h>
  #include <sys/types.h>
  #include <dirent.h>
#endif

struct zipvfs_t {
    char archive_path[512];
    char mode;
    char mount_dir[512];
    int is_mounted;
};

static void ensure_dir_exists(const char* filepath) {
    char path[512];
    strncpy(path, filepath, sizeof(path) - 1);
    path[sizeof(path) - 1] = '\0';
    
    char* slash = strchr(path, '/');
    while (slash) {
        *slash = '\0';
#ifdef _WIN32
        CreateDirectoryA(path, NULL);
#else
        mkdir(path, 0777);
#endif
        *slash = '/';
        slash = strchr(slash + 1, '/');
    }
    
    slash = strchr(path, '\\');
    while (slash) {
        *slash = '\0';
#ifdef _WIN32
        CreateDirectoryA(path, NULL);
#endif
        *slash = '\\';
        slash = strchr(slash + 1, '\\');
    }
}

#ifdef _WIN32
static void add_dir_to_zip(struct zip_t *zip, const char *dir_path, const char *base_path) {
    char search_path[MAX_PATH];
    snprintf(search_path, sizeof(search_path), "%s\\*", dir_path);
    
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(search_path, &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;
    
    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
            continue;
            
        char full_path[MAX_PATH];
        snprintf(full_path, sizeof(full_path), "%s\\%s", dir_path, fd.cFileName);
        
        const char *relative_path = full_path + strlen(base_path);
        if (relative_path[0] == '\\' || relative_path[0] == '/') {
            relative_path++;
        }
        
        char zip_entry_name[MAX_PATH];
        strncpy(zip_entry_name, relative_path, sizeof(zip_entry_name));
        for (int i = 0; zip_entry_name[i]; i++) {
            if (zip_entry_name[i] == '\\') zip_entry_name[i] = '/';
        }
        
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            add_dir_to_zip(zip, full_path, base_path);
        } else {
            zip_entry_open(zip, zip_entry_name);
            zip_entry_fwrite(zip, full_path);
            zip_entry_close(zip);
        }
    } while (FindNextFileA(hFind, &fd));
    
    FindClose(hFind);
}

static void remove_dir_recursive(const char *dir_path) {
    char search_path[MAX_PATH];
    snprintf(search_path, sizeof(search_path), "%s\\*", dir_path);
    
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(search_path, &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;
    
    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
            continue;
            
        char full_path[MAX_PATH];
        snprintf(full_path, sizeof(full_path), "%s\\%s", dir_path, fd.cFileName);
        
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            remove_dir_recursive(full_path);
        } else {
            DeleteFileA(full_path);
        }
    } while (FindNextFileA(hFind, &fd));
    
    FindClose(hFind);
    RemoveDirectoryA(dir_path);
}
#else
// POSIX implementations if needed
static void add_dir_to_zip(struct zip_t *zip, const char *dir_path, const char *base_path) {
    DIR *d = opendir(dir_path);
    if (!d) return;
    struct dirent *dir;
    while ((dir = readdir(d)) != NULL) {
        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0)
            continue;
        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, dir->d_name);
        struct stat st;
        if (stat(full_path, &st) == 0) {
            const char *relative_path = full_path + strlen(base_path);
            if (relative_path[0] == '/') relative_path++;
            if (S_ISDIR(st.st_mode)) {
                add_dir_to_zip(zip, full_path, base_path);
            } else {
                zip_entry_open(zip, relative_path);
                zip_entry_fwrite(zip, full_path);
                zip_entry_close(zip);
            }
        }
    }
    closedir(d);
}

static void remove_dir_recursive(const char *dir_path) {
    DIR *d = opendir(dir_path);
    if (!d) return;
    struct dirent *dir;
    while ((dir = readdir(d)) != NULL) {
        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0)
            continue;
        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, dir->d_name);
        struct stat st;
        if (stat(full_path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                remove_dir_recursive(full_path);
            } else {
                unlink(full_path);
            }
        }
    }
    closedir(d);
    rmdir(dir_path);
}
#endif

zipvfs_t *zipvfs_open(const char *archive_path, char mode) {
    zipvfs_t *vfs = malloc(sizeof(zipvfs_t));
    if (!vfs) return NULL;
    
    strncpy(vfs->archive_path, archive_path, sizeof(vfs->archive_path) - 1);
    vfs->archive_path[sizeof(vfs->archive_path) - 1] = '\0';
    vfs->mode = mode;
    vfs->mount_dir[0] = '\0';
    vfs->is_mounted = 0;
    
    return vfs;
}

void zipvfs_close(zipvfs_t *vfs) {
    if (!vfs) return;
    if (vfs->is_mounted) {
        zipvfs_umount(vfs);
    }
    free(vfs);
}

int zipvfs_mount(zipvfs_t *vfs, const char *target_dir) {
    if (!vfs || vfs->is_mounted) return -1;
    
    strncpy(vfs->mount_dir, target_dir, sizeof(vfs->mount_dir) - 1);
    vfs->mount_dir[sizeof(vfs->mount_dir) - 1] = '\0';
    
    // Create mount directory
#ifdef _WIN32
    CreateDirectoryA(vfs->mount_dir, NULL);
#else
    mkdir(vfs->mount_dir, 0777);
#endif
    
    // If we are reading or appending, extract the archive first
    if (vfs->mode == 'r' || vfs->mode == 'a') {
        FILE *f = fopen(vfs->archive_path, "rb");
        if (f) {
            fclose(f);
            int res = zip_extract(vfs->archive_path, vfs->mount_dir, NULL, NULL);
            if (res < 0) return res;
        }
    }
    
    vfs->is_mounted = 1;
    return 0;
}

int zipvfs_umount(zipvfs_t *vfs) {
    if (!vfs || !vfs->is_mounted) return -1;
    
    if (vfs->mode == 'w' || vfs->mode == 'a') {
        struct zip_t *zip = zip_open(vfs->archive_path, ZIP_DEFAULT_COMPRESSION_LEVEL, 'w');
        if (zip) {
            add_dir_to_zip(zip, vfs->mount_dir, vfs->mount_dir);
            zip_close(zip);
        }
    }
    
    // Clean up temporary files on disk
    remove_dir_recursive(vfs->mount_dir);
    vfs->is_mounted = 0;
    
    return 0;
}

int zipvfs_read(zipvfs_t *vfs, const char *virtual_path, void **out_data, size_t *out_size) {
    if (!vfs || !vfs->is_mounted) return -1;
    
    char full_path[512];
    snprintf(full_path, sizeof(full_path), "%s/%s", vfs->mount_dir, virtual_path);
    
    FILE *f = fopen(full_path, "rb");
    if (!f) return -1;
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    void *buf = malloc(size);
    if (!buf) {
        fclose(f);
        return -1;
    }
    
    size_t read_bytes = fread(buf, 1, size, f);
    fclose(f);
    
    if (read_bytes != (size_t)size) {
        free(buf);
        return -1;
    }
    
    *out_data = buf;
    *out_size = (size_t)size;
    return 0;
}

int zipvfs_write(zipvfs_t *vfs, const char *virtual_path, const void *data, size_t size) {
    if (!vfs || !vfs->is_mounted) return -1;
    
    char full_path[512];
    snprintf(full_path, sizeof(full_path), "%s/%s", vfs->mount_dir, virtual_path);
    
    ensure_dir_exists(full_path);
    
    FILE *f = fopen(full_path, "wb");
    if (!f) return -1;
    
    size_t written = fwrite(data, 1, size, f);
    fclose(f);
    
    return (written == size) ? 0 : -1;
}

int zipvfs_delete(zipvfs_t *vfs, const char *virtual_path) {
    if (!vfs || !vfs->is_mounted) return -1;
    
    char full_path[512];
    snprintf(full_path, sizeof(full_path), "%s/%s", vfs->mount_dir, virtual_path);
    
    return remove(full_path);
}

int zipvfs_list(zipvfs_t *vfs, const char *virtual_dir, char ***out_names, size_t *out_count) {
    if (!vfs || !vfs->is_mounted) return -1;
    
    char full_path[512];
    snprintf(full_path, sizeof(full_path), "%s/%s", vfs->mount_dir, virtual_dir);
    
    char **names = NULL;
    size_t count = 0;
    
#ifdef _WIN32
    char search_path[512];
    snprintf(search_path, sizeof(search_path), "%s\\*", full_path);
    
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(search_path, &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
                continue;
                
            names = realloc(names, sizeof(char*) * (count + 1));
            names[count] = _strdup(fd.cFileName);
            count++;
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }
#else
    DIR *d = opendir(full_path);
    if (d) {
        struct dirent *dir;
        while ((dir = readdir(d)) != NULL) {
            if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0)
                continue;
            names = realloc(names, sizeof(char*) * (count + 1));
            names[count] = strdup(dir->d_name);
            count++;
        }
        closedir(d);
    }
#endif
    
    *out_names = names;
    *out_count = count;
    return 0;
}
