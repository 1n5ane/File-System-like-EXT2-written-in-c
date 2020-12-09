#ifndef __IO_H__
#define __IO_H__
#define MAX_OPEN_FDS 100
#include <stdio.h>
#include "../cursor/cursor.h"
typedef struct fd_entry{
  long int offset;//offset in data block;
  int cur_data_block;//which data block
  long int inode_id;//which file/dir
}Fd_entry;

typedef struct fd_table{
  Fd_entry table[MAX_OPEN_FDS];
}Fd_table;

//my_open return index to Fd_entry table for unuses descriptor
int my_open(int cfs_fd,Cursor * cursor,char * file_path);//for reading and writing no rights implemented...
size_t my_read(int cfs_fd,int my_fd,void * buffer,int ssize);
size_t my_write(int cfs_fd,int my_fd,void *buffer,int ssize);
void print_entry(int my_fd);
void my_close(int my_fd);
void fd_table_init(void);
void insert_fd_entry(int index,long int iid,int cur_data_block);//insert inode id to open_fds.table[index]
int get_Available_fd(void);
void close_all(void);

#endif
