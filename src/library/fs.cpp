// fs.cpp: File System

#include "sfs/fs.h"

#include <algorithm>

#include <assert.h>
#include <stdio.h>
#include <string.h>

void FileSystem::print_dirent(Dirent &dirent) {
    for (int i = 0; i < dirent.NameLength; i++) {
        printf("%c", dirent.Name[i]);
    }
}


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
                    printf("    indirect block: %d\n", block.Inodes[j].Indirect);
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
    if (disk->mounted())
        return false;
    Block block;

    int n = disk->size();
    block.Super.MagicNumber = FileSystem::MAGIC_NUMBER;
    block.Super.Blocks = n;
    block.Super.InodeBlocks = (n%10 == 0? n/10: (n/10)+1);
    block.Super.Inodes = FileSystem::INODES_PER_BLOCK * block.Super.InodeBlocks;
    disk->write(0, block.Data);

    char buf[Disk::BLOCK_SIZE];
    memset(buf, 0, Disk::BLOCK_SIZE);

    // writing every inode block with zeros
    for (uint32_t i = INODE_BLOCKS_OFFSET; i <= block.Super.InodeBlocks; i++) {
        disk->write(i, buf); 
    }

    // make the inode zero
    Inode izero = {0};
    izero.Valid = true;
    
    // since using save_inode we need this (bad design)
    this->disk = disk;

    if (!save_inode(0, &izero)) return false;

    // don't have to do this
    this->disk = nullptr;

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

    if (sblock.Super.Blocks != disk->size())
        return false;

    uint32_t n = disk->size();
    uint32_t inode_blocks = (n%10 == 0? n/10: (n/10)+1);

    if (sblock.Super.InodeBlocks != inode_blocks)
        return false;

    uint32_t inodes  = FileSystem::INODES_PER_BLOCK * sblock.Super.InodeBlocks;

    if (sblock.Super.Inodes != inodes)
        return false;


    // Allocate free block bitmap
    m_offset = sblock.Super.InodeBlocks+1;
    m_free_bitmap_size = sblock.Super.Blocks-m_offset;
    m_free_bitmap = new unsigned char[m_free_bitmap_size];
    memset(m_free_bitmap, 0, m_free_bitmap_size);

    // Allocate inode table
    m_itable_size = sblock.Super.InodeBlocks*INODES_PER_BLOCK;
    m_itable = new unsigned char[m_itable_size];
    memset(m_itable, 0, m_free_bitmap_size);

    Block iblock;
    for (uint32_t i = 1; i <= sblock.Super.InodeBlocks; i++) {
        disk->read(i, iblock.Data);
        for (uint32_t j = 0; j < INODES_PER_BLOCK; j++) {

            if (iblock.Inodes[j].Valid) {
                m_itable[(i-1)*INODES_PER_BLOCK+j] = 1; 

                // checking 5  direct pointers
                for (uint32_t k = 0; k < POINTERS_PER_INODE; k++) {
                    uint32_t block_ind = iblock.Inodes[j].Direct[k];
                    if (block_ind != 0) {
                        m_free_bitmap[block_ind-m_offset] = 1; 
                    } 
                }

                if (iblock.Inodes[j].Indirect != 0) {
                    // indirect block
                    Block indi_block;                

                    disk->read(iblock.Inodes[j].Indirect, indi_block.Data);
// fs.cpp: File System

#include "sfs/fs.h"

#include <algorithm>

#include <assert.h>
#include <stdio.h>
#include <string.h>

void FileSystem::print_dirent(Dirent &dirent) {
    for (int i = 0; i < dirent.NameLength; i++) {
        printf("%c", dirent.Name[i]);
    }
}


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
                    printf("    indirect block: %d\n", block.Inodes[j].Indirect);
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
    if (disk->mounted())
        return false;
    Block block;

    int n = disk->size();
    block.Super.MagicNumber = FileSystem::MAGIC_NUMBER;
    block.Super.Blocks = n;
    block.Super.InodeBlocks = (n%10 == 0? n/10: (n/10)+1);
    block.Super.Inodes = FileSystem::INODES_PER_BLOCK * block.Super.InodeBlocks;
    disk->write(0, block.Data);

    char buf[Disk::BLOCK_SIZE];
    memset(buf, 0, Disk::BLOCK_SIZE);

    // writing every inode block with zeros
    for (uint32_t i = INODE_BLOCKS_OFFSET; i <= block.Super.InodeBlocks; i++) {
        disk->write(i, buf); 
    }

    // make the inode zero
    Inode izero = {0};
    izero.Valid = true;
    
    // since using save_inode we need this (bad design)
    this->disk = disk;

    if (!save_inode(0, &izero)) return false;

    // don't have to do this
    this->disk = nullptr;

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

    if (sblock.Super.Blocks != disk->size())
        return false;

    uint32_t n = disk->size();
    uint32_t inode_blocks = (n%10 == 0? n/10: (n/10)+1);

    if (sblock.Super.InodeBlocks != inode_blocks)
        return false;

    uint32_t inodes  = FileSystem::INODES_PER_BLOCK * sblock.Super.InodeBlocks;

    if (sblock.Super.Inodes != inodes)
        return false;


    // Allocate free block bitmap
    m_offset = sblock.Super.InodeBlocks+1;
    m_free_bitmap_size = sblock.Super.Blocks-m_offset;
    m_free_bitmap = new unsigned char[m_free_bitmap_size];
    memset(m_free_bitmap, 0, m_free_bitmap_size);

    // Allocate inode table
    m_itable_size = sblock.Super.InodeBlocks*INODES_PER_BLOCK;
    m_itable = new unsigned char[m_itable_size];
    memset(m_itable, 0, m_free_bitmap_size);

    Block iblock;
    for (uint32_t i = 1; i <= sblock.Super.InodeBlocks; i++) {
        disk->read(i, iblock.Data);
        for (uint32_t j = 0; j < INODES_PER_BLOCK; j++) {

            if (iblock.Inodes[j].Valid) {
                m_itable[(i-1)*INODES_PER_BLOCK+j] = 1; 

                // checking 5  direct pointers
                for (uint32_t k = 0; k < POINTERS_PER_INODE; k++) {
                    uint32_t block_ind = iblock.Inodes[j].Direct[k];
                    if (block_ind != 0) {
                        m_free_bitmap[block_ind-m_offset] = 1; 
                    } 
                }

                if (iblock.Inodes[j].Indirect != 0) {
                    // indirect block
                    Block indi_block;                

                    disk->read(iblock.Inodes[j].Indirect, indi_block.Data);

                    for (uint32_t k = 0; k < POINTERS_PER_BLOCK; k++) {
                        uint32_t block_ind = indi_block.Pointers[k];
                        if (block_ind != 0) {
                            m_free_bitmap[block_ind-m_offset] = 1; 
                        } 
                    }
                }
            }
        }
    }
    disk->mount();

    this->disk = disk;

    Inode node;
    if (!load_inode(0, &node)) {
        printf("failed on reading root directory\n");
        return false; 
    }
    if (!node.Valid){

        printf("[-] root directory not found\n");
        return false;
    } 

    
    m_current_dir.Inode   = 0;
    m_current_dir.Type    = static_cast<uint8_t>(DirentType::DIR_T);
    strcpy(m_current_dir.Name, "/");
    printf("[+] root dir mounted\n");

    m_is_mounted = true;

    return true;
}

// Create inode ----------------------------------------------------------------

bool FileSystem::mkfile(const char *name) {
    if (!make_file_or_dir(name, DirentType::FILE_T))
        return false;
    return true;
}

bool FileSystem::mkdir(const char *name) {
    if (!make_file_or_dir(name, DirentType::DIR_T))
        return false;
    return true;
}

char *FileSystem::get_current_dir() {
    return m_current_dir.Name;
}

bool FileSystem::make_file_or_dir(const char *name, DirentType type) {
    if (!mounted()) {
        printf("must be mounted\n");
        return false; 
    }
    bool found = false;
    // Locate free inode in inode table
    uint32_t i;
    for (i = 0; i < m_itable_size; i++) {
        if (m_itable[i] == 0) {
            found = true;
            break;
        } 
    }
    if (!found) return -1;
    
    Inode node = {0};
    node.Valid = 1;


    // set the inode on the inode table that exist
    m_itable[i] = 1;

    // make Dirent for this inode in the current dirent 
    Dirent new_dirent;
    new_dirent.Inode = i;
    if (type == DirentType::FILE_T) {
        new_dirent.Type  = static_cast<uint8_t>(DirentType::FILE_T);
    }
    else if (type == DirentType::DIR_T) {
        new_dirent.Type  = static_cast<uint8_t>(DirentType::DIR_T);
    }
    else 
        return false;

    new_dirent.NameLength = strnlen(name, DIRENT_NAME_SIZE);
    strncpy(new_dirent.Name, name, DIRENT_NAME_SIZE);

    // save the dirent to the current dirent
    // add_dirent
    if (!add_new_dirent(new_dirent, m_current_dir.Inode)) {
        return false;
    }
    else {
        if (!save_inode(i, &node))
            return false;
        else {
            if (type == DirentType::DIR_T) {
                // add new dirent to the content of the new directory inode
                Dirent previous_dir;
                previous_dir.Inode = m_current_dir.Inode;
                previous_dir.Type  = static_cast<uint8_t>(DirentType::DIR_T);
                
                previous_dir.NameLength = 2;
                strncpy(previous_dir.Name, "..", 2);
                
                if (!add_new_dirent(previous_dir, i))
                    return false;
            }
        }
    }

    return true;
}

// Remove inode ----------------------------------------------------------------

bool FileSystem::remove(size_t inumber) {
    if (!disk->mounted())
        return false;
    // Load inode information
    Inode node;
    load_inode(inumber, &node);
    if (!node.Valid)
        return false;

    // Free direct blocks
    for(int i=0;i<5;i++) { 
        uint32_t t = node.Direct[i];
        if (t != 0) {
            m_free_bitmap[t-m_offset] = 0; 
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
                m_free_bitmap[t-m_offset] = 0;
                pblock.Pointers[i] = 0;
            }
        }
        
        disk->write(node.Indirect, pblock.Data);

        // m_free_bitmap[node.Indirect-offset] = 0;
        unset_free_bitmap(node.Indirect);
        node.Indirect = 0;
    }

    node.Valid = 0;
    // Clear inode in inode table
    save_inode(inumber, &node);
    m_itable[inumber] = 0;
    return true;
}

// Inode stat ------------------------------------------------------------------

ssize_t FileSystem::stat(size_t inumber) {

    // Load inode information
    Inode node;
    load_inode(inumber, &node);
    if (!node.Valid)
        return -1;
    
    return node.Size;
}

// Read from inode -------------------------------------------------------------

ssize_t FileSystem::read(size_t inumber, char *data, size_t length, size_t offset) {
    if (m_itable[inumber] == 0) return -1;

    Inode node;

    // Load inode information
    load_inode(inumber, &node);

    if (offset > node.Size) return 0;
    
    // Read block and copy to data
    return bytes_read;
}

// Write to inode --------------------------------------------------------------
ssize_t FileSystem::write(size_t inumber, char *data, 
                          size_t length) {

    Inode node;
    load_inode(inumber, &node);
    if (!node.Valid) {
        return -1; 
    }
    // find how many data blocks we need to write
    uint32_t blocks_count = length / Disk::BLOCK_SIZE;
    if (length % Disk::BLOCK_SIZE)
        ++blocks_count;

    ssize_t total_written_bytes = 0;
    Block block;
    for (int index = 0; i < blocks_count; ++index) {

        uint32_t write_size = length % Disk::BLOCK_SIZE;
        memcpy(block.Data, data, write_size);

        if (!save_nth_block(inumber, index, &block)) {
            printf("error while writing to disk save_nth_block\n"); 
            return -1;
        } 

        data += write_size;
        node.Size += write_size;
        total_written_bytes += write_size;
    }

    return total_written_bytes;

}

bool FileSystem::load_inode(size_t inumber, Inode *node) {
    
    uint32_t block_ind = inumber / INODES_PER_BLOCK; 
    Block iblock;

    disk->read(block_ind+1, iblock.Data);

    *node = iblock.Inodes[inumber];

    // Record inode if found
    return true;
}


bool FileSystem::save_inode(size_t inumber, Inode *node) {
    
    uint32_t block_ind = inumber / INODES_PER_BLOCK; 
    Block iblock;
    disk->read(block_ind+INODE_BLOCKS_OFFSET, iblock.Data); 
    iblock.Inodes[inumber] = *node;

    disk->write(block_ind+1, iblock.Data);

    return true;
}


bool FileSystem::read_nth_block(size_t inumber, size_t nthblock, Block *block) {
    Inode node;
    load_inode(inumber, &node);

    if (nthblock < POINTERS_PER_INODE) {
        uint32_t t = node.Direct[nthblock]; 
        if (t == 0) return false;
        disk->read(t, block->Data);
        return true;
    }
    else {
        uint32_t t = node.Indirect;
        if (t == 0) return false;
        Block dblock;
        
        disk->read(t, dblock.Data);

        t = dblock.Pointers[nthblock-POINTERS_PER_INODE]; 
        if (t == 0) return false;

        disk->read(t, block->Data);    
        return true;
    }
}

ssize_t FileSystem::allocate_free_block() {
    for (uint32_t i = 0; i < m_free_bitmap_size; i++) {
        if (m_free_bitmap[i] == 0) {
            m_free_bitmap[i] = 1;
            return (i+m_offset); 
        }
    }
    return -1;
}

bool FileSystem::save_nth_block(size_t inumber, 
                                size_t nthblock, Block *block) {

    Inode node;
    load_inode(inumber, &node);

    if (nthblock < POINTERS_PER_INODE) {
        ssize_t t = node.Direct[nthblock]; 
        if (t == 0) {
            t = allocate_free_block();
            node.Direct[nthblock] = t;
            save_inode(inumber, &node);
        }
        disk->write(t, block->Data);
        return true;
    }
    else {
        uint32_t indirect_block = node.Indirect;
        if (indirect_block == 0) {
            indirect_block = allocate_free_block();
            node.Indirect = indirect_block;
            save_inode(inumber, &node);
        } 
        Block dblock;

        disk->read(indirect_block, dblock.Data);

        uint32_t t = dblock.Pointers[nthblock-POINTERS_PER_INODE]; 
        if (t == 0) {
            t = allocate_free_block();
            dblock.Pointers[nthblock-POINTERS_PER_INODE] = t;
            disk->write(indirect_block, dblock.Data);
        }

        disk->write(t, block->Data);
        return true;
    }
}

bool FileSystem::add_new_dirent(const Dirent &dirent, uint32_t inum) {
    // uint32_t inum = m_current_dir.Inode;
    uint32_t total_inode_blocks = POINTERS_PER_INODE + POINTERS_PER_BLOCK;
    Block block = {0};

    // iterate over the inode dirent blocks
    for (uint32_t b = 0; b < total_inode_blocks; b++) {

        if (!read_nth_block(inum, b, &block)) {
            // make the nth block here
            save_nth_block(inum, b, &block);
        }

        for (uint32_t i = 0; i < DIRENTS_PER_BLOCK; i++) {
            uint8_t type = block.Dirents[i].Type;
            if (type == static_cast<uint8_t>(DirentType::FILE_T) ||
                type == static_cast<uint8_t>(DirentType::DIR_T)) {

                continue;
            } 
                
            block.Dirents[i].Inode = dirent.Inode;
            block.Dirents[i].Type = dirent.Type;
            block.Dirents[i].NameLength = dirent.NameLength;
            strncpy(block.Dirents[i].Name, dirent.Name, DIRENT_NAME_SIZE) ;
            save_nth_block(inum, b, &block);
            return true;
        }
    }
    return false;
}


void FileSystem::list() {
    if (!mounted()) {
        printf("must be mounted\n");
        return;
    } 

    Inode node;
    load_inode(m_current_dir.Inode, &node);

    Block block;
    for (uint32_t i = 0; i < POINTERS_PER_INODE+POINTERS_PER_BLOCK; i++) {
        if (!read_nth_block(m_current_dir.Inode, i, &block))
            break;

        for (uint32_t j = 0; j < DIRENTS_PER_BLOCK; j++) {
            uint8_t type = block.Dirents[j].Type;

            if (type == static_cast<uint8_t>(DirentType::FILE_T)) {
                
                print_dirent(block.Dirents[j]);
                printf("\n");
            }
            else if (type == static_cast<uint8_t>(DirentType::DIR_T)) {
                printf("\x1b[94m");
                print_dirent(block.Dirents[j]);
                printf("\x1b[39m");
                printf("\n");
            }
        }
    }
}

bool FileSystem::change_directory(char *name) {
    if (!mounted()) {
        printf("must be mounted\n");
        return false;
    } 

    Inode node;
    load_inode(m_current_dir.Inode, &node);

    Block block;
    for (uint32_t i = 0; i < POINTERS_PER_INODE+POINTERS_PER_BLOCK; i++) {

        if (!read_nth_block(m_current_dir.Inode, i, &block))
            break;

        for (uint32_t j = 0; j < DIRENTS_PER_BLOCK; j++) {
            uint8_t type = block.Dirents[j].Type;

            if (type == static_cast<uint8_t>(DirentType::DIR_T)) {
                if (strncmp(block.Dirents[j].Name, name, block.Dirents[j].NameLength) == 0 &&
                    strlen(name) == block.Dirents[j].NameLength) {

                    m_current_dir = block.Dirents[j];
                    if (m_current_dir.Inode == 0) {
                        strcpy(m_current_dir.Name, "/");
                    }

                    return true;
                }
            }
        }
    }
    return true;
}

ssize_t FileSystem::find_inumber(char *name, size_t directory_inumber) {
    if (!mounted()) {
        printf("must be mounted\n");
        return;
    } 

    Inode node;
    load_inode(m_current_dir.Inode, &node);

    Block block;
    for (uint32_t i = 0; i < POINTERS_PER_INODE+POINTERS_PER_BLOCK; i++) {
        if (!read_nth_block(m_current_dir.Inode, i, &block))
            break;

        for (uint32_t j = 0; j < DIRENTS_PER_BLOCK; j++) {
            uint8_t type = block.Dirents[j].Type;

            if (type == static_cast<uint8_t>(DirentType::FILE_T) &&
                strncmp(name, block.Dirents[j].Name, block.Dirents[j].NameLength)) {
                return block.Dirents[j].Inode;
            }
        }
    }
}


                    for (uint32_t k = 0; k < POINTERS_PER_BLOCK; k++) {
                        uint32_t block_ind = indi_block.Pointers[k];
                        if (block_ind != 0) {
                            m_free_bitmap[block_ind-m_offset] = 1; 
                        } 
                    }
                }
            }
        }
    }
    disk->mount();

    this->disk = disk;

    Inode node;
    if (!load_inode(0, &node)) {
        printf("failed on reading root directory\n");
        return false; 
    }
    if (!node.Valid){

        printf("[-] root directory not found\n");
        return false;
    } 

    
    m_current_dir.Inode   = 0;
    m_current_dir.Type    = static_cast<uint8_t>(DirentType::DIR_T);
    strcpy(m_current_dir.Name, "/");
    printf("[+] root dir mounted\n");

    m_is_mounted = true;

    return true;
}

