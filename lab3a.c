//NAME: Bibek Ghimire Jehu Lee
//EMAIL: bibekg@ucla.edu jehulee97@g.ucla.edu
//ID: 004569045 404646481

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>

#include "ext2_fs.h"

///FUNCTION TABLE/////////////////////////
void print_error();
void print_usage();
void print_superblock();
void print_group();
void print_freeblock();
void print_freeinode();

void process_everythang();
void process_data_block(const int cur_block, const int ref_block, const int file_offset, const int inode_number, const int max_ind_level, const int curr_ind_level, const bool is_dir, const bool was_referenced);
void print_directories(const int inode_number, const int entries_base, const int file_offset);
void print_indirect(const int inode_number, const int ind_level, const int file_offset, const int cur_block, const int ref_block);
void print_inode(int inode_number, struct ext2_inode* cur_inode, char file_type);

/////////////////////////////////////////

///GLOBALS///
struct ext2_super_block* superblock;
struct ext2_group_desc* group;
struct ext2_inode* inode;
int n_block_groups, n_inodes;
int fild, blocksize, inodesize;

///DRIVER///
int main(int argc, char * argv[]) {
  int n_groups = 0;
  if(argc != 2) { print_usage(); exit(1); }

  fild = open(argv[1], O_RDONLY);
  if(fild < 0 ) {
    print_error();
    exit(1);
  }

  //superblock
  superblock = malloc(sizeof(struct ext2_super_block));
  pread(fild, superblock, EXT2_MIN_BLOCK_SIZE, 1024);
  print_superblock();

  // number of groups
  n_block_groups = 1 + ((superblock->s_blocks_count - 1) / superblock->s_blocks_per_group);
  // total number of inodes
  n_inodes = 1 + ((superblock->s_inodes_count - 1) / superblock->s_inodes_per_group);
  // total blocksize
  blocksize = 1024 << superblock->s_log_block_size;
  // size of each inode
  inodesize = superblock->s_inode_size;

  //block group
  group = malloc(sizeof(struct ext2_group_desc) * n_block_groups);
  pread(fild, group, EXT2_MIN_BLOCK_SIZE, 1024 + EXT2_MIN_BLOCK_SIZE);
  print_group();

  //free block entries
  print_freeblock();

  //free inode entries
  print_freeinode();

  //I-node summary -> Implicit call to directory entries and indirect block references
  process_everythang();


  exit(0); //exit at success
}

///////////////
///UTILITIES///
///////////////

void print_error() {
  fprintf(stderr, "%s\n", strerror(errno));
}

void print_usage() {
  fprintf(stderr, "%s\n", "Usage: ./lab3a [image.img]");
}

// Works
void print_superblock() {
  fprintf(stdout, "SUPERBLOCK,%d,%d,%d,%d,%d,%d,%d\n",
  superblock->s_blocks_count,
  superblock->s_inodes_count,
  (1024 << superblock->s_log_block_size),
  superblock->s_inode_size,
  superblock->s_blocks_per_group,
  superblock->s_inodes_per_group,
  superblock->s_first_ino);
}

// Works
void print_group() {
  int n_blocks_in_group, k;

  for (k = 0; k < n_block_groups; k++) {
    // Not enough blocks to fill up a group
    if (superblock->s_blocks_count < superblock->s_blocks_per_group) {
      n_blocks_in_group = superblock->s_blocks_count;
    }
    // If processing last group, may be incomplete
    else if ( k == n_block_groups -1) {
      n_blocks_in_group = superblock->s_blocks_count - (superblock->s_blocks_per_group * n_block_groups);
    }
    // Default case
    else {
      n_blocks_in_group = superblock->s_blocks_per_group;
    }

    fprintf(stdout, "GROUP,%d,%d,%d,%d,%d,%d,%d,%d\n",
    k,
    n_blocks_in_group,
    superblock->s_inodes_per_group,  //fishy may be incorrect
    group[k].bg_free_blocks_count,
    group[k].bg_free_inodes_count,
    group[k].bg_block_bitmap,
    group[k].bg_inode_bitmap,
    group[k].bg_inode_table);
  }
}

