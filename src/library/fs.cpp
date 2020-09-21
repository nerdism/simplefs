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
    for (uint32_t i = INODE_BLOCKS_OFFSET; i < block.Super.InodeBlocks; i++) {
        disk->write(i, buf); 
    }

    // make the inode zero
    Inode izero = {0};
    izero.Valid = true;
    
    if (!save_inode(0, &izero)) return false;

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
    
    // save inode if found
    Inode node = {0};
    node.Valid = 1;

    if (!save_inode(i, &node))
        return false;

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

    if (!add_new_dirent(new_dirent)) {

        return false;
    }
    // save the dirent to the current dirent
    // add_dirent

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
    /* printf("length: %ld, offset: %ld\n", length, offset); */
    /* printf("m_itable: %p m_free_bitmap: %p\n", this->m_itable, this->m_free_bitmap); */
    /* for (uint32_t i = 0; i < m_itable_size; i++) { */
    // __m_itable_size
    /*     printf("%d", m_itable[i]); */ 
    /* } */
    if (m_itable[inumber] == 0) return -1;

    Inode node;
    // Load inode information
    load_inode(inumber, &node);

    if (offset > node.Size) return 0;

    // the beginning block
    uint32_t begin_block = (offset / Disk::BLOCK_SIZE);

    // actual length that can be read;
    uint32_t alength = node.Size - offset;

    if (alength <= 0) return 0;

    /*
     * meaning: if length was greater than file size
     *          just make the actual length the 
     *          the remaining length of the file
     */ 
    if (alength >= length) alength = length;
        

    // number of the blocks should be read
    uint32_t num_block_read = ((offset - ((begin_block-1)*Disk::BLOCK_SIZE) +
                               alength) / Disk::BLOCK_SIZE) + 1;

    // printf("%d",num_block_read);
    
    uint32_t bytes_read = 0;

    for (uint32_t i = 0; i < num_block_read; i++) {
        Block block;
        if (!read_nth_block(inumber, begin_block+i, &block)) {

            // printf("here..........\n");
            return bytes_read;
        }

        // inside 
        /* uint32_t block_offset = i * Disk::BLOCK_SIZE; */
        uint32_t read_begin = offset % Disk::BLOCK_SIZE;
        uint32_t read_len;

        if (read_begin + alength < Disk::BLOCK_SIZE) {
            read_len = read_begin + alength; 
        }
        else {
            read_len = Disk::BLOCK_SIZE - read_begin; 
        }
        
        strncpy(data, &block.Data[read_begin], read_len);

        data += read_len;
        bytes_read += read_len;
        alength -= read_len;
    }
    
    // Read block and copy to data
    return bytes_read;
}