// Create inode ----------------------------------------------------------------

bool FileSystem::mkfile(const char *name) {
    if (!make_file_or_dir(name, DirentType::FILE_T))
        return false;
    return true;
}

bool FileSystem::mkdir(const char *name) {
    if (!make_file_or_dir(name, DirentType::DIR_T))
        return false;
    return true;
}

char *FileSystem::get_current_dir() {
    return m_current_dir.Name;
}

bool FileSystem::make_file_or_dir(const char *name, DirentType type) {
    if (!mounted()) {
        printf("must be mounted\n");
        return false; 
    }
    bool found = false;
    // Locate free inode in inode table
    uint32_t i;
    for (i = 0; i < m_itable_size; i++) {
        if (m_itable[i] == 0) {
            found = true;
            break;
        } 
    }
    if (!found) return -1;
    
    Inode node = {0};
    node.Valid = 1;


    // set the inode on the inode table that exist
    m_itable[i] = 1;

    // make Dirent for this inode in the current dirent 
    Dirent new_dirent;
    new_dirent.Inode = i;
    if (type == DirentType::FILE_T) {
        new_dirent.Type  = static_cast<uint8_t>(DirentType::FILE_T);
    }
    else if (type == DirentType::DIR_T) {
        new_dirent.Type  = static_cast<uint8_t>(DirentType::DIR_T);
    }
    else 
        return false;

    new_dirent.NameLength = strnlen(name, DIRENT_NAME_SIZE);
    strncpy(new_dirent.Name, name, DIRENT_NAME_SIZE);

    // save the dirent to the current dirent
    // add_dirent
    if (!add_new_dirent(new_dirent, m_current_dir.Inode)) {
        return false;
    }
    else {
        if (!save_inode(i, &node))
            return false;
        else {
            if (type == DirentType::DIR_T) {
                // add new dirent to the content of the new directory inode
                Dirent previous_dir;
                previous_dir.Inode = m_current_dir.Inode;
                previous_dir.Type  = static_cast<uint8_t>(DirentType::DIR_T);
                
                previous_dir.NameLength = 2;
                strncpy(previous_dir.Name, "..", 2);
                
                if (!add_new_dirent(previous_dir, i))
                    return false;
            }
        }
    }

    return true;
}

