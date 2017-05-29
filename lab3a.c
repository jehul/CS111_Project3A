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
void print_inode();
void print_directories();
void print_ib_references();
/////////////////////////////////////////

///GLOBALS///
struct ext2_super_block* superblock;
struct ext2_group_desc* group;
struct ext2_inode* inode;
int n_block_groups, n_inodes;
int fild, blocksize, inodesize;

///DRIVER///
int main(int argc, char * argv[])
{
  int n_groups = 0;
  if(argc != 2) { print_usage(); exit(1); }

  fild = open(argv[1], O_RDONLY);
  if( fild < 0 )
    {
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

  //I-node summary
  print_inode();

  //Directory entries (incomplete)
  print_directories();

  // Indirect block references
  print_ib_references();

  exit(0); //exit at success
}

///////////////
///UTILITIES///
///////////////

void print_error()
{
  fprintf(stderr, "%s\n", strerror(errno));
}

void print_usage()
{
  fprintf(stderr, "%s\n", "Usage: ./lab3a [image.img]");
}

void print_superblock()
{
  fprintf(stdout, "SUPERBLOCK,%d,%d,%d,%d,%d,%d,%d\n",
	  superblock->s_blocks_count,
	  superblock->s_inodes_count,
	  (1024 << superblock->s_log_block_size),
	  superblock->s_inode_size,
	  superblock->s_blocks_per_group,
	  superblock->s_inodes_per_group,
	  superblock->s_first_ino);
}

void print_group()
{
  int n_blocks_in_group;

  for (int k = 0; k < n_block_groups; k++) {
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

void print_freeblock()
{
  //for every block group, print BFREE if bit == 0
  int k = 0, i = 0, j = 0;
  int bit = 0;

  __u8 buf;

  for (k = 0 ; k < n_block_groups; k++)
    {
      for (i = 0; i < blocksize; i++)
	{
	  pread(fild, &buf, 1, (group[k].bg_block_bitmap * blocksize) + i);
	  for (j = 0; j < 8; j++)
	    {
	      bit = buf & (1 << j);

	      if (bit == 0)
		fprintf(stdout, "BFREE,%d\n", (k*n_block_groups) + (i*8) + j + 1);
	      //check this
	    }
	}
    }

}

void print_freeinode()
{
  int k = 0, i = 0, j = 0;
  int bit = 0;
  __u8 buf;

  for (k = 0 ; k < n_block_groups; k++)
    {
      for (i = 0; i < blocksize; i++)
	{
	  pread(fild, &buf, 1, (group[k].bg_inode_bitmap * blocksize) + i);
	  for (j = 0; j < 8; j++)
	    {
	      bit = buf & (1 << j);

	      if (bit == 0)
		fprintf(stdout, "IFREE,%d\n", (k*n_inodes) + (i*8) + j + 1);
	      //check this
	    }
	}
    }

}

void print_inode_printer(struct ext2_inode* cur_inode, int inode_number) {
  char file_type;

  if ((cur_inode->i_mode & 0x8000) == 0x8000)       { file_type = 'f'; }
  else if ((cur_inode->i_mode & 0xA000) == 0xA000)  { file_type = 's'; }
  else if ((cur_inode->i_mode & 0x4000) == 0x4000)  { file_type = 'd'; }
  else                                              { file_type = '?'; }

  const time_t c_time_sec = (const time_t) cur_inode->i_ctime;
  const time_t m_time_sec = (const time_t) cur_inode->i_mtime;
  const time_t a_time_sec = (const time_t) cur_inode->i_atime;

  struct tm* c_time = localtime(&c_time_sec);
  struct tm* m_time = localtime(&m_time_sec);
  struct tm* a_time = localtime(&a_time_sec);

  //      0     0  0  0   0  0  0  0                              0                            1                             1  1
  //      1     2  3  4   5  6  7  8                              9                            0                             1  2
  printf("INODE,%d,%c,%o,%d,%d,%d,%02d/%02d/%02d %02d:%02d:%02d,%02d/%02d/%02d %02d:%02d:%02d,%02d/%02d/%02d %02d:%02d:%02d,%d,%d\n",
    inode_number,               // 2
    file_type,                  // 3
    cur_inode->i_mode & 0777,   // 4
    cur_inode->i_uid,           // 5
    cur_inode->i_gid,           // 6
    cur_inode->i_links_count,   // 7
    c_time->tm_mon + 1,         // 8: date
    c_time->tm_mday,
    c_time->tm_year % 100,
    c_time->tm_hour,            // 8: time
    c_time->tm_min,
    c_time->tm_sec,
    m_time->tm_mon + 1,         // 9: date
    m_time->tm_mday,
    m_time->tm_year % 100,
    m_time->tm_hour,            // 9: time
    m_time->tm_min,
    m_time->tm_sec,
    a_time->tm_mon + 1,         // 10: date
    a_time->tm_mday,
    a_time->tm_year % 100,
    a_time->tm_hour,            // 10: time
    a_time->tm_min,
    a_time->tm_sec,
    cur_inode->i_fsize,         // 11
    cur_inode->i_blocks         // 12
  );
}

void print_inode() {
  struct ext2_inode* cur_inode = malloc(sizeof(struct ext2_inode));

  // For each block group, group[i]
  for (int i = 0; i < n_block_groups; i++) {
    // For each inode in the table, table[j]
    for (int j = 0; j < superblock->s_inodes_per_group; j++) {
      // Calculate offsets for inode bitmap and inode table
      __u32 bitmapOffset = (group[i].bg_inode_bitmap * blocksize) + j;
      __u32 tableOffset = (group[i].bg_inode_table * blocksize) + (j * inodesize);

      // Verify via bitmap that current inode is allocated
      __u8 inode_allocated;
      pread(fild, &inode_allocated, 1, bitmapOffset);
      inode_allocated = (inode_allocated >> 7) & 1;
      if (!inode_allocated) continue;

      // Read in current inode and print CSV for it
      pread(fild, cur_inode, inodesize, tableOffset);
      if (cur_inode->i_mode != 0 && cur_inode->i_links_count != 0) {
        // i + j is block number + inode offset within block
        // add 1 because inode numbers start at 1
        print_inode_printer(cur_inode, i + j + 1);
      }
    } // end: for(j)
  } // end: for(i)

  free (cur_inode);
}

void print_directories() {

}

void print_ib_references() {

}

//waddup Bibek
//wassgood Jehu, how's it going
