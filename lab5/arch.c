#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <utime.h>
#include <fcntl.h>
#include <utime.h>
#include <stdlib.h>
#include <string.h>

struct file_header{
    char name[256]; //название
    mode_t mode; //права доступа
    uid_t uid; //владелец
    gid_t gid; //группа
    off_t size; //размер
    time_t mtime; //время последнего изменения файла
    int deleted; //флаг: удален или нет
};

void help_me(){
    printf("Usage:\n");
    printf("archiver arch_name -i file    Add file to archive\n");
    printf("archiver arch_name -e file    Extract file from archive\n");
    printf("archiver arch_name -s         Show archive contents\n");
    printf("archiver -h                   Show this help\n");
}

int add_to_archive(const char *arch_name, const char *file_name) {
    int arch_fd = open(arch_name, O_RDWR | O_CREAT, 0644);
    if(arch_fd < 0){
        perror("Error opening archive.\n");
        return 1;
    }
    int file_fd = open(file_name, O_RDONLY);
    if (file_fd < 0){
        perror("Error opening file");
        close(arch_fd);
        return 1;
    }
    struct stat st;
    if(fstat(file_fd, &st) < 0) {
        perror("Error fstat");
        close(file_fd);
        close(arch_fd);
        return 1;
    }

    struct file_header header;
    memset(&header, 0, sizeof(header));
    strncpy(header.name, file_name, 255);
    header.mode = st.st_mode;
    header.uid = st.st_uid;
    header.gid = st.st_gid;
    header.mtime = st.st_mtime;
    header.size = st.st_size;

    lseek(arch_fd, 0, SEEK_END);
    if (write(arch_fd, &header, sizeof(header)) != sizeof(header)){
        perror("Error writing header");
        close(file_fd);
        close(arch_fd);
        return 1;
    }
    char buf[4096];
    ssize_t bytes;
    while((bytes = read(file_fd, buf, 4096)) > 0) {
        if(write(arch_fd, buf, bytes) != bytes){
            perror("Error writing file data");
            close(file_fd);
            close(arch_fd);
            return 1;
        }
    }

    close(file_fd);
    close(arch_fd);
    printf("File '%s' added to archive '%s'.\n", file_name, arch_name);
    return 0;

}
int extract_from_archive(const char *arch_name, char *file_name){
    int arch_fd = open(arch_name, O_RDONLY);
    if(arch_name < 0){
        perror("Error opening archive");
        return 1;
    }

    char temp_name[] = "temp_archiveXXXXXX";
    int temp_fd = mkstemp(temp_name);
    if(temp_fd < 0) {
        perror("Error creating temporary archive");
        close(arch_fd);
        return 1;
    }

    struct file_header header;
    char buf[4096];
    int found = 0;

    while(read(arch_fd, &header, sizeof(header)) == sizeof(header)){
        if(strcmp(header.name, file_name) == 0){
            found = 1;
            int out_fd = open(file_name, O_WRONLY | O_CREAT | O_TRUNC, header.mode);
            if(out_fd < 0){
                perror("Error creating output file");
                close(arch_fd);
                close(temp_fd);
                return 1;
            }

            off_t remaining = header.size;
            while(remaining > 0){
                ssize_t chunk = remaining > 4096 ? 4096 : remaining;
                ssize_t bytes = read(arch_fd, buf, chunk);
                if(bytes <= 0) break;
                write(out_fd, buf, bytes);
                remaining -= bytes;
            }

            struct utimbuf times;
            times.actime = header.mtime;
            times.modtime = header.mtime;
            fchown(out_fd, header.uid, header.gid);
            fchmod(out_fd, header.mode);
            utime(file_name, &times);

            close(out_fd);
        } else {
            //скопировать в новый архиыв
            write(temp_fd, &header, sizeof(header));
            off_t remaining = header.size;
            while(remaining > 0){
                ssize_t chunk = remaining > 4096 ? 4096 : remaining;
                ssize_t bytes = read(arch_fd, buf, chunk);
                if(bytes <= 0) break;
                write(temp_fd, buf, bytes);
                remaining -= bytes;
            }
        }
        }
        close(arch_fd);
        close(temp_fd);
        if(!found){
            fprintf(stderr, "File '%s' not found in archive .\n", file_name);
            unlink(temp_name);
            return 1;
        }
        if(rename(temp_name, arch_name) < 0){
            perror("Error replacing archive");
            return 1;
        }
        printf("file '%s' successfully extracted from '%s'.\n", file_name, arch_name);
        return 0;
    }

int show_archive(const char *arch_name) {
    int arch_fd = open(arch_name, O_RDONLY);
    if (arch_fd < 0) {
        perror("Error opening archive");
        return 1;
    }

    struct file_header header;
    while (read(arch_fd, &header, sizeof(header)) == sizeof(header)) {
        printf("%-20s %8ld bytes  mode:%o  mtime:%ld\n",
               header.name, (long)header.size, header.mode, (long)header.mtime);
        lseek(arch_fd, header.size, SEEK_CUR);
    }

    close(arch_fd);
    return 0;
}


int main(int argc, char** argv){
    if(argc < 2){
        help_me();
        return 1;
    }

    if(strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0){
        help_me();
        return 0;
    }
    if(argc < 3){
        fprintf(stderr, "At least 3 arguments is required.\n");
        help_me();
        return 1;
    }
    const char *arch_name = argv[1];

    if(strcmp(argv[2], "-i") == 0 || strcmp(argv[2], "--input") == 0){
        if(argc < 4){
            fprintf(stderr, "Specify a file to add.\n");
            return 1;
        }
        return add_to_archive(arch_name, argv[3]);
    }

    else if(strcmp(argv[2], "-e") == 0 || strcmp(argv[2], "--extract") == 0){
        if(argc < 4){
            fprintf(stderr, "Specify a file to extract.");
            return 1;
        }
        return extract_from_archive(arch_name, argv[3]);
    }
    else if(strcmp(argv[2], "-s") == 0 || strcmp(argv[2], "--stat") == 0){
        return show_archive(arch_name);
    }

    else {
        help_me();
        return 1;
    }
}