#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void usage(const char *prog) {
    fprintf(stderr, "Usage: %s <mode> <path>\n", prog);
    fprintf(stderr, " Numeric: 755, 0755, 1777, 2755, 0664\n");
    fprintf(stderr, " Symbolic: +x, u-r, g+rw, ug+rw, a+rwx, u=rw, u+r,g-w\n");
}

int is_octal_string(const char *s) {
    if (*s == '\0') return 0;
    for (const char *p = s; *p; ++p) {
        if (*p < '0' || *p > '7') return 0;
    }
    return 1;
}

mode_t perms_for_class_mask(mode_t class_mask, mode_t rwx_mask) {
    mode_t out = 0;
    if (class_mask & S_IRWXU) {
        if (rwx_mask & 04) out |= S_IRUSR;
        if (rwx_mask & 02) out |= S_IWUSR;
        if (rwx_mask & 01) out |= S_IXUSR;
    }
    if (class_mask & S_IRWXG) {
        if (rwx_mask & 04) out |= S_IRGRP;
        if (rwx_mask & 02) out |= S_IWGRP;
        if (rwx_mask & 01) out |= S_IXGRP;
    }
    if (class_mask & S_IRWXO) {
        if (rwx_mask & 04) out |= S_IROTH;
        if (rwx_mask & 02) out |= S_IWOTH;
        if (rwx_mask & 01) out |= S_IXOTH;
    }
    return out;
}

int apply_symbolic(mode_t curr, const char *spec, mode_t *out_mode) {
    const char *p = spec;
    mode_t mode = curr;

    while (*p) {
        mode_t class_mask = 0;
        int seen_class = 0;
        while (*p == 'u' || *p == 'g' || *p == 'o' || *p == 'a') {
            seen_class = 1;
            if (*p == 'u') class_mask |= S_IRWXU;
            else if (*p == 'g') class_mask |= S_IRWXG;
            else if (*p == 'o') class_mask |= S_IRWXO;
            else if (*p == 'a') class_mask |= (S_IRWXU|S_IRWXG|S_IRWXO);
            ++p;
        }
        if (!seen_class) {
            class_mask = S_IRWXU|S_IRWXG|S_IRWXO;
        }

        char op = *p;
        if (op != '+' && op != '-' && op != '=') {
            fprintf(stderr, "Invalid mode: expected '+', '-', or '=' near: \"%s\"\n", p);
            return -1;
        }
        ++p;

        int have_perm = 0;
        int rwx_bits = 0; // 04=r, 02=w, 01=x
        while (*p == 'r' || *p == 'w' || *p == 'x') {
            have_perm = 1;
            if (*p == 'r') rwx_bits |= 04;
            else if (*p == 'w') rwx_bits |= 02;
            else if (*p == 'x') rwx_bits |= 01;
            ++p;
        }
        if (!have_perm && op != '=') {
            fprintf(stderr, "Invalid mode: expected permissions after '%c'\n", op);
            return -1;
        }

        mode_t mask = perms_for_class_mask(class_mask, rwx_bits);

        if (op == '+') {
            mode |= mask;
        } else if (op == '-') {
            mode &= ~mask;
        } else if (op == '=') {
            mode_t clear_mask = 0;
            if (class_mask & S_IRWXU) clear_mask |= S_IRWXU;
            if (class_mask & S_IRWXG) clear_mask |= S_IRWXG;
            if (class_mask & S_IRWXO) clear_mask |= S_IRWXO;
            mode &= ~clear_mask;
            mode |= mask;
        }

        if (*p == ',') {
            ++p; 
        } else if (*p == '\0') {
            break;
        } else {
            fprintf(stderr, "Invalid mode near: \"%s\"\n", p);
            return -1;
        }
    }

    *out_mode = mode;
    return 0;
}
int apply_numeric(mode_t curr_type_and_other_bits, const char *s, mode_t *out_mode) {
    size_t len = strlen(s);
    if (len < 3 || len > 4 || !is_octal_string(s)) {
        fprintf(stderr, "Invalid numeric mode: %s\n", s);
        return -1;
    }
    long val = strtol(s, NULL, 8);
    if (val < 0 || val > 07777) {
        fprintf(stderr, "Numeric mode out of range: %s\n", s);
        return -1;
    }
    mode_t perm = (mode_t)val;

    mode_t preserved = curr_type_and_other_bits & ~07777;
    *out_mode = preserved | (perm & 07777);
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        usage(argv[0]);
        return 1;
    }
    const char *mode_str = argv[1];
    const char *path = argv[2];

    struct stat st;
    if (lstat(path, &st) != 0) {
        perror("lstat");
        return 1;
    }

    mode_t new_mode = 0;
    int is_numeric = is_octal_string(mode_str) && (strlen(mode_str) == 3 || strlen(mode_str) == 4);

    if (is_numeric) {
        if (apply_numeric(st.st_mode, mode_str, &new_mode) != 0) {
            return 1;
        }
    } else {
        mode_t start = st.st_mode;
        if (apply_symbolic(start, mode_str, &new_mode) != 0) {
            return 1;
        }

        new_mode = (st.st_mode & ~07777) | (new_mode & 07777);
    }

    if (chmod(path, new_mode) != 0) {
        perror("chmod");
        return 1;
    }

    return 0;
}