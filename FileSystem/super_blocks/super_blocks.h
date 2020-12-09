#ifndef __SUPER_BLOCKS_H__
#define __SUPER_BLOCKS_H__
#include <stdint.h>
typedef struct Inode Inode;
typedef struct super_block{
                    //IMPORTANT: DEDICATED INODE NUMBERS PER GROUP_BLOCK to make searching and organizing easier
  int GROUP_BLOCKS;
  int DATA_BLOCK_SIZE;
  int INODE_SIZE;
  int GROUP_BLOCK_SIZE;
  int MAX_DATA_BLOCKS;
  int MAX_FILES_IN_DATA_BLOCK;
  int FILENAME_SIZE;
  int inodes_in_block_group;// i assume that every inode has at least one data_block
}Superblock;              //this number depends on GROUP_BLOCK size and in DATA_BLOCK size (MAX INODES IN BLOCK GROUP)

typedef struct group_superblock{
  int group_block_id;
  int inode_sum;
  int data_sum;
  int data_holes;
  int inode_holes;
  int free_data_blocks;
}Group_superblock;

typedef struct Block{
  Group_superblock * info;
  Inode * inode;
  uint8_t * data_end;
}Block;

int compute_inodes_in_block_group(int DATA_BLOCK_SIZE,int GROUP_BLOCK_SIZE,int INODE_SIZE);
void print_superblock(const Superblock b);
void print_gsuperblock(const Group_superblock gb);
void group_sb_init(Superblock sb,Group_superblock * gb);

#endif
