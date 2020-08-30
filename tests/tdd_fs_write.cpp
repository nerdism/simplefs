
#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include "sfs/disk.h"
#include "sfs/fs.h"

#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#include <sys/types.h>
#include <cstdint>
#include <cstring>


/* struct SuperBlock {		// Superblock structure */
/*     uint32_t MagicNumber;	// File system magic number */
/*     uint32_t Blocks;	// Number of blocks in file system */
/*     uint32_t InodeBlocks;	// Number of blocks reserved for inodes */
/*     uint32_t Inodes;	// Number of inodes in file system */
/* }; */

/* struct Inode { */
/*     uint32_t Valid;		// Whether or not inode is valid */
/*     uint32_t Size;		// Size of file */
/*     uint32_t Direct[FileSystem::POINTERS_PER_INODE]; // Direct pointers */
/*     uint32_t Indirect;	// Indirect pointer */
/* }; */

/* union Block { */
/*     SuperBlock  Super;			    // Superblock */
/*     Inode	Inodes[FileSystem::INODES_PER_BLOCK];	    // Inode block */
/*     uint32_t    Pointers[FileSystem::POINTERS_PER_BLOCK];   // Pointer block */
/*     char	Data[Disk::BLOCK_SIZE];	    // Data block */
/* }; */


const char *IMAGE_FILE = "image.test";

/* number of the block of the test file*/
const uint32_t NUM_BLOCK = 200; 

TEST_CASE("fs write test", "write") {
    Disk disk;
    FileSystem fs;

    FILE *f = fopen(IMAGE_FILE, "w+");
    fclose(f);

    truncate(IMAGE_FILE, Disk::BLOCK_SIZE*NUM_BLOCK);
    disk.open(IMAGE_FILE, NUM_BLOCK);

    fs.format(&disk);


    if (!fs.mount(&disk)) {
        fprintf(stderr, "mount failed\n"); 
        exit(1);
    }

    int inode = fs.create();

    SECTION("inode does not exist test") {
    
        char buf[] = "z";
        uint32_t inode_non_exist = 45;
        REQUIRE(fs.write(inode_non_exist, buf, 1, 0) == -1); 
    
    }
    SECTION("test one letter") {
    
        char buf[] = "b";

        REQUIRE(fs.write(inode, buf, strlen(buf), 0) == 1);

        char t[100] = {0};
        fs.read(inode, t, 100, 0);
        
        REQUIRE(!strcmp(t, "b"));
    }
    SECTION("write a string less than a block") {
        char buf[] = "hi how are you"; 
        uint32_t len = strlen(buf);
        REQUIRE(fs.write(inode, buf, len, 0) == len);

        char t[100] = {0};
        fs.read(inode, t, len, 0);
        
        REQUIRE(!strcmp(t, buf));
    }
    SECTION("write a string less than a block 2") {
        uint32_t size = 4095;
        char buf[size]; 
        memset(buf, ',', size);
        uint32_t len = size;
        REQUIRE(fs.write(inode, buf, len, 0) == len);

        char t[size] = {0};
        int c = fs.read(inode, t, len, 0);
        
        REQUIRE(c == size);
        REQUIRE(!strncmp(t, buf, size));
    }
    SECTION("write a string exactly a block") {
        uint32_t size = 4096;
        char buf[size]; 
        memset(buf, 'u', size);

        REQUIRE(fs.write(inode, buf, size, 0) == size);

        char t[size] = {0};
        int c = fs.read(inode, t, size, 0);
        REQUIRE(c == size);
        REQUIRE(!strncmp(t, buf, size));
    }
    SECTION("write less than block from different offset") {
        uint32_t size = 1000;
        char buf[size]; 
        memset(buf, 'q', size);

        REQUIRE(fs.write(inode, buf, size, 150) == size);

        char t[size] = {0};
        int c = fs.read(inode, t, size, 150);
        REQUIRE(c == size);
        REQUIRE(!strncmp(t, buf, size));
    }
    // fs.remove(inode);
    unlink(IMAGE_FILE);
}


