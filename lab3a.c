// NAME: Bibek Ghimire Jehu Lee
// EMAIL: bibekg@ucla.edu jehulee97@g.ucla.edu
// ID: 004569045 404646481

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>
#include "ext2_fs.h"

/////////////////////////////
/// FUNCTION DECLARATIONS ///
/////////////////////////////

void print_error();
void print_usage();
void print_superblock();
void print_group();
void print_free_items();

void process_all_inodes();
void process_data_block(const int curr_block, const int ref_block, const int file_offset, const int inode_number, const int max_ind_level, const int curr_ind_level, const bool is_dir, const bool was_referenced);
void print_directories(const int inode_number, const int entries_base, const int file_offset);
void print_indirect(const int inode_number, const int ind_level, const int file_offset, const int curr_block, const int ref_block);
void print_inode(int inode_number, struct ext2_inode* cur_inode, char file_type);

///////////////////
/// GLOBAL VARS ///
///////////////////

struct ext2_super_block* superblock;
struct ext2_group_desc* group;
int num_groups, num_inodes, fild, blocksize, inode_size, PPB;
int exit_code = 0;

///////////////////
/// MAIN DRIVER ///
///////////////////

int main(int argc, char * argv[]) {
  if(argc != 2) { print_usage(); exit(1); }

  fild = open(argv[1], O_RDONLY);
  if (fild < 0) { print_error(); exit(1); }

  superblock = malloc(sizeof(struct ext2_super_block));
  pread(fild, superblock, EXT2_MIN_BLOCK_SIZE, 1024);

  num_groups = 1 + ((superblock->s_blocks_count - 1) / superblock->s_blocks_per_group);
  num_inodes = 1 + ((superblock->s_inodes_count - 1) / superblock->s_inodes_per_group);
  blocksize = 1024 << superblock->s_log_block_size;
  PPB = blocksize / sizeof(int);
  inode_size = superblock->s_inode_size;

  group = malloc(sizeof(struct ext2_group_desc) * num_groups);
  pread(fild, group, EXT2_MIN_BLOCK_SIZE, 1024 + EXT2_MIN_BLOCK_SIZE);

  print_superblock();
  print_group();
  print_free_items();
  process_all_inodes();

  exit(exit_code);
}

/////////////////
/// UTILITIES ///
/////////////////

void print_error() {
  fprintf(stderr, "%s\n", strerror(errno));
}

void print_usage() {
  fprintf(stderr, "%s\n", "Usage: ./lab3a [image.img]");
}

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

void print_group() {
  int n_blocks_in_group, k;

  for (k = 0; k < num_groups; k++) {
    // Not enough blocks to fill up a group
    if (superblock->s_blocks_count < superblock->s_blocks_per_group) {
      n_blocks_in_group = superblock->s_blocks_count;
    }
    // If processing last group, may be incomplete
    else if ( k == num_groups -1) {
      n_blocks_in_group = superblock->s_blocks_count - (superblock->s_blocks_per_group * num_groups);
    }
    // Default case
    else {
      n_blocks_in_group = superblock->s_blocks_per_group;
    }

    fprintf(stdout, "GROUP,%d,%d,%d,%d,%d,%d,%d,%d\n",
      k,
      n_blocks_in_group,
      superblock->s_inodes_per_group,
      group[k].bg_free_blocks_count,
      group[k].bg_free_inodes_count,
      group[k].bg_block_bitmap,
      group[k].bg_inode_bitmap,
      group[k].bg_inode_table
    );
  }
}

void print_free_items() {
  int i,j,k;
  __u8 block_buf, inode_buf;

  for (i = 0 ; i < num_groups; i++) {
    for (j = 0; j < blocksize; j++) {
      pread(fild, &block_buf, 1, (group[i].bg_block_bitmap * blocksize) + j);
      pread(fild, &inode_buf, 1, (group[i].bg_inode_bitmap * blocksize) + j);
      for (k = 0; k < 8; k++) {
        if ((block_buf & (1 << k)) == 0) {
          fprintf(stdout, "BFREE,%d\n", (i * num_groups) + (j * 8) + k + 1);
        }
        if ((inode_buf & (1 << k)) == 0) {
          fprintf(stdout, "IFREE,%d\n", (i * num_inodes) + (j * 8) + k + 1);
        }
      }
    }
  }
}

