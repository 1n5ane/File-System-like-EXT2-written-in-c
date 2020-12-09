#ifndef __CURSOR_H__
#define __CURSOR_H__
enum path_type{ABSOLUTE,RELATIVE};
#include "../inode/inode.h"
#include "../super_blocks/super_blocks.h"
typedef struct Cursor{
  Group_superblock info;
  Inode node;//cfs fd always points to an inode indicated by node
  char path[300];//the path to there
}Cursor;
void print_cursor(Cursor * cursor);
int go_to_path(int cfs_fd,Cursor * cursor,char * path);//move cursor there for cd
int find_path(int cfs_fd,Cursor * cursor,char * path);//returns inode id of dir or file
enum path_type typeofpath(char * path);
void current_dir_rewind(int cfs_fd,Cursor * cursor);
void update_info(int cfs_fd,Cursor *cursor);
void cursor_alloc(Cursor * cursor);
void cursor_free(Cursor * cursor);
void cursor_copy(Cursor * dst,Cursor * src);


#endif
