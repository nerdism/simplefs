// sfssh.cpp: Simple file system shell

#include "sfs/disk.h"
#include "sfs/fs.h"

#include <sstream>
#include <string>
#include <stdexcept>

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Macros

#define streq(a, b) (strcmp((a), (b)) == 0)

// Command prototypes
//
void list_dir(char * dirname, FileSystem& fs);

void do_debug   (Disk &disk, FileSystem &fs, int args, char *arg1, char *arg2);
void do_format  (Disk &disk, FileSystem &fs, int args, char *arg1, char *arg2);
void do_mount   (Disk &disk, FileSystem &fs, int args, char *arg1, char *arg2);
void do_cat     (Disk &disk, FileSystem &fs, int args, char *arg1, char *arg2);
void do_copyout (Disk &disk, FileSystem &fs, int args, char *arg1, char *arg2);
void do_list    (Disk &disk, FileSystem &fs, int args, char *arg1, char *arg2);
void do_mkfile  (Disk &disk, FileSystem &fs, int args, char *arg1, char *arg2);
void do_mkdir   (Disk &disk, FileSystem &fs, int args, char *arg1, char *arg2);
void do_pwd     (Disk &disk, FileSystem &fs, int args, char *arg1, char *arg2);
void do_cd     (Disk &disk, FileSystem &fs, int args, char *arg1, char *arg2);
void do_remove  (Disk &disk, FileSystem &fs, int args, char *arg1, char *arg2);
void do_stat    (Disk &disk, FileSystem &fs, int args, char *arg1, char *arg2);
void do_copyin  (Disk &disk, FileSystem &fs, int args, char *arg1, char *arg2);
void do_help    (Disk &disk, FileSystem &fs, int args, char *arg1, char *arg2);

bool copyout(FileSystem &fs, size_t inumber, const char *path);
bool copyin(FileSystem &fs, const char *path, size_t inumber);

// Main execution

int main(int argc, char *argv[]) {
    Disk	disk;
    FileSystem	fs;

    if (argc != 3 && argc != 4) {
    	fprintf(stderr, "Usage: %s <diskfile> <nblocks>\n", argv[0]);
    	fprintf(stderr, "Usage: %s <diskfile> <nblocks> <folder_to_convert>\n", argv[0]);
    	return EXIT_FAILURE;
    }

    try {
    	disk.open(argv[1], atoi(argv[2]));
    } catch (std::runtime_error &e) {
    	fprintf(stderr, "Unable to open disk %s: %s\n", argv[1], e.what());
    	return EXIT_FAILURE;
    }

    if (argv[3]) {
        printf("argv[3]: %s\n", argv[3]);
        fs.format(&disk);
        fs.mount(&disk);
        list_dir(argv[3], fs);
        printf("list_dir executed\n");
    }
    else {
    
        while (true) {
            char line[BUFSIZ], cmd[BUFSIZ], arg1[BUFSIZ], arg2[BUFSIZ];

            fprintf(stderr, "sfs> ");
            fflush(stderr);

            if (fgets(line, BUFSIZ, stdin) == NULL) {
                break;
            }

            int args = sscanf(line, "%s %s %s", cmd, arg1, arg2);
            if (args == 0) {
                continue;
            }

            if (streq(cmd, "debug")) {
                do_debug(disk, fs, args, arg1, arg2);
            } else if (streq(cmd, "format")) {
                do_format(disk, fs, args, arg1, arg2);
            } else if (streq(cmd, "mount")) {
                do_mount(disk, fs, args, arg1, arg2);
            } else if (streq(cmd, "cat")) {
                do_cat(disk, fs, args, arg1, arg2);
            } else if (streq(cmd, "copyout")) {
                do_copyout(disk, fs, args, arg1, arg2);
            } else if (streq(cmd, "mkfile")) {
                do_mkfile(disk, fs, args, arg1, arg2);
            } else if (streq(cmd, "mkdir")) {
                do_mkdir(disk, fs, args, arg1, arg2); 
            } else if (streq(cmd, "pwd")) {
                do_pwd(disk, fs, args, arg1, arg2); 
            } else if (streq(cmd, "cd")) {
                do_cd(disk, fs, args, arg1, arg2); 
            } else if (streq(cmd, "ls")) {
                do_list(disk, fs, args, arg1, arg2); 
            } else if (streq(cmd, "remove")) {
                do_remove(disk, fs, args, arg1, arg2);
            } else if (streq(cmd, "stat")) {
                do_stat(disk, fs, args, arg1, arg2);
            } else if (streq(cmd, "copyin")) {
                do_copyin(disk, fs, args, arg1, arg2);
            } else if (streq(cmd, "help")) {
                do_help(disk, fs, args, arg1, arg2);
            } else if (streq(cmd, "exit") || streq(cmd, "quit")) {
                break;
            } else {
                printf("Unknown command: %s", line);
                printf("Type 'help' for a list of commands.\n");
            }
        }
    }

    return EXIT_SUCCESS;
}