// Remove inode ----------------------------------------------------------------

bool FileSystem::remove(size_t inumber) {
    if (!disk->mounted())
        return false;
    // Load inode information
    Inode node;
    load_inode(inumber, &node);
    if (!node.Valid)
        return false;

    // Free direct blocks
    for(int i=0;i<5;i++) { 
        uint32_t t = node.Direct[i];
        if (t != 0) {
            m_free_bitmap[t-m_offset] = 0; 
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
                m_free_bitmap[t-m_offset] = 0;
                pblock.Pointers[i] = 0;
            }
        }
        
        disk->write(node.Indirect, pblock.Data);

        // m_free_bitmap[node.Indirect-offset] = 0;
        unset_free_bitmap(node.Indirect);
        node.Indirect = 0;
    }

    node.Valid = 0;
    // Clear inode in inode table
    save_inode(inumber, &node);
    m_itable[inumber] = 0;
    return true;
}

// Inode stat ------------------------------------------------------------------

ssize_t FileSystem::stat(size_t inumber) {

    // Load inode information
    Inode node;
    load_inode(inumber, &node);
    if (!node.Valid)
        return -1;
    
    return node.Size;
}

// Read from inode -------------------------------------------------------------

ssize_t FileSystem::read(size_t inumber, char *data) {

    Inode node;

    // Load inode information
    if (!load_inode(inumber, &node)
        return -1;

    uint32_t block_count = node.Size / Disk::BLOCK_SIZE;
    if (node.Size % Disk::BLOCK_SIZE)
        ++block_count;
    
    uint32_t bytes_read = 0;
    uint32_t read_bytes_remind = node.Size;
    Block block;

    for (int index = 0; i < blocks_count; ++index) {
        uint32_t read_size = read_bytes_remind % Disk::BLOCK_SIZE;
        if (!read_nth_block(inumber, index, block)) {
            printf("error when reading read_nth_block"); 
            return -1;
        }
        
    }
    
    // Read block and copy to data
    return bytes_read;
}

// Write to inode --------------------------------------------------------------
ssize_t FileSystem::write(size_t inumber, char *data, 
                          size_t length) {

    Inode node;
    load_inode(inumber, &node);
    if (!node.Valid) {
        return -1; 
    }
    // find how many data blocks we need to write
    uint32_t blocks_count = length / Disk::BLOCK_SIZE;
    if (length % Disk::BLOCK_SIZE)
        ++blocks_count;

    ssize_t total_written_bytes = 0;
    Block block;
    for (int index = 0; i < blocks_count; ++index) {

        uint32_t write_size = length % Disk::BLOCK_SIZE;
        memcpy(block.Data, data, write_size);

        if (!save_nth_block(inumber, index, &block)) {
            printf("error while writing to disk save_nth_block\n"); 
            return -1;
        } 

        data += write_size;
        node.Size += write_size;
        total_written_bytes += write_size;
    }

    return total_written_bytes;

}

bool FileSystem::load_inode(size_t inumber, Inode *node) {
    
    uint32_t block_ind = inumber / INODES_PER_BLOCK; 
    Block iblock;

    disk->read(block_ind+1, iblock.Data);

    *node = iblock.Inodes[inumber];

    // Record inode if found
    return true;
}


bool FileSystem::save_inode(size_t inumber, Inode *node) {
    
    uint32_t block_ind = inumber / INODES_PER_BLOCK; 
    Block iblock;
    disk->read(block_ind+INODE_BLOCKS_OFFSET, iblock.Data); 
    iblock.Inodes[inumber] = *node;

    disk->write(block_ind+1, iblock.Data);

    return true;
}


bool FileSystem::read_nth_block(size_t inumber, size_t nthblock, Block *block) {
    Inode node;
    load_inode(inumber, &node);

    if (nthblock < POINTERS_PER_INODE) {
        uint32_t t = node.Direct[nthblock]; 
        if (t == 0) return false;
        disk->read(t, block->Data);
        return true;
    }
    else {
        uint32_t t = node.Indirect;
        if (t == 0) return false;
        Block dblock;
        
        disk->read(t, dblock.Data);

        t = dblock.Pointers[nthblock-POINTERS_PER_INODE]; 
        if (t == 0) return false;

        disk->read(t, block->Data);    
        return true;
    }
}

ssize_t FileSystem::allocate_free_block() {
    for (uint32_t i = 0; i < m_free_bitmap_size; i++) {
        if (m_free_bitmap[i] == 0) {
            m_free_bitmap[i] = 1;
            return (i+m_offset); 
        }
    }
    return -1;
}

bool FileSystem::save_nth_block(size_t inumber, 
                                size_t nthblock, Block *block) {

    Inode node;
    load_inode(inumber, &node);

    if (nthblock < POINTERS_PER_INODE) {
        ssize_t t = node.Direct[nthblock]; 
        if (t == 0) {
            t = allocate_free_block();
            node.Direct[nthblock] = t;
            save_inode(inumber, &node);
        }
        disk->write(t, block->Data);
        return true;
    }
    else {
        uint32_t indirect_block = node.Indirect;
        if (indirect_block == 0) {
            indirect_block = allocate_free_block();
            node.Indirect = indirect_block;
            save_inode(inumber, &node);
        } 
        Block dblock;

        disk->read(indirect_block, dblock.Data);

        uint32_t t = dblock.Pointers[nthblock-POINTERS_PER_INODE]; 
        if (t == 0) {
            t = allocate_free_block();
            dblock.Pointers[nthblock-POINTERS_PER_INODE] = t;
            disk->write(indirect_block, dblock.Data);
        }

        disk->write(t, block->Data);
        return true;
    }
}

bool FileSystem::add_new_dirent(const Dirent &dirent, uint32_t inum) {
    // uint32_t inum = m_current_dir.Inode;
    uint32_t total_inode_blocks = POINTERS_PER_INODE + POINTERS_PER_BLOCK;
    Block block = {0};

    // iterate over the inode dirent blocks
    for (uint32_t b = 0; b < total_inode_blocks; b++) {

        if (!read_nth_block(inum, b, &block)) {
            // make the nth block here
            save_nth_block(inum, b, &block);
        }

        for (uint32_t i = 0; i < DIRENTS_PER_BLOCK; i++) {
            uint8_t type = block.Dirents[i].Type;
            if (type == static_cast<uint8_t>(DirentType::FILE_T) ||
                type == static_cast<uint8_t>(DirentType::DIR_T)) {

                continue;
            } 
                
            block.Dirents[i].Inode = dirent.Inode;
            block.Dirents[i].Type = dirent.Type;
            block.Dirents[i].NameLength = dirent.NameLength;
            strncpy(block.Dirents[i].Name, dirent.Name, DIRENT_NAME_SIZE) ;
            save_nth_block(inum, b, &block);
            return true;
        }
    }
    return false;
}


void FileSystem::list() {
    if (!mounted()) {
        printf("must be mounted\n");
        return;
    } 

    Inode node;
    load_inode(m_current_dir.Inode, &node);

    Block block;
    for (uint32_t i = 0; i < POINTERS_PER_INODE+POINTERS_PER_BLOCK; i++) {
        if (!read_nth_block(m_current_dir.Inode, i, &block))
            break;

        for (uint32_t j = 0; j < DIRENTS_PER_BLOCK; j++) {
            uint8_t type = block.Dirents[j].Type;

            if (type == static_cast<uint8_t>(DirentType::FILE_T)) {
                
                print_dirent(block.Dirents[j]);
                printf("\n");
            }
            else if (type == static_cast<uint8_t>(DirentType::DIR_T)) {
                printf("\x1b[94m");
                print_dirent(block.Dirents[j]);
                printf("\x1b[39m");
                printf("\n");
            }
        }
    }
}

bool FileSystem::change_directory(char *name) {
    if (!mounted()) {
        printf("must be mounted\n");
        return false;
    } 

    Inode node;
    load_inode(m_current_dir.Inode, &node);

    Block block;
    for (uint32_t i = 0; i < POINTERS_PER_INODE+POINTERS_PER_BLOCK; i++) {

        if (!read_nth_block(m_current_dir.Inode, i, &block))
            break;

        for (uint32_t j = 0; j < DIRENTS_PER_BLOCK; j++) {
            uint8_t type = block.Dirents[j].Type;

            if (type == static_cast<uint8_t>(DirentType::DIR_T)) {
                if (strncmp(block.Dirents[j].Name, name, block.Dirents[j].NameLength) == 0 &&
                    strlen(name) == block.Dirents[j].NameLength) {

                    m_current_dir = block.Dirents[j];
                    if (m_current_dir.Inode == 0) {
                        strcpy(m_current_dir.Name, "/");
                    }

                    return true;
                }
            }
        }
    }
    return true;
}

ssize_t FileSystem::find_inumber(char *name, size_t directory_inumber) {
    if (!mounted()) {
        printf("must be mounted\n");
        return;
    } 

    Inode node;
    load_inode(m_current_dir.Inode, &node);

    Block block;
    for (uint32_t i = 0; i < POINTERS_PER_INODE+POINTERS_PER_BLOCK; i++) {
        if (!read_nth_block(m_current_dir.Inode, i, &block))
            break;

        for (uint32_t j = 0; j < DIRENTS_PER_BLOCK; j++) {
            uint8_t type = block.Dirents[j].Type;

            if (type == static_cast<uint8_t>(DirentType::FILE_T) &&
                strncmp(name, block.Dirents[j].Name, block.Dirents[j].NameLength)) {
                return block.Dirents[j].Inode;
            }
        }
    }
}