// Works
void print_freeblock() {
  //for every block group, print BFREE if bit == 0
  int k = 0, i = 0, j = 0;
  int bit = 0;

  __u8 buf;

  for (k = 0 ; k < n_block_groups; k++) {
    for (i = 0; i < blocksize; i++) {
      pread(fild, &buf, 1, (group[k].bg_block_bitmap * blocksize) + i);
      for (j = 0; j < 8; j++) {
        bit = buf & (1 << j);
        if (bit == 0) {
          fprintf(stdout, "BFREE,%d\n", (k*n_block_groups) + (i*8) + j + 1);
        }
        //check this
      }
    }
  }

}

// Works
void print_freeinode() {
  int k = 0, i = 0, j = 0;
  int bit = 0;
  __u8 buf;

  for (k = 0 ; k < n_block_groups; k++) {
    for (i = 0; i < blocksize; i++) {
      pread(fild, &buf, 1, (group[k].bg_inode_bitmap * blocksize) + i);
      for (j = 0; j < 8; j++) {
        bit = buf & (1 << j);
        if (bit == 0) {
          fprintf(stdout, "IFREE,%d\n", (k*n_inodes) + (i*8) + j + 1);
        }
        //check this
      }
    }
  }

}

void process_everythang() {
  const int PPB = blocksize / sizeof(int);
  struct ext2_inode* cur_inode = malloc(sizeof(struct ext2_inode));
  int i,j, file_type;
  int file_offset = 0;
  // For each block group, group[i]
  for (i = 0; i < n_block_groups; i++) {
    // For each inode in the table, table[j]
    for (j = 0; j < superblock->s_inodes_per_group; j++) {
      __u32 tableOffset = (group[i].bg_inode_table * blocksize) + (j * inodesize);
      pread(fild, cur_inode, inodesize, tableOffset);
      if (cur_inode->i_mode != 0 && cur_inode->i_links_count != 0) {
        int inode_number = i + j + 1;
        if      ((cur_inode->i_mode & 0x8000) == 0x8000)  { file_type = 'f'; }
        else if ((cur_inode->i_mode & 0xA000) == 0xA000)  { file_type = 's'; }
        else if ((cur_inode->i_mode & 0x4000) == 0x4000)  { file_type = 'd'; }
        else                                              { file_type = '?'; }

        // We now have an inode
        print_inode(inode_number, cur_inode, file_type);

        fprintf(stderr, "12, 13, 14: %d, %d, %d\n", cur_inode->i_block[12], cur_inode->i_block[13], cur_inode->i_block[14]);

        int indirection_level, cur_block;

        // Setup process call for direct blocks
        for (int k = 0; k < 12; k++) {
          cur_block = cur_inode->i_block[k];
          file_offset = k;
          process_data_block(cur_block, 0, file_offset, inode_number, 0, 0, (file_type == 'd'), false);
        }

        // Setup process call for indirect blocks
        for (int k = 0; k < 3; k++) {
          indirection_level = k + 1;
          cur_block = cur_inode->i_block[k + 12];
          if (cur_block == 0) continue;  // value of 0 is the i_block array terminator
          switch (indirection_level) {
            case 1: file_offset = 12; break;
            case 2: file_offset = 12 + PPB; break;
            case 3: file_offset = 12 + PPB + (PPB * PPB); break;
            default: break;
          }
          if (cur_block != 0) {
            process_data_block(cur_block, 0, file_offset, inode_number, indirection_level, 0, (file_type == 'd'), false);
          }
        }
        if (cur_block == 0) continue;  // value of 0 is the i_block array terminator
      }
    } // end: for(j)
  } // end: for(i)
  free(cur_inode);
}

// Recursive-ready function for scanning data blocks
// was_referenced should be false on first call to not print INDIRECT for root block
void process_data_block(const int cur_block, const int ref_block, const int file_offset, const int inode_number, const int max_ind_level, const int curr_ind_level, const bool is_dir, const bool was_referenced) {

  if (was_referenced && cur_block != 0) { print_indirect(inode_number, curr_ind_level, file_offset, cur_block, ref_block); }

  // This means cur_block refers to a block that contains real data, not pointers or 0
  if (curr_ind_level == max_ind_level) {
    if (cur_block != 0) {
      // If the inode is a directory, print a DIRENT CSV for the cur_block
      if (is_dir) { print_directories(inode_number, cur_block * blocksize, file_offset); }
    }
  }
  // cur_block contains pointers, i.e. is an indirect block
  else {
    const int PPB = blocksize / sizeof(int);  // pointers per block
    // Read and process each block indirectly pointed to
    for (int i = 0; i < PPB; i++) {
      int next_block;
      pread(fild, &next_block, 4, (cur_block * blocksize) + (i * sizeof(int)));
      process_data_block(next_block, cur_block, file_offset + i, inode_number, max_ind_level, curr_ind_level + 1, is_dir, true);
    }
  }
}