// Command functions

void do_debug(Disk &disk, FileSystem &fs, int args, char *arg1, char *arg2) {
    if (args != 1) {
    	printf("Usage: debug\n");
    	return;
    }

    fs.debug(&disk);
}

void do_format(Disk &disk, FileSystem &fs, int args, char *arg1, char *arg2) {
    if (args != 1) {
    	printf("Usage: format\n");
    	return;
    }

    if (fs.format(&disk)) {
    	printf("disk formatted.\n");
    } else {
    	printf("format failed!\n");
    }
}

void do_mount(Disk &disk, FileSystem &fs, int args, char *arg1, char *arg2) {
    if (args != 1) {
    	printf("Usage: mount\n");
    	return;
    }

    if (fs.mount(&disk)) {
    	printf("disk mounted.\n");
    } else {
    	printf("mount failed!\n");
    }
}

void do_cat(Disk &disk, FileSystem &fs, int args, char *arg1, char *arg2) {
    if (args != 2) {
    	printf("Usage: cat <inode>\n");
    	return;
    }

    if (!copyout(fs, atoi(arg1), "/dev/stdout")) {
    	printf("cat failed!\n");
    }
}

void do_mkfile(Disk &disk, FileSystem &fs, int args, char *arg1, char *arg2) {
    if (args != 2) {
    	printf("Usage: mkfile file_name\n");
    	return;
    }

    if (fs.mkfile(arg1) >= 0) {
        printf("created the file successfully\n");
    } else {
        printf("failed to create file\n");
    }
}
void do_mkdir(Disk &disk, FileSystem &fs, int args, char *arg1, char *arg2) {
    if (args != 2) {
    	printf("Usage: mkdir dir_name\n");
    	return;
    }

    if (fs.mkdir(arg1) >= 0) {
        printf("created the directory successfully\n");
    } else {
        printf("failed to create directory\n");
    }
}

void do_pwd(Disk &disk, FileSystem &fs, int args, char *arg1, char *arg2) {
    if (args != 1) {
    	printf("Usage: pwd\n");
    	return;
    }

    printf("%s\n", fs.get_current_dir());

}

void do_list(Disk &disk, FileSystem &fs, int args, char *arg1, char *arg2) {
    if (args != 1) {
    	printf("usage: ls\n");
    	return;
    }

    fs.list();

}

void do_cd(Disk &disk, FileSystem &fs, int args, char *arg1, char *arg2) {
    if (args != 2) {
    	printf("usage: cd directory_name\n");
    	return;
    }

    if (!fs.change_directory(arg1)) {
        printf("unable to change directory\n"); 
    }

}

void do_remove(Disk &disk, FileSystem &fs, int args, char *arg1, char *arg2) {
    if (args != 2) {
    	printf("Usage: remove <inode>\n");
    	return;
    }

    ssize_t inumber = atoi(arg1);
    if (fs.remove(inumber)) {
    	printf("removed inode %ld.\n", inumber);
    } else {
    	printf("remove failed!\n");
    }
}

void do_stat(Disk &disk, FileSystem &fs, int args, char *arg1, char *arg2) {
    if (args != 2) {
    	printf("Usage: stat <inode>\n");
    	return;
    }

    ssize_t inumber = atoi(arg1);
    ssize_t bytes   = fs.stat(inumber);
    if (bytes >= 0) {
    	printf("inode %ld has size %ld bytes.\n", inumber, bytes);
    } else {
    	printf("stat failed!\n");
    }
}

