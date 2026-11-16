#include <stdint.h>

#include <inttypes.h>

#include <stdio.h>

#include <dirent.h>

#include <string.h>

#include <sys/stat.h>

#include <sys/types.h>

#include <pwd.h>

#include <stdbool.h>
// use bool, true, false

#include <fcntl.h>
// use fcntl::{openat, AT_SYMLINK_NOFOLLOW, O_RDONLY, O_DIRECTORY}

#include <unistd.h>
// use unistd::close



typedef struct dirent dirent_t;
typedef        DIR    dir_t;
typedef struct stat   stat_t;
typedef struct passwd passwd_t;

// Seems like a non literal string as a format string is insecure
#define DASH "\x1b[1;90m-\x1b[39;21m"

#define KiB(n) ((n) * 1024)
#define MiB(n) (KiB(n) * 1024)
#define GiB(n) (MiB(n) * 1024)



int print_dir_entries(dir_t *dir, int depth, bool recursive, bool long_form, bool all_files) {
    int dir_fd = dirfd(dir);

    if (depth == 0) {
        printf("\x1b[4mPermissions\x1b[24m \x1b[4mSize\x1b[24m   \x1b[4mOwner\x1b[24m   \x1b[4mName\x1b[24m\n");
    }

    dirent_t *entry;
    while ((entry = readdir(dir))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // "Safe" indexing: a C-string will have at least a null terminator.
        if (!all_files && entry->d_name[0] == '.') {
            continue;
        }

        stat_t meta;
        // fstatat is a gnu std function
        if (fstatat(dir_fd, entry->d_name, &meta, AT_SYMLINK_NOFOLLOW) < 0) {
            perror("dir (stat)");
            closedir(dir);
            return 1;
        }

        passwd_t *owner = getpwuid(meta.st_uid);
        if (!owner) {
            perror("dir (getpwuid)");
            closedir(dir);
            return 1;
        }

        // File type
        printf(
              S_ISLNK(meta.st_mode) ? "\x1b[1;36ml\x1b[39;21m"
            : S_ISDIR(meta.st_mode) ? "\x1b[1;34md\x1b[39;21m"
            : S_ISCHR(meta.st_mode) ? "\x1b[1;33mc\x1b[39;21m"
            : "\x1b[1m-\x1b[21m"
        );

        // Owner permissions
        printf(meta.st_mode & S_IRUSR ? "\x1b[33mr\x1b[39m" : DASH);
        printf(meta.st_mode & S_IWUSR ? "\x1b[31mw\x1b[39m" : DASH);
        printf(meta.st_mode & S_IXUSR ? "\x1b[32mx\x1b[39m" : DASH);

        // Group permissions
        printf(meta.st_mode & S_IRGRP ? "\x1b[33mr\x1b[39m" : DASH);
        printf(meta.st_mode & S_IWGRP ? "\x1b[31mw\x1b[39m" : DASH);
        printf(meta.st_mode & S_IXGRP ? "\x1b[32mx\x1b[39m" : DASH);

        // Others' permissions
        printf(meta.st_mode & S_IROTH ? "\x1b[33mr\x1b[39m" : DASH);
        printf(meta.st_mode & S_IWOTH ? "\x1b[31mw\x1b[39m" : DASH);
        printf(meta.st_mode & S_IXOTH ? "\x1b[32mx\x1b[39m" : DASH);

        // File size
        if (S_ISREG(meta.st_mode)) {
            printf("  \x1b[1;32m");
            float size = (float) meta.st_size;
            if (size > GiB(1.0)) {
                printf("%5.2fG", size / GiB(1.0));
            } else if (size > MiB(1.0)) {
                printf("%5.2fM", size / MiB(1.0));
            } else if (size > KiB(1.0)) {
                printf("%5.2fk", size / KiB(1.0));
            } else {
                printf("%5.0fB", size);
            }
            printf("\x1b[39;21m ");
        } else {
            printf("         ");
        }

        // Owner name
        printf("\x1b[1;33m%s\x1b[39;21m ", owner->pw_name);

        if (depth > 0) {
            for (int i = 0; i < depth; i++) {
                printf("    ");
            }
        }
        printf("%s %s\n", S_ISDIR(meta.st_mode) ? "" : "󰡯", entry->d_name);

        // List inner entries
        if (recursive && S_ISDIR(meta.st_mode)) {
            int fd = openat(dir_fd, entry->d_name, O_RDONLY | O_DIRECTORY);
            if (fd < 0) {
                perror("dir (openat)");
                closedir(dir);
                return 1;
            }

            dir_t *subdir = fdopendir(fd);
            if (!subdir) {
                close(fd);
                perror("dir (fdopendir)");
                closedir(dir);
                return 1;
            }

            print_dir_entries(subdir, depth + 1, recursive, long_form, all_files);
        }
    }

    closedir(dir);
    return 0;
}

int main(int argc, char** argv) {
    int opt;
    bool long_form = false;
    bool all_files = false;
    bool recursive = false;
    while ((opt = getopt(argc, argv, "lar")) != -1) {
        switch (opt) {
            case 'l':
                long_form = true;
                break;
            case 'a':
                all_files = true;
                break;
            case 'r':
                recursive = true;
                break;
            default:
                fprintf(stderr, "How to use: lab-ls [-l] [-a] [-r]");
                return 1;
        }
    }
    char *path = ".";

    dir_t *dir = opendir(path);
    if (!dir) {
        perror("dir (opendir)");
        return 1;
    }
    return print_dir_entries(dir, 0, recursive, long_form, all_files);
}
