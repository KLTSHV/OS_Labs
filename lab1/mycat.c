#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

static int is_blank_line(const char*s, ssize_t len){
    if (len == 0) return 1;
    if (len == 1 && s[0] == '\n') return 1;
    return 0;
}

static int cat_stream(FILE *fp, const char *name, int opt_n, int opt_b, int opt_E) {
    char *line = NULL;
    size_t cap = 0;
    ssize_t len;
    long long ln_all = 0;
    long long ln_nonblank = 0;

    while ((len = getline(&line, &cap, fp)) != -1) {
        int blank = is_blank_line(line, len);
        int do_number = 0;
        long long num = 0;
        if (opt_b) {
            if (!blank) { do_number = 1; num = ++ln_nonblank; }
        } else if (opt_n) {
            do_number = 1; num = ++ln_all;
        }
        if (do_number)
            printf("%6lld\t", num);
        if (opt_E) {
            /* напечатаем строку, подставив '$' перед '\n' либо в конце, если \n нет */
            if (len > 0 && line[len-1] == '\n') {
                fwrite(line, 1, len-1, stdout);
                putchar('$');
                putchar('\n');
            } else {
                fwrite(line, 1, len, stdout);
                putchar('$');
            }
        } else {
            fwrite(line, 1, len, stdout);
        }
    }
    if (ferror(fp)) {
        fprintf(stderr, "mycat: ошибка чтения '%s': %s\n", name, strerror(errno));
        free(line);
        return 1;
    }
    free(line);
    return 0;
}
static void usage(const char *prog) {
    fprintf(stderr, "Использование: %s [-n] [-b] [-E] [FILE...]\n", prog);
    fprintf(stderr, "  -n  нумеровать все строки\n");
    fprintf(stderr, "  -b  нумеровать только непустые (перекрывает -n)\n");
    fprintf(stderr, "  -E  показывать $ в конце каждой строки\n");
}

int main (int argc, char** argv){
    int opt;
    int opt_n = 0;
    int opt_b = 0;
    int opt_E = 0;
    while((opt = getopt(argc, argv, "nbE")) != -1 ){
        switch(opt){
            case 'n': opt_n = 1; break;
            case 'b': opt_b = 1; break;
            case 'E': opt_E = 1; break;
        }
    }
    int rc = 0;
    if (optind == argc) {
        rc |= cat_stream(stdin, "stdin", opt_n, opt_b, opt_E);
    } else {
        for(int i = optind; i < argc; ++i){
            const char *name = argv[i];
            FILE *fp;
            if (strcmp(name, "-") == 0){
                fp = stdin;
                rc |= cat_stream(fp, "stdin", opt_n, opt_b, opt_E);
            } else {
                fp = fopen(name, "rb");
                if (!fp){
                    fprintf(stderr, "mycat: не могу открыть '%s': %s\n", name, strerror(errno));
                    rc = 1;
                    continue;
                }
                rc |= cat_stream(fp, name, opt_n, opt_b, opt_E);
            }
        }
    }

}