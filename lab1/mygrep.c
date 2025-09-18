#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

static int is_substr(const char *line, const char *pattern) {
    return strstr(line, pattern) != NULL;
}

static int grep_stream(FILE *fp, const char *name, const char *pattern) {
    char *line = NULL;
    size_t cap = 0;
    ssize_t len;
    int rc = 0;

    while ((len = getline(&line, &cap, fp)) != -1) {
        if (is_substr(line, pattern)) {
            fwrite(line, 1, (size_t)len, stdout);
        }
    }
    if (ferror(fp)) {
        fprintf(stderr, "mygrep: ошибка чтения '%s': %s\n", name, strerror(errno));
        rc = 1;
    }
    free(line);
    return rc;
}

int main(int argc, char **argv) {
    if (argc < 2) { 
        fprintf(stderr, "Использование: %s PATTERN [FILE...]\n", argv[0]); 
        return 1; 
    }
    const char *pattern = argv[1];
    int rc = 0;

    if (argc == 2) {
        // читаем из stdin
        rc |= grep_stream(stdin, "stdin", pattern);
    } else {
        for (int i = 2; i < argc; ++i) {
            const char *name = argv[i];
            if (strcmp(name, "-") == 0) {
                rc |= grep_stream(stdin, "stdin", pattern);
                continue;
            }
            FILE *fp = fopen(name, "r");
            if (!fp) {
                fprintf(stderr, "mygrep: не могу открыть '%s': %s\n", name, strerror(errno));
                rc = 1;
                continue;
            }
            rc |= grep_stream(fp, name, pattern);
            fclose(fp);
        }
    }
    return rc;
}