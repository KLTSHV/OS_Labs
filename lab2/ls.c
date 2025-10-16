#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <locale.h>
#include <limits.h>
#include <errno.h>

#if defined(__has_include)
#  if __has_include(<sys/xattr.h>)
#    define HAVE_SYS_XATTR 1
#    include <sys/xattr.h>
#  endif
#  if __has_include(<sys/acl.h>)
#    define HAVE_SYS_ACL 1
#    include <sys/acl.h>
#  endif
#  if __has_include(<sys/sysmacros.h>)
#    include <sys/sysmacros.h>
#  endif
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define COLOR_RESET "\033[0m"
#define COLOR_DIR   "\033[34m"
#define COLOR_EXEC  "\033[32m"
#define COLOR_LINK  "\033[36m"

typedef struct {
    char *name;
    struct stat st;
} Entry;

static int cmpfunc(const void *a, const void *b) {
    const char *sa = *(const char * const *)a;
    const char *sb = *(const char * const *)b;
    return strcoll(sa, sb); // сортировка по локали
}

static int numwidth(unsigned long long x) {
    int w = 1;
    while (x >= 10) { x /= 10; w++; }
    return w;
}

static void build_permissions(mode_t m, char out[11]) {
    char t = '-';
    if (S_ISDIR(m)) t='d';
    else if (S_ISLNK(m)) t='l';
    else if (S_ISCHR(m)) t='c';
    else if (S_ISBLK(m)) t='b';
    else if (S_ISFIFO(m)) t='p';
    else if (S_ISSOCK(m)) t='s';

    out[0]=t;
    out[1]=(m&S_IRUSR)?'r':'-';
    out[2]=(m&S_IWUSR)?'w':'-';
    out[3]=(m&S_IXUSR)?'x':'-';
    out[4]=(m&S_IRGRP)?'r':'-';
    out[5]=(m&S_IWGRP)?'w':'-';
    out[6]=(m&S_IXGRP)?'x':'-';
    out[7]=(m&S_IROTH)?'r':'-';
    out[8]=(m&S_IWOTH)?'w':'-';
    out[9]=(m&S_IXOTH)?'x':'-';
    out[10]='\0';

    // setuid/setgid/sticky
    if (m & S_ISUID) out[3] = (out[3]=='x') ? 's' : 'S';
    if (m & S_ISGID) out[6] = (out[6]=='x') ? 's' : 'S';
#ifdef S_ISVTX
    if (m & S_ISVTX) out[9] = (out[9]=='x') ? 't' : 'T';
#endif
}

static char attr_marker(const char *fullpath, mode_t mode) {
    int has_x = 0, has_acl_flag = 0;
    (void)mode;

#if defined(__APPLE__)
  #if defined(HAVE_SYS_XATTR)
    // На macOS listxattr(.., XATTR_NOFOLLOW)
    ssize_t n = listxattr(fullpath, NULL, 0, XATTR_NOFOLLOW);
    if (n > 0) has_x = 1;
  #endif
  #if defined(HAVE_SYS_ACL)
    acl_t acl = acl_get_link_np(fullpath, ACL_TYPE_EXTENDED);
    if (acl) { has_acl_flag = 1; acl_free(acl); }
  #endif

#elif defined(__linux__)
  #if defined(HAVE_SYS_XATTR)
    // На Linux используем llistxattr, чтобы не следовать по symlink
    ssize_t n = llistxattr(fullpath, NULL, 0);
    if (n > 0) has_x = 1;
  #endif
  #if defined(HAVE_SYS_ACL)
    // ACL_TYPE_ACCESS для обычных файлов/каталогов
    acl_t acl = acl_get_file(fullpath, ACL_TYPE_ACCESS);
    if (acl) {
        acl_entry_t e; int r = acl_get_entry(acl, ACL_FIRST_ENTRY, &e);
        if (r == 1) has_acl_flag = 1;
        acl_free(acl);
    }
  #endif

#else
    (void)fullpath;
#endif
    if (has_x) return '@';
    if (has_acl_flag) return '+';
    return ' ';
}

static void format_time(time_t mtime, char *buf, size_t bufsz) {
    time_t now = time(NULL);
    // старше 6 месяцев — печатаем год, иначе HH:MM
    const long six_months = 6L*30*24*60*60;
    struct tm tm;
    localtime_r(&mtime, &tm);
    if ((now - mtime) > six_months || (mtime - now) > 60) {
        strftime(buf, bufsz, "%b %e  %Y", &tm);
    } else {
        strftime(buf, bufsz, "%b %e %H:%M", &tm);
    }
}

static void color_print_name(const char *name, const struct stat *st) {
    if (S_ISDIR(st->st_mode)) {
        printf(COLOR_DIR "%s" COLOR_RESET, name);
    } else if (S_ISLNK(st->st_mode)) {
        printf(COLOR_LINK "%s" COLOR_RESET, name);
    } else if (st->st_mode & (S_IXUSR|S_IXGRP|S_IXOTH)) {
        printf(COLOR_EXEC "%s" COLOR_RESET, name);
    } else {
        printf("%s", name);
    }
}