void print_directories(const int inode_number, const int entries_base, const int file_offset) {
  struct ext2_dir_entry d_entry;
  int entry_offset = 0;

  while(1) {
    pread(fild, &d_entry, sizeof(struct ext2_dir_entry), entries_base + entry_offset);

    // Reached end of entries for this directory or hit empty inode
    if (entry_offset >= 1024 || d_entry.inode == 0) break;

    fprintf(stdout,"DIRENT,%d,%d,%d,%d,%d,'%s'\n",
      inode_number,
      file_offset + entry_offset,
      d_entry.inode,
      d_entry.rec_len,
      d_entry.name_len,
      d_entry.name
    );

    entry_offset += d_entry.rec_len;
  }
}

void print_indirect(const int inode_number, const int ind_level, const int file_offset, const int block_id, const int ref_block) {
  fprintf(stdout, "INDIRECT,%d,%d,%d,%d,%d\n",
    inode_number,
    ind_level,
    file_offset,
    ref_block,
    block_id);
}

void print_inode(int inode_number, struct ext2_inode* cur_inode, char file_type) {

  const time_t c_time_sec = (const time_t) cur_inode->i_ctime;
  const time_t m_time_sec = (const time_t) cur_inode->i_mtime;
  const time_t a_time_sec = (const time_t) cur_inode->i_atime;

  struct tm* c_time = gmtime(&c_time_sec);
  int c_hour = c_time->tm_hour;            // 8: time
  int c_min = c_time->tm_min;
  int c_sec = c_time->tm_sec;
  struct tm* m_time = gmtime(&m_time_sec);
  int m_hour = m_time->tm_hour;            // 9: time
  int m_min = m_time->tm_min;
  int m_sec = m_time->tm_sec;
  struct tm* a_time = gmtime(&a_time_sec);
  int a_hour = a_time->tm_hour;            // 10: time
  int a_min = a_time->tm_min;
  int a_sec = a_time->tm_sec;

  //      0     0  0  0   0  0  0  0                              0                            1                             1  1
  //      1     2  3  4   5  6  7  8                              9                            0                             1  2
  printf("INODE,%d,%c,%o,%d,%d,%d,%02d/%02d/%02d %02d:%02d:%02d,%02d/%02d/%02d %02d:%02d:%02d,%02d/%02d/%02d %02d:%02d:%02d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
    inode_number,               // 2
    file_type,                  // 3
    cur_inode->i_mode & 0777,   // 4
    cur_inode->i_uid,           // 5
    cur_inode->i_gid,           // 6
    cur_inode->i_links_count,   // 7
    c_time->tm_mon + 1,         // 8: date
    c_time->tm_mday,
    c_time->tm_year % 100,
    c_hour,            // 8: time
    c_min,
    c_sec,
    m_time->tm_mon + 1,         // 9: date
    m_time->tm_mday,
    m_time->tm_year % 100,
    m_hour,            // 9: time
    m_min,
    m_sec,
    a_time->tm_mon + 1,         // 10: date
    a_time->tm_mday,
    a_time->tm_year % 100,
    a_hour,            // 10: time
    a_min,
    a_sec,
    cur_inode->i_size,          // 11: file size
    cur_inode->i_blocks,        // 12:
    cur_inode->i_block[0],      // 13-27: block ids
    cur_inode->i_block[1],
    cur_inode->i_block[2],
    cur_inode->i_block[3],
    cur_inode->i_block[4],
    cur_inode->i_block[5],
    cur_inode->i_block[6],
    cur_inode->i_block[7],
    cur_inode->i_block[8],
    cur_inode->i_block[9],
    cur_inode->i_block[10],
    cur_inode->i_block[11],
    cur_inode->i_block[12],
    cur_inode->i_block[13],
    cur_inode->i_block[14]
  );

}
