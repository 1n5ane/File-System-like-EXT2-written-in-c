#ifndef __FS_OPERATIONS_H__
#define __FS_OPERATIONS_H__
#include "../inode/inode.h"
#include "../cursor/cursor.h"
#include "../super_blocks/super_blocks.h"
  enum ls_options{ALL_FILES,ONLY_DIRS,
    ONLY_LINKS,NON_RECURSIVE,DETAILED,NON_DETAILED,INORDER,NOORDER,SINGLE_FILE,RECURSIVE,NORMAL_FILES};
  enum cp_options{REC,NON_REC,ASK,DONTASK};
  int reserve_DataBlock(int cfs_fd,Inode * inode,int blocks);//this functs find avail data blocks and append it to Inode datastream
  long int get_Available_inode_id(int group_bl,int cfs_fd);
  long int seek_Available_inode_id(int group_bl,int cfs_fd);
  void insert_inode(int cfs_fd,Inode * inode);
  void write_inode(int cfs_fd,Inode * i);
  void move_fd_to_group_block(int cfs_fd,int i);
  void move_fd_to_inode(int cfs_fd,int id);
  void move_fd_to_DataBlock(int cfs_fd,Data_location there);
  int append_GroupBlock(int cfs_fd);
  Data_location occupy_data_hole(int cfs_fd,Inode * inode,int group_block);
  void get_Superblock(int cfs_fd,Superblock *sb);
  void get_inode(int cfs_fd,int id,Inode * node);
  void get_Group_superblock(int cfs_fd,int which,Group_superblock * gb);
  int my_mkdir(int cfs_fd,Cursor * cursor,char * dir_name);
  int cfs_touch(int cfs_fd,Cursor * cursor,char * file_name,char * options);
  int check_if_entity_exists(int cfs_fd,Inode * inode,char * entity_name,Data_location *exist_location);//checks inode if entity_name exists
  int find_Data_location_for_entity(int cfs_fd,Inode *inode,Data_location *free_location);//find free place for new entry in inode dir
  int insert_entry_in_DataBlock(int cfs_fd,Inode * inode,char* entity_name,int id,Data_location there);
  void update_access_time(int cfs_fd,Inode * inode);
  void update_modification_time(int cfs_fd,Inode * inode);
  void update_creation_time(int cfs_fd,Inode * inode);
  void cfs_ls(int cfs_fd,Cursor * cursor,
    char * file_name,enum ls_options which_files,enum ls_options how,
    enum ls_options detail,enum ls_options order,enum ls_options type);
  void dir_traverse(int cfs_fd,Inode *dir_to_traverse,int **inode_id,enum ls_options how,enum ls_options order);
  int get_entity_from_dir(int cfs_fd,int i,Inode *dir_to_traverse);
  int cfs_import(int cfs_fd,Cursor * cur_cursor,char * file_path,char * dir);
  int cfs_export(int cfs_fd,Cursor * cur_cursor,char * file_path,char * dir);
  int cfs_cat(int cfs_fd,Cursor * cur_cursor,char **files,int num_of_files);
  int free_DataBlock(int cfs_fd,Data_location x);
  int cfs_cp(int cfs_fd,Cursor * cur_cursor,char **files,int num_of_files,enum cp_options how,enum cp_options ask);
  int cfs_mv(int cfs_fd,Cursor * cur_cursor,char **files,int num_of_files,enum cp_options ask);
  void rm_entry(int cfs_fd,Inode * file_to_rm,Inode * parent_dir);
  int cfs_rm(int cfs_fd,Cursor * cur_cursor,char **files,int num_of_files,enum cp_options ask);
  void rm_inode(int cfs_fd,Inode * to_be_rm);//clears the spot inode is, returns num of entities deleted

#endif