int main(int argc, char *argv[]) {
    setlocale(LC_ALL, "");

    int opt, show_all = 0, long_format = 0;
    while ((opt = getopt(argc, argv, "la")) != -1) {
        switch (opt) {
            case 'l': long_format = 1; break;
            case 'a': show_all = 1; break;
            default:
                fprintf(stderr, "Usage: %s [-l] [-a] [dir]\n", argv[0]);
                return 1;
        }
    }

    const char *path = (optind < argc) ? argv[optind] : ".";
    DIR *dir = opendir(path);
    if (!dir) {
        perror("opendir");
        return 1;
    }

    // читаем имена
    size_t cap = 64, count = 0;
    char **names = malloc(cap * sizeof(char*));
    if (!names) { perror("malloc"); closedir(dir); return 1; }

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (!show_all && ent->d_name[0] == '.')
            continue;
        if (count == cap) {
            cap *= 2;
            char **tmp = realloc(names, cap * sizeof(char*));
            if (!tmp) { perror("realloc"); closedir(dir); return 1; }
            names = tmp;
        }
        names[count++] = strdup(ent->d_name);
    }
    closedir(dir);

    // сортировка по локали
    qsort(names, count, sizeof(char*), cmpfunc);

    // если не long, печатаем сразу
    if (!long_format) {
        for (size_t i = 0; i < count; i++) {
            char full[PATH_MAX];
            snprintf(full, sizeof(full), "%s/%s", path, names[i]);
            struct stat st;
            if (lstat(full, &st) == -1) { perror("lstat"); free(names[i]); continue; }
            color_print_name(names[i], &st);
            printf("  ");
            free(names[i]);
        }
        free(names);
        printf("\n");
        return 0;
    }

    // long format: сначала соберём st и ширины колонок
    Entry *es = calloc(count, sizeof(Entry));
    if (!es) { perror("calloc"); for (size_t i=0;i<count;i++) free(names[i]); free(names); return 1; }

    long long total_blocks = 0;
    int w_links = 1, w_user = 1, w_group = 1, w_size = 1;

    for (size_t i = 0; i < count; i++) {
        es[i].name = names[i];
        char full[PATH_MAX];
        snprintf(full, sizeof(full), "%s/%s", path, names[i]);
        if (lstat(full, &es[i].st) == -1) {
            perror("lstat");
            memset(&es[i].st, 0, sizeof(struct stat));
            continue;
        }
        total_blocks += es[i].st.st_blocks;

        // ширины
        int nlw = numwidth((unsigned long long)es[i].st.st_nlink);
        if (nlw > w_links) w_links = nlw;

        struct passwd *pw = getpwuid(es[i].st.st_uid);
        struct group  *gr = getgrgid(es[i].st.st_gid);
        int ulen = pw ? (int)strlen(pw->pw_name) : numwidth((unsigned long long)es[i].st.st_uid);
        int glen = gr ? (int)strlen(gr->gr_name) : numwidth((unsigned long long)es[i].st.st_gid);
        if (ulen > w_user) w_user = ulen;
        if (glen > w_group) w_group = glen;

        // размер (без maj/min для устройств)
        int szw = numwidth((unsigned long long)es[i].st.st_size);
        if (szw > w_size) w_size = szw;
    }
    free(names);

    // Печатаем "total" в блоках пользователя:
    long long display_total = total_blocks;
    const char *bs = getenv("BLOCKSIZE");
    if (bs && *bs) {
        long long blocksize = atoll(bs);
        if (blocksize <= 0) blocksize = 512;
        display_total = (total_blocks * 512 + blocksize - 1) / blocksize;
    } else {
        display_total = total_blocks / 2;
    }
    printf("total %lld\n", display_total);

    // Печать
    for (size_t i = 0; i < count; i++) {
        char perms[11];
        build_permissions(es[i].st.st_mode, perms);

        char full[PATH_MAX];
        snprintf(full, sizeof(full), "%s/%s", path, es[i].name);
        char marker = attr_marker(full, es[i].st.st_mode);

        printf("%s%c ", perms, marker);

        // ссылки
        printf("%*lu ", w_links, (unsigned long)es[i].st.st_nlink);

        // владелец / группа
        struct passwd *pw = getpwuid(es[i].st.st_uid);
        struct group  *gr = getgrgid(es[i].st.st_gid);
        if (pw) printf("%-*s ", w_user, pw->pw_name);
        else    printf("%-*lu ", w_user, (unsigned long)es[i].st.st_uid);
        if (gr) printf("%-*s ", w_group, gr->gr_name);
        else    printf("%-*lu ", w_group, (unsigned long)es[i].st.st_gid);

        // размер
        printf("%*lld ", w_size, (long long)es[i].st.st_size);

        // время
        char tbuf[64];
        format_time(es[i].st.st_mtime, tbuf, sizeof tbuf);
        printf("%s ", tbuf);

        // имя (+ стрелка для symlink)
        color_print_name(es[i].name, &es[i].st);
        if (S_ISLNK(es[i].st.st_mode)) {
            char linkto[PATH_MAX];
            ssize_t n = readlink(full, linkto, sizeof(linkto)-1);
            if (n >= 0) {
                linkto[n] = '\0';
                printf(" -> %s", linkto);
            }
        }
        printf("\n");
        free(es[i].name);
    }
    free(es);

    return 0;
}