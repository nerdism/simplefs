// fs.cpp: File System

#include "sfs/fs.h"

#include <algorithm>

#include <assert.h>
#include <stdio.h>
#include <string.h>

// Debug file system -----------------------------------------------------------

void FileSystem::debug(Disk *disk) {
    Block block;

    // Read Superblock
    disk->read(0, block.Data);

    // inode block num
    uint32_t inode_b = block.Super.InodeBlocks;

    printf("SuperBlock:\n");
    if (block.Super.MagicNumber == FileSystem::MAGIC_NUMBER)
        printf("    magic number is %s\n", "valid");
    else
        printf("    magic number is %s\n", "invalid");

    printf("    %u blocks\n"         , block.Super.Blocks);
    printf("    %u inode blocks\n"   , inode_b);
    printf("    %u inodes\n"         , block.Super.Inodes);

    // Read Inode blocks
    for (uint32_t i = 1; i <= inode_b; i++) {
        disk->read(i, block.Data);
        for (uint32_t j = 0; j < FileSystem::INODES_PER_BLOCK; j++) {
            
            if (block.Inodes[j].Valid) {
                printf("Inode %d:\n", (i-1)*FileSystem::INODES_PER_BLOCK + j);
                printf("    size: %d bytes\n", block.Inodes[j].Size);
                printf("    direct blocks:");
                for (int k = 0; k < 5; k++) {
                    if (block.Inodes[j].Direct[k] != 0) {
                        printf(" %d", block.Inodes[j].Direct[k]);
                    }  
                }
                printf("\n");

                if (block.Inodes[j].Indirect != 0) {
                    printf("    indirect blocks: %d\n", block.Inodes[j].Indirect);
                    // indirect block
                    Block indi_block;                

                    disk->read(block.Inodes[j].Indirect, indi_block.Data);

                    printf("    indirect data blocks:");
                    for (uint32_t k = 0; k < POINTERS_PER_BLOCK; k++) {
                        uint32_t block_ind = indi_block.Pointers[k];
                        if (block_ind != 0) {
                            printf(" %d", block_ind);
                        } 
                    }
                    printf("\n");
                }

            }
        }
    }
}

// Format file system ----------------------------------------------------------

bool FileSystem::format(Disk *disk) {
    Block block;

    // Write superblock
    int n = disk->size();
    block.Super.Blocks = n;
    block.Super.InodeBlocks = (n%10 == 0? n/10: (n/10)+1);
    block.Super.Inodes = FileSystem::INODES_PER_BLOCK * block.Super.InodeBlocks;
    disk->write(0, block.Data);

    for (uint32_t i = 1; i < block.Super.Blocks; i++) {
        char buf[Disk::BLOCK_SIZE];
        memset(buf, 0, Disk::BLOCK_SIZE);
        disk->write(i, buf); 
    }

    // Clear all other blocks
    return true;
}

// Mount file system -----------------------------------------------------------
bool FileSystem::mount(Disk *disk) {
    if (disk->mounted())
        return false;

    // Read superblock
    Block sblock;
    disk->read(0, sblock.Data); 
    if (sblock.Super.MagicNumber != FileSystem::MAGIC_NUMBER)
        return false;

    // Set device and mount

    // Copy metadata

    // Allocate free block bitmap
    offset = sblock.Super.InodeBlocks+1;
    uint32_t fbm_size = sblock.Super.Blocks-offset;
    fbm = new unsigned char[fbm_size];
    memset(fbm, 0, fbm_size);

    // Allocate inode table
    itable_size = sblock.Super.InodeBlocks*INODES_PER_BLOCK;
    itable = new unsigned char[itable_size];
    memset(fbm, 0, fbm_size);

    Block iblock;
    for (uint32_t i = 1; i <= sblock.Super.InodeBlocks; i++) {
        disk->read(i, iblock.Data);
        for (uint32_t j = 0; j < INODES_PER_BLOCK; j++) {
            if (iblock.Inodes[j].Valid) {
                itable[(i-1)*INODES_PER_BLOCK+j] = 1; 
                // checking 5  direct pointers
                for (uint32_t k = 0; k < POINTERS_PER_INODE; k++) {
                    uint32_t block_ind = iblock.Inodes[j].Direct[k];
                    if (block_ind != 0) {
                        fbm[block_ind-offset] = 1; 
                    } 
                }

                if (iblock.Inodes[j].Indirect != 0) {
                    // indirect block
                    Block indi_block;                

                    disk->read(iblock.Inodes[j].Indirect, indi_block.Data);

                    for (uint32_t k = 0; k < POINTERS_PER_BLOCK; k++) {
                        uint32_t block_ind = indi_block.Pointers[k];
                        if (block_ind != 0) {
                            fbm[block_ind-offset] = 1; 
                        } 
                    }
                }
            }
        }
    }
    disk->mount();
    this->disk = disk;

    /* for (uint32_t i = 0 ; i < itable_size; i++) { */
    /*     printf("%d: %d\n", i, itable[i]); */ 
    /* } */

    return true;
}