void do_copyout(Disk &disk, FileSystem &fs, int args, char *arg1, char *arg2) {
    if (args != 3) {
    	printf("Usage: copyout <inode> <file>\n");
    	return;
    }

    /* if (!copyout(fs, atoi(arg1), arg2)) { */
    /* 	printf("copyout failed!\n"); */
    /* } */
}


void do_copyin(Disk &disk, FileSystem &fs, int args, char *arg1, char *arg2) {
    if (args != 3) {
        printf("    copyin  <src_file> <dst_simplefs_file>\n");
    	return;
    }

    /* if (!copyin(fs, arg1, arg2)) { */
    /* 	printf("copyin failed!\n"); */
    /* } */
}

void do_help(Disk &disk, FileSystem &fs, int args, char *arg1, char *arg2) {
    printf("Commands are:\n");
    printf("    format\n");
    printf("    mount\n");
    printf("    debug\n");
    printf("    create\n");
    printf("    remove  <inode>\n");
    printf("    mkfile <<F12>jjj>\n");
    printf("    ls\n");
    printf("    pwd\n");
    printf("    cd      <dir_name>\n");
    printf("    cat     <inode>\n");
    printf("    stat    <inode>\n");
    printf("    copyin  <src_file> <dst_simplefs_file>\n");
    printf("    copyout <inode> <file>\n");
    printf("    help\n");
    printf("    quit\n");
    printf("    exit\n");
}

bool copyout(FileSystem &fs, size_t inumber, const char *path) {
    FILE *stream = fopen(path, "w");
    if (stream == nullptr) {
    	fprintf(stderr, "Unable to open %s: %s\n", path, strerror(errno));
    	return false;
    }

    char buffer[4*BUFSIZ] = {0};

    ssize_t result = fs.read(inumber, buffer);

    printf("%lu bytes copied\n", offset);
    fclose(stream);
    return true;
}

bool copyin(FileSystem &fs, const char *path, size_t inumber) {
    FILE *stream = fopen(path, "r");
    if (stream == nullptr) {
    	fprintf(stderr, "Unable to open %s: %s\n", path, strerror(errno));
    	return false;
    }

    char buffer[1024*1024*10] = {0};

    ssize_t result = fread(buffer, 1, sizeof(buffer), stream);

    if (result <= 0) {
        return false;
    }

    ssize_t actual = fs.write(inumber, buffer, result);

    printf("%lu bytes copied\n", actual);
    fclose(stream);
    return true;
}

void create_simplefs_file(char *real_file, uint32_t inumber, FileSystem& fs) {
    FILE* file = fopen(real_file, "r");
    if (!file)
        printf("can not open file %s\n", real_file);

    size_t size = 1024 * 1024 * 5;
    char buffer[size];

    uint32_t read_bytes = fread(buffer, sizeof(char), size, file);
    
    uint32_t bytes = fs.write(inumber, buffer, read_bytes);


    if ( bytes != read_bytes)
    {
        printf("error in create_simplefs_file\n");
        printf("read: %d, write: %d\n", read_bytes, bytes);
    }

}

void list_dir(char * dirname, FileSystem& fs) {
    struct dirent *direntp;
    DIR *dirp;
    char *subdirname;
    if ((dirp = opendir(dirname)) == NULL) 
    {
        return;
    }
    while ((direntp = readdir(dirp)) != NULL)
    {
        if(strcmp(direntp->d_name, ".")==0 || strcmp(direntp->d_name, "..")==0)
              continue; // skip current and parent directories
        // printf("%s\n", direntp->d_name);


        // build child dir name and call listOfDir
        subdirname = (char*)malloc(strlen(direntp->d_name) + strlen(dirname) + 2);
        // make a directory in simplefs

        strcpy(subdirname, dirname);
        strcat(subdirname, "/");
        strcat(subdirname, direntp->d_name);


        if( direntp->d_type == DT_DIR) {
            fs.mkdir(direntp->d_name);
            fs.change_directory(direntp->d_name);
            list_dir(subdirname, fs);
            fs.change_directory("..");
        }
        else if (direntp->d_type == DT_REG) {
            // make a file in simplefs
            uint32_t inumber = fs.mkfile(direntp->d_name);
            create_simplefs_file(subdirname, inumber, fs);

        }

        free(subdirname);
    }
    closedir(dirp);
}