void process_all_inodes() {
  struct ext2_inode cur_inode;
  int i,j,k, file_type, curr_block, file_offset;

  // For every in the filesystem
  for (i = 0; i < num_groups; i++) {
    for (j = 0; j < superblock->s_inodes_per_group; j++) {

      __u32 tableOffset = (group[i].bg_inode_table * blocksize) + (j * inode_size);
      pread(fild, &cur_inode, inode_size, tableOffset);

      if (cur_inode.i_mode != 0 && cur_inode.i_links_count != 0) {
        int inode_number = i + j + 1;

        // Determine file type
        if      ((cur_inode.i_mode & 0x8000) == 0x8000)  { file_type = 'f'; }
        else if ((cur_inode.i_mode & 0xA000) == 0xA000)  { file_type = 's'; }
        else if ((cur_inode.i_mode & 0x4000) == 0x4000)  { file_type = 'd'; }
        else                                             { file_type = '?'; }

        // Print inode summary CSV
        print_inode(inode_number, &cur_inode, file_type);

        // Process direct blocks
        for (k = 0; k < 12; k++) {
          curr_block = cur_inode.i_block[k];
          file_offset = k;
          process_data_block(curr_block, 0, file_offset, inode_number, 0, 0, (file_type == 'd'), false);
        }

        // Process indirect blocks
        for (k = 0; k < 3; k++) {
          curr_block = cur_inode.i_block[k + 12];
          switch (k + 1) {
            case 1: file_offset = 12; break;
            case 2: file_offset = 12 + PPB; break;
            case 3: file_offset = 12 + PPB + (PPB * PPB); break;
            default: break;
          }
          if (curr_block != 0) {
            process_data_block(curr_block, 0, file_offset, inode_number, k + 1, 0, (file_type == 'd'), false);
          }
        }
      }
    }
  }
}

void process_data_block(const int curr_block, const int ref_block, const int file_offset, const int inode_number, const int max_ind_level, const int curr_ind_level, const bool is_dir, const bool was_referenced) {
  int i, next_block;

  // Print indirect reference CSV
  if (was_referenced && curr_block != 0) {
    print_indirect(inode_number, curr_ind_level, file_offset, curr_block, ref_block);
  }

  // curr_block is a data block
  if (curr_ind_level == max_ind_level) {
    if (curr_block != 0 && is_dir) {
      // If the inode is a directory, print a DIRENT CSV for the curr_block
      print_directories(inode_number, curr_block * blocksize, file_offset);
    }
  }

  // curr_block is an indirect block
  else {
    // Read and process each block that is indirectly pointed to
    for (i = 0; i < PPB; i++) {
      pread(fild, &next_block, 4, (curr_block * blocksize) + (i * sizeof(int)));
      if (next_block != 0) {
        process_data_block(next_block, curr_block, file_offset + i, inode_number, max_ind_level, curr_ind_level + 1, is_dir, true);
      }
    }
  }
}

void print_directories(const int inode_number, const int entries_base, const int file_offset) {
  struct ext2_dir_entry d_entry;
  int entry_offset = 0;

  while(1) {
    pread(fild, &d_entry, sizeof(struct ext2_dir_entry), entries_base + entry_offset);

    // Reached end of entries for this directory or hit empty inode
    if (entry_offset >= blocksize) break;

    if (d_entry.inode != 0) {
      fprintf(stdout,"DIRENT,%d,%d,%d,%d,%d,'%s'\n",
        inode_number,
        file_offset + entry_offset,
        d_entry.inode,
        d_entry.rec_len,
        d_entry.name_len,
        d_entry.name
      );
    }

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
  int c_hour = c_time->tm_hour;
  int c_min = c_time->tm_min;
  int c_sec = c_time->tm_sec;
  struct tm* m_time = gmtime(&m_time_sec);
  int m_hour = m_time->tm_hour;
  int m_min = m_time->tm_min;
  int m_sec = m_time->tm_sec;
  struct tm* a_time = gmtime(&a_time_sec);
  int a_hour = a_time->tm_hour;
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
    c_hour,                     // 8: time
    c_min,
    c_sec,
    m_time->tm_mon + 1,         // 9: date
    m_time->tm_mday,
    m_time->tm_year % 100,
    m_hour,                     // 9: time
    m_min,
    m_sec,
    a_time->tm_mon + 1,         // 10: date
    a_time->tm_mday,
    a_time->tm_year % 100,
    a_hour,                     // 10: time
    a_min,
    a_sec,
    cur_inode->i_size,          // 11: file size
    cur_inode->i_blocks,        // 12: number of blocks
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
