//NAME: Bibek Ghimire Jehu Lee
//EMAIL: bibekg@ucla.edu jehulee97@g.ucla.edu
//ID: 004569045 404646481

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <linux/types.h>
#include <fcntl.h>
#include <unistd.h>


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

  // Calculate some sizes
  n_block_groups = 1 + ((superblock->s_blocks_count - 1) / superblock->s_blocks_per_group);
  n_inodes = 1 + ((superblock->s_inodes_count - 1) / superblock->s_inodes_per_group);
  blocksize = 1024 << superblock->s_log_block_size;
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
      else if ( k == n_block_groups -1) {
    	  n_blocks_in_group = superblock->s_blocks_count - (superblock->s_blocks_per_group * n_block_groups);
      }
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

//UNDER CONSTRUCTION
void print_inode() //must multigroup
{
  /*int k = 0;
  inode = malloc(sizeof(ext2_inode));
  for ( k = 0; k < superblock->s_inodes_per_group; k++)
    {
      pread(fild, inode, inodesize,
	    (group->bg_inode_table * blocksize) + (inodesize * k));

    }
  */

  int k = 0, i = 0, j = 0, count = 1;
  int bit = 0, inodenumber = 0;
  __u8 buf;

  for (k = 0 ; k < n_block_groups; k++)
    {
      for (i = 0; i < blocksize; i++)
	{
	  pread(fild, &buf, 1, (group[k].bg_inode_bitmap * blocksize) + i);
	  for (j = 0; j < 8; j++)
	    {
	      bit = buf & (1 << j);

	      if (bit == 1 && count <= superblock->s_inodes_per_group)
		{
		  inodenumber = count;
		  fprintf(stdout, "%d\n", inodenumber);
		}
	      count++;
	    }
	}
    }
}

void print_directories() {

}

void print_ib_references() {

}

//waddup Bibek
//wassgood Jehu, how's it going
