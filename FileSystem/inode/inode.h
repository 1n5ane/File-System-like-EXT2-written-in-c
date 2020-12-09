#ifndef __INODE_H__
#define __INODE_H__
#include <time.h>
#include <stdint.h>
enum f_type{REGULAR_FILE,DIRECTORY,LINK};

typedef struct data_location{
  int block_group;//which group;
  int block;//which block from the end
}Data_location;

typedef struct Datastream{
  int size;
  Data_location * block;
}Datastream;

typedef struct Inode{
  unsigned  int  nodeid;
  char *filename;
  unsigned  int  size;
  enum f_type type;
  unsigned  int  parent_nodeid;
  time_t  creation_time;
  time_t  access_time;
  time_t  modification_time;
  Datastream data;
}Inode;

int sizeofInode(void);
void init_inode(int id,char * name,int parent,enum f_type tp,Inode * i);
void free_inode(Inode i);
void alloc_inode(Inode * iptr);
void copy_inode(Inode *dest,Inode *src);
void print_inode(const Inode inode);
#endif
