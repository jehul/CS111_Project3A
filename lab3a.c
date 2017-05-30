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
void print_inode_printer(struct ext2_inode* cur_inode, int inode_number);
void print_inode();
void print_directories(struct ext2_inode* in, int inode_number);
void print_ib_references(int inodenumber, int blockid, int count);
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

  //I-node summary -> Implicit call to directory entries and indirect block references
  print_inode();

    
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

void print_inode_printer(struct ext2_inode* cur_inode, int inode_number) {
  char file_type;

  if ((cur_inode->i_mode & 0x8000) == 0x8000)       { file_type = 'f'; }
  else if ((cur_inode->i_mode & 0xA000) == 0xA000)  { file_type = 's'; }
  else if ((cur_inode->i_mode & 0x4000) == 0x4000)  { file_type = 'd'; }
  else                                              { file_type = '?'; }

  const time_t c_time_sec = (const time_t) cur_inode->i_ctime;
  const time_t m_time_sec = (const time_t) cur_inode->i_mtime;
  const time_t a_time_sec = (const time_t) cur_inode->i_atime;

  int c_hour,c_min,c_sec,m_hour,m_min,m_sec,a_hour,a_min,a_sec;

  struct tm* c_time = gmtime(&c_time_sec);
  c_hour = c_time->tm_hour;            // 8: time
  c_min = c_time->tm_min;
  c_sec = c_time->tm_sec;
  struct tm* m_time = gmtime(&m_time_sec);
  m_hour = m_time->tm_hour;            // 9: time
  m_min = m_time->tm_min;
  m_sec = m_time->tm_sec;
  struct tm* a_time = gmtime(&a_time_sec);
  a_hour = a_time->tm_hour,            // 10: time
  a_min = a_time->tm_min,
  a_sec = a_time->tm_sec,
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
  cur_inode->i_block[14]);       

  if (file_type == 'd') { print_directories(cur_inode, inode_number); }
  
  //Looks for Indirect Block References
  int k = 0; 
  for (k = 12; k < 14; k++){
    if(cur_inode->i_block[k] != 0)   
      print_ib_references(inode_number, cur_inode->i_block[k], k + 1);
  }
}

void print_inode() {
  struct ext2_inode* cur_inode = malloc(sizeof(struct ext2_inode));
  int i,j;
  // For each block group, group[i]
  for (i = 0; i < n_block_groups; i++) {
    // For each inode in the table, table[j]
    for (j = 0; j < superblock->s_inodes_per_group; j++) {
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

void print_directories(struct ext2_inode* in, int inode_number) {
  struct ext2_dir_entry d_entry;
  int i = 0;
  int entry_len = 0;
  // For each block in the i_block array
  for ( i = 0; i < 15; i++) {
    int cur_block = in->i_block[i];
    if (cur_block == 0) break;  // value of 0 is the array terminator

    // Current block is a direct block
    if(i + 1 <= 12)  {
      int entries_base = (cur_block * blocksize);
      int entry_offset = 0;
      while(1) {
        pread(fild, &d_entry, sizeof(struct ext2_dir_entry), entries_base + entry_offset);

        // Reached end of entries for this directory or hit empty inode
        if (entry_offset >= 1024 || d_entry.inode == 0) break;

        entry_len = d_entry.rec_len;
        
        fprintf(stdout,"DIRENT,%d,%d,%d,%d,%d,'%s'\n",
          inode_number,
          entry_offset,
          d_entry.inode,
          entry_len,
          d_entry.name_len,
          d_entry.name
        );

        entry_offset += d_entry.rec_len;
      }

    }

     
  }
}

void print_ib_references(int inodenumber, int blockid, int count) {
  int buf, buf2, buf3;
  
  int k, j, i;
  int level_of_indirection;
  int file_offset; //may have to correct this
  
  for (k = 0; k < (blocksize/sizeof(int)); k++)
    {
      level_of_indirection = 1;//START 1st level of indirection
      pread(fild, &buf, 4, (blockid * blocksize) + (k * 4));
      if (buf != 0)
	{
	  file_offset = 12 + k;
	  fprintf(stdout, "INDIRECT,%d,%d,%d,%d,%d\n",
		  inodenumber,
		  level_of_indirection,
		  file_offset,
		  blockid,
		  buf);
	}

      if (count == 13) continue; //END 1st level of Indirection: DO NOT GO FURTHER

      for (j = 0; j < (blocksize/sizeof(int)); j++)
	{
	  level_of_indirection = 2;//START 2nd level of indirection
	  pread(fild, &buf2, 4, (buf * blocksize) + (j * 4));
	  if (buf2 != 0)
	    {
	      file_offset = 12 + j;
	      fprintf(stdout, "INDIRECT,%d,%d,%d,%d,%d\n",
		      inodenumber,
		      level_of_indirection,
		      file_offset,
		      buf,
		      buf2);
	    }

	  if (count == 14) continue; //END 2nd level of indirection: DO NOT GO FURTHER
	  for (i = 0; i < (blocksize/sizeof(int)); i++)
	    {
	      level_of_indirection = 3;//START 3rd level of indirection
	      pread(fild, &buf3, 4, (buf2 * blocksize) + (i * 4));
	      if (buf2 != 0)
		{
		  file_offset = 12 + i;
		  fprintf(stdout, "INDIRECT,%d,%d,%d,%d,%d\n",
			  inodenumber,
			  level_of_indirection,
			  file_offset,
			  buf2,
			  buf3);
		}
	    }//for loop inner	  
	}//for loop middle
    }//for loop outer
 
}

//waddup Bibek
//wassgood Jehu, how's it going
