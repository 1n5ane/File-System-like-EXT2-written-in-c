#include <stdio.h>
#include <sys/mman.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include "super_blocks.h"
#include "../io/io.h"
#include "../inode/inode.h"

extern Fd_table open_fds;
extern int MDB;
extern int MFNS;
extern int MAX_INODES;
extern const int GROUP_BLOCK_SZ;

int compute_inodes_in_block_group(int DATA_BLOCK_SIZE,int GROUP_BLOCK_SIZE,int INODE_SIZE){
  return (GROUP_BLOCK_SIZE-sizeof(Group_superblock))/(INODE_SIZE+DATA_BLOCK_SIZE);//ipotheto oti mia kalh periptwsh einai
                                                      //ena inode kai ena data block
}

void print_superblock(const Superblock b){
  printf("CURRENT GROUP BLOCKS: %d\n",b.GROUP_BLOCKS);
  printf("GROUP BLOCK SIZE: %d bytes\n",b.GROUP_BLOCK_SIZE);
  printf("DATA BLOCK SIZE: %d bytes\n",b.DATA_BLOCK_SIZE);
  printf("INODE SIZE: %d bytes\n",b.INODE_SIZE);
  printf("MAX DATA BLOCKS PER FILE: %d blocks (%d bytes)\n",b.MAX_DATA_BLOCKS,b.MAX_DATA_BLOCKS*b.DATA_BLOCK_SIZE);
  printf("MAX FILENAME SIZE: %d bytes\n",b.FILENAME_SIZE);
  printf("MAX FILES IN DATA BLOCK: %d\n",b.MAX_FILES_IN_DATA_BLOCK);
  printf("max inodes in a block group: %d\n",b.inodes_in_block_group);
  return ;
}
void print_gsuperblock(const Group_superblock gb){
  printf("GROUP BLOCK ID: %d\n",gb.group_block_id);
  printf("INODES IN BLOCK: %d\n",gb.inode_sum);
  printf("DATA BLOCKS: %d\n",gb.data_sum);
  printf("DATA HOLES: %d bytes\n",gb.data_holes);
  printf("INODE HOLES: %d bytes\n",gb.inode_holes);
  printf("FREE DATA BLOCKS: %d\n",gb.free_data_blocks);

}

void group_sb_init(Superblock sb,Group_superblock * gb){
  gb->group_block_id=sb.GROUP_BLOCKS - 1;
  gb->inode_sum=0;
  gb->data_sum=0;
  gb->data_holes=0;
  gb->inode_holes=0;
  gb->free_data_blocks=(sb.GROUP_BLOCK_SIZE-sizeof(Group_superblock))/sb.DATA_BLOCK_SIZE;
  return ;
}
