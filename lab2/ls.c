#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>

#define COLOR_RESET "\033[0m"
#define COLOR_DIR "\033[34m"
#define COLOR_EXEC "\033[32m"
#define COLOR_LINK "\033[36m"

int cmpfunc(const void *a, const void *b) {
     char **sa = (const char **)a;
    char **sb = (const char **)b;
    return strcmp(*sa, *sb);
}

void print_permissions(mode_t mode) {
    char perms[11] = "----------";

    if (S_ISDIR(mode)) perms[0] = 'd';
    else if (S_ISLNK(mode)) perms[0] = 'l';

    if (mode & S_IRUSR) perms[1] = 'r';
    if (mode & S_IWUSR) perms[2] = 'w';
    if (mode & S_IXUSR) perms[3] = 'x';
    if (mode & S_IRGRP) perms[4] = 'r';
    if (mode & S_IWGRP) perms[5] = 'w';
    if (mode & S_IXGRP) perms[6] = 'x';
    if (mode & S_IROTH) perms[7] = 'r';
    if (mode & S_IWOTH) perms[8] = 'w';
    if (mode & S_IXOTH) perms[9] = 'x';

    printf("%s ", perms);
}

void print_file(const char *name, const char *path, int long_format) {
    struct stat st;
    char fullpath[4096];
    snprintf(fullpath, sizeof(fullpath), "%s/%s", path, name);

    if (lstat(fullpath, &st) == -1) {
        perror("stat");
        return;
    }

    if (long_format) {
        print_permissions(st.st_mode);

        printf("%hu ", st.st_nlink);

        struct passwd *pw = getpwuid(st.st_uid);
        struct group  *gr = getgrgid(st.st_gid);

        printf("%s %s ", pw ? pw->pw_name : "?", gr ? gr->gr_name : "?");
        printf("%5lld ", st.st_size);

        char timebuf[64];
        strftime(timebuf, sizeof(timebuf), "%b %d %H:%M", localtime(&st.st_mtime));
        printf("%s ", timebuf);
    }

    // Цвет
    if (S_ISDIR(st.st_mode)) {
        printf(COLOR_DIR "%s" COLOR_RESET, name);
    } else if (S_ISLNK(st.st_mode)) {
        printf(COLOR_LINK "%s" COLOR_RESET, name);
    } else if (st.st_mode & S_IXUSR) {
        printf(COLOR_EXEC "%s" COLOR_RESET, name);
    } else {
        printf("%s", name);
    }

    if (long_format) printf("\n");
    else printf("  ");
}

int main(int argc, char *argv[]) {
    int opt;
    int show_all = 0;
    int long_format = 0;

    while ((opt = getopt(argc, argv, "la")) != -1) {
        switch (opt) {
            case 'l': long_format = 1; break;
            case 'a': show_all = 1; break;
            default:
                fprintf(stderr, "Usage: %s [-l] [-a] [dir]\n", argv[0]);
                exit(1);
        }
    }

    const char *path = (optind < argc) ? argv[optind] : ".";

    DIR *dir = opendir(path);
    if (!dir) {
        perror("opendir");
        return 1;
    }

    struct dirent *entry;
    char **files = NULL;
    size_t count = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (!show_all && entry->d_name[0] == '.')
            continue;
        files = realloc(files, sizeof(char*) * (count + 1));
        files[count++] = strdup(entry->d_name);
    }

    closedir(dir);

    qsort(files, count, sizeof(char*), cmpfunc);

    for (size_t i = 0; i < count; i++) {
        print_file(files[i], path, long_format);
        free(files[i]);
    }
    free(files);

    if (!long_format) printf("\n");
    return 0;
}