// Create inode ----------------------------------------------------------------

ssize_t FileSystem::create() {
    bool found = false;
    // Locate free inode in inode table
    uint32_t i;
    for (i = 0; i < itable_size; i++) {
        if (itable[i] == 0) {
            found = true;
            break;
        } 
    }
    if (!found) return -1;
    
    // Record inode if found
    Inode node;
    memset(&node, 0, sizeof(node));
    node.Valid = 1;
    save_inode(i, &node);

    return i;
}

// Remove inode ----------------------------------------------------------------

bool FileSystem::remove(size_t inumber) {
    // Load inode information
    Inode node;
    load_inode(inumber, &node);

    // Free direct blocks
    for(int i=0;i<5;i++) { 
        uint32_t t = node.Direct[i];
        if (t != 0) {
            fbm[t-offset] = 0; 
        }
        node.Direct[i] = 0;
    }

    // Free indirect blocks
    if (node.Indirect != 0) {
        // clear pointer block
        Block pblock;
        disk->read(node.Indirect, pblock.Data);
        for (uint32_t i = 0; i < POINTERS_PER_BLOCK; i++) {
            uint32_t t = pblock.Pointers[i]; 
            if (t != 0) {
                fbm[t-offset] = 0;
                pblock.Pointers[i] = 0;
            }
        }
        
        disk->write(node.Indirect, pblock.Data);
        node.Indirect = 0;
        fbm[node.Indirect-offset] = 0;
    }


    // Clear inode in inode table
    save_inode(inumber, &node);
    itable[inumber] = 0;
    return true;
}

// Inode stat ------------------------------------------------------------------

ssize_t FileSystem::stat(size_t inumber) {
    // Load inode information
    Inode node;
    load_inode(inumber, &node);
    
    return node.Size;
}

// Read from inode -------------------------------------------------------------

ssize_t FileSystem::read(size_t inumber, char *data, size_t length, size_t offset) {
    // Load inode information
    if (itable[inumber] == 0) return -1;

    Inode node;
    load_inode(inumber, &node);

    // the beginning block
    uint32_t begin_block = (offset / Disk::BLOCK_SIZE) + 1;

    // number of the blocks should be read
    uint32_t num_block_read = ((offset - ((begin_block-1)*Disk::BLOCK_SIZE) +
                               length) / Disk::BLOCK_SIZE) + 1;

    

    
    // Read block and copy to data
    return 0;
}

// Write to inode --------------------------------------------------------------
ssize_t FileSystem::write(size_t inumber, char *data, size_t length, size_t offset) {

    // Load inode
    
    // Write block and copy to data
    return 0;
}

bool FileSystem::load_inode(size_t inumber, Inode *node) {
    
    uint32_t block_ind = inumber / INODES_PER_BLOCK; 
    Block iblock;

    disk->read(block_ind, iblock.Data);


    *node = iblock.Inodes[inumber];

    // Record inode if found
    return true;
}


bool FileSystem::save_inode(size_t inumber, Inode *node) {
    
    uint32_t block_ind = inumber / INODES_PER_BLOCK; 
    Block iblock;
    
    iblock.Inodes[inumber] = *node;

    disk->write(block_ind, iblock.Data);

    return true;
}


bool FileSystem::read_nth_block(size_t inumber, size_t nthblock, Block *block) {
    
    Inode node;
    load_inode(inumber, &node);

    if (inumber <= 5) {
        
    }
    else {
    
    }
}


