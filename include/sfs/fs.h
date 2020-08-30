// fs.h: File System

#pragma once

#include "sfs/disk.h"

#include <stdint.h>

class FileSystem {
public:
    const static uint32_t MAGIC_NUMBER	     = 0xf0f03410;
    const static uint32_t INODES_PER_BLOCK   = 128;
    const static uint32_t POINTERS_PER_INODE = 5;
    const static uint32_t POINTERS_PER_BLOCK = 1024;

private:
    struct SuperBlock {		// Superblock structure
    	uint32_t MagicNumber;	// File system magic number
    	uint32_t Blocks;	// Number of blocks in file system
    	uint32_t InodeBlocks;	// Number of blocks reserved for inodes
    	uint32_t Inodes;	// Number of inodes in file system
    };

    struct Inode {
    	uint32_t Valid;		// Whether or not inode is valid
    	uint32_t Size;		// Size of file
    	uint32_t Direct[POINTERS_PER_INODE]; // Direct pointers
    	uint32_t Indirect;	// Indirect pointer
    };

    union Block {
    	SuperBlock  Super;			    // Superblock
    	Inode	    Inodes[INODES_PER_BLOCK];	    // Inode block
    	uint32_t    Pointers[POINTERS_PER_BLOCK];   // Pointer block
    	char	    Data[Disk::BLOCK_SIZE];	    // Data block
    };

    #define FILE_T  0xaf
    #define DIR_T   0x5b 
    #define DIRENT_NAME_SIZE 32
    
    /* it's pointer to a inode which determines the type of the inode */
    struct Dirent {
        uint32_t Inode; /* inode number  */
        uint32_t Type;  /* type of inode */
        uint32_t Resv;  /* size of inode */
        char Name[DIRENT_NAME_SIZE];  /* name of the item */
    };

    bool    load_inode          (size_t inumber, Inode *node);
    bool    save_inode          (size_t inumber, Inode *node);

    /* read nth data block of a inode */
    bool    read_nth_block      (size_t inumber, size_t nthblock, Block *block);
    ssize_t allocate_free_block ();
    bool    save_nth_block      (size_t inumber, size_t nthblock, Block *block);

    // set b block in free block map to occupied
    inline void set_fbmap  (uint32_t b) {free_bmap[b-offset] = 1;}
    inline void unset_fbmap(uint32_t b) {free_bmap[b-offset] = 0;}

    bool add_new_dirent(Dirent d);

    Dirent current_dir;

    // offset is the number of the non data blocks
    uint32_t        offset = 0;

    // free block map size
    uint32_t        freebmap_size;

    // free block map
    unsigned char   *free_bmap;

    // itable size;
    uint32_t        itable_size;

    // inode table
    unsigned char   *itable; 

    // disk pointer;
    Disk *disk;

public:
    static void debug   (Disk *disk);
    static bool format  (Disk *disk);

    bool        mount   (Disk *disk);

    ssize_t     mkfile  (const char *name);
    bool        remove  (size_t inumber);
    ssize_t     stat    (size_t inumber);

    ssize_t     read    (size_t inumber, 
                         char *data, 
                         size_t length, 
                         size_t offset);

    ssize_t     write   (size_t inumber, char *data, size_t length, size_t offset);
};