// Write to inode --------------------------------------------------------------
ssize_t FileSystem::write(size_t inumber, char *data, 
                          size_t length, size_t offset) {

    Inode node;
    load_inode(inumber, &node);
    if (!node.Valid) {
        return -1; 
    }

    
    uint32_t free_block_num = allocate_free_block();
    node.Direct[0] = free_block_num;
    Block block;
    
    uint64_t byte_count = 0;
    while (byte_count < length) {
        block.Data[byte_count] = data[byte_count];
        byte_count++;
    }
    node.Size = byte_count;

    disk->write(node.Direct[0], block.Data);
    save_inode(inumber, &node);
    return byte_count;
    /*printf("bp 1\n");*/
    /*if (m_itable[inumber] == 0) return -1;*/

    /*// Load inode*/
    /*Inode node;*/
    /*// Load inode information*/
    /*load_inode(inumber, &node);*/
    /*printf("bp 2\n");*/

    /*if (offset > node.Size) return 0;*/

    /*// The beginning block*/
    /*uint32_t begin_block = (offset / Disk::BLOCK_SIZE) + 1;*/   

    /*// number of the blocks should be read*/
    /*uint32_t num_block_write = ((offset - ((begin_block-1)*Disk::BLOCK_SIZE) +*/
    /*                           length) / Disk::BLOCK_SIZE) + 1;*/

    /*uint32_t write_bytes = 0;*/
    /*printf("bp 3\n");*/
    /*printf("number of block to write: %d\n", num_block_write);*/
    /*for (uint32_t i = 0; i < num_block_write; i++) {*/
    /*    Block block;*/
    /*    uint32_t current_block = begin_block+i;*/

    /*    printf("bp 4\n");*/
    /*    printf("current block: %d\n", current_block);*/

    /*    */
    /*     * if nth block of inode is not set*/ 
    /*     * then allocate new free block to inode*/ 
    /*     */
    /*    if (!read_nth_block(inumber, current_block, &block)) {*/

    /*        printf("bp 5\n");*/
    /*        uint32_t fb = allocate_free_block();*/ 
    /*        if (current_block <= 5) {*/
    /*            node.Direct[current_block-1] = fb;*/
    /*            printf("bp 5.1\n");*/
    /*            save_inode(inumber, &node);*/
    /*        }*/
    /*        else {*/
    /*            if (node.Indirect == 0) {*/
    /*                uint32_t indirect_fb = allocate_free_block();*/
    /*                node.Indirect = indirect_fb;*/
    /*                save_inode(inumber, &node);*/
    /*            }*/
    /*            Block indi_block;*/

    /*            disk->read(node.Indirect, indi_block.Data);*/

    /*            printf("bp 5.2\n");*/
                
               /* for (uint32_t m = 0; m < POINTERS_PER_BLOCK; m++) { */
               /*     printf("bp 5.3\n"); */
               /*     if (current_block == m+6) { */
               /*         break; */
               /*     } */
               /* } */ 

    /*            indi_block.Pointers[current_block-INDIRECT_OFFSET] = fb;*/
    /*            disk->write(node.Indirect, indi_block.Data);*/
    /*        }*/
    /*        printf("bp 6\n");*/

           /* if (read_nth_block(inumber, current_block, &block)) { */
           /*     printf("loaded nth block after allocating\n"); */   
           /* } */
           /* else { */
           /*     printf("can not find nth block after allocating\n"); */   
           /* } */
    /*    }*/
    /*    uint32_t write_begin  = offset % Disk::BLOCK_SIZE;*/
    /*    if (current_block == begin_block) write_begin = 0;*/

    /*    uint32_t write_length = Disk::BLOCK_SIZE - write_begin;*/
    /*    if (current_block == begin_block+num_block_write-1);*/
            

       /* strncpy(&block.Data[write_begin], data, write_length); */
    /*    uint32_t count_bytes = 0;*/
    /*    while (data[count_bytes] != '\0'   ||*/ 
    /*           count_bytes == write_length*/
    /*           ) {*/
    /*        block.Data[write_begin++] = data[count_bytes++];*/ 
    /*    }*/

    /*    data += count_bytes;*/ 
    /*    write_bytes += count_bytes;*/

    /*    printf("bp 7\n");*/
        
    /*    save_nth_block(inumber, current_block, &block);*/
    /*}*/
    /*node.Size += write_bytes;*/
    /*save_inode(inumber, &node);*/

    /*printf("write_bytes %d\n", write_bytes);*/
    /*printf("bp 8\n");*/
    /*return write_bytes;*/
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
    disk->read(block_ind+1, iblock.Data); 
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
        for (uint32_t i = 0; i < POINTERS_PER_BLOCK; i++) {
            t = dblock.Pointers[i]; 
            if (t == 0) return false;
            if (i == nthblock-POINTERS_PER_INODE) {
                disk->read(t, block->Data);    
                break;
            }
        }        

    }
    return true;
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

    if (nthblock <= POINTERS_PER_INODE) {
        uint32_t t = node.Direct[nthblock-1]; 
        if (t == 0) return false;
        disk->write(t, block->Data);
        return true;
    }
    else {
        
    }
    return true;
}

bool FileSystem::add_new_dirent(const Dirent &dirent) {
    uint32_t inum = m_current_dir.Inode;
    uint32_t total_inode_blocks = POINTERS_PER_INODE + POINTERS_PER_BLOCK;
    Block block;

    // iterate over the inode dirent blocks
    for (uint32_t b = 0; b < total_inode_blocks; b++) {

        if (!read_nth_block(inum, b, &block)) return false;

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


