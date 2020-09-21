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
    const static uint32_t INDIRECT_OFFSET    = 6;
    const static uint32_t DIRENT_NAME_SIZE   = 26;
    const static uint32_t INODE_BLOCKS_OFFSET = 1; // number of the block right after direct blocks
    const static uint32_t DIRENTS_PER_BLOCK  = 256; // Disk::BLOCK_SIZE/sizeof(Dirent)

private:
    struct SuperBlock {		// Superblock structure
    	uint32_t MagicNumber;	// File system magic number
    	uint32_t Blocks;	// Number of blocks in file system
    	uint32_t InodeBlocks;	// Number of blocks reserved for inodes
    	uint32_t Inodes; 	// Number of inodes in file system
    };

    struct Inode {
    	uint32_t Valid;		// Whether or not inode is valid
    	uint32_t Size;		// Size of file
    	uint32_t Direct[POINTERS_PER_INODE]; // Direct pointers
    	uint32_t Indirect; 	// Indirect pointer
    };

    /* it's pointer to a inode which determines the type of the inode */
    #pragma pack(1)
    struct Dirent {
        uint32_t Inode; /* inode number  */
        uint8_t  Type;  /* type of inode */
        uint8_t  NameLength;  /* size of inode */
        char     Name[DIRENT_NAME_SIZE];   /* name of the item */
    };
    #pragma pack()

    union Block {
    	SuperBlock  Super;			    // Superblock
    	Inode	    Inodes[INODES_PER_BLOCK];	    // Inode block
        Dirent      Dirents[DIRENTS_PER_BLOCK];
    	uint32_t    Pointers[POINTERS_PER_BLOCK];   // Pointer block
    	char	    Data[Disk::BLOCK_SIZE];	    // Data block
    };

    enum class DirentType {
        FILE_T = 0xaf,
        DIR_T  = 0xb1
    };

    bool    load_inode          (size_t inumber, Inode *node);
    bool    save_inode          (size_t inumber, Inode *node);

    /* read nth data block of a inode */
    bool    read_nth_block      (size_t inumber, size_t nthblock, Block *block);

    /**
     * @Brief find a free block from the free blocks bitmap
     *
     * @Return free block number from beginning
     */
    ssize_t allocate_free_block ();
    bool    save_nth_block      (size_t inumber, size_t nthblock, Block *block);

    // set b block in free block map to occupied
    inline void set_free_bitmap  (uint32_t b) {m_free_bitmap[b-m_offset] = 1;}
    inline void unset_free_bitmap(uint32_t b) {m_free_bitmap[b-m_offset] = 0;}

    /**
     * @Brief takes a dirent and addes it to current dirent
     *
     * @Param dirent a new dirent that we want to add to current
     * @Return true if successful false if failed 
     */
    bool add_new_dirent(const Dirent& dirent);

    /**
     * @Brief make a file or directory
     *
     * @Param name file or directory name
     * @Param type type if either FILE_T OR DIR_T
     * @Return true if successful
     */
    bool make_file_or_dir(const char *name, DirentType type);

    Dirent m_current_dir;

    // offset is the number of the non data blocks
    uint32_t        m_offset;

    // free block map size
    uint32_t        m_free_bitmap_size;

    // free block map
    unsigned char   *m_free_bitmap;

    // itable size;
    uint32_t        m_itable_size;

    // inode table
    unsigned char   *m_itable; 

    // disk pointer;
    Disk *disk;

public:
    static void debug   (Disk *disk);

    bool format  (Disk *disk);

    bool        mount   (Disk *disk);

    /**
     * @Brief create a file with the given name on the 
     *  current directory
     * 
     * @Param name of the file
     * @return true if successful false if fail
     */
    bool        mkfile  (const char *name);

    /**
     * @Brief create a directory with the given name on the 
     *  current directory
     * 
     * @Param  name of the directory
     * @return true if successful false if fail
     */
    bool        mkdir   (const char *name);
    char        *get_current_dir();
    bool        remove  (size_t inumber);
    ssize_t     stat    (size_t inumber);

    ssize_t     read    (size_t inumber, 
                         char *data, 
                         size_t length, 
                         size_t offset);

    ssize_t     write   (size_t inumber, char *data, size_t length, size_t offset);
};
