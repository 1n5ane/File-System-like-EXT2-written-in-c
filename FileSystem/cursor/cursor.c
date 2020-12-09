#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cursor.h"
#include <unistd.h>
#include "../io/io.h"
#include "../fs_operations/fs_operations.h"

extern Fd_table open_fds;
extern int MDB;
extern int MFNS;
extern const int GROUP_BLOCK_SZ;
extern int MAX_INODES;
extern int DATA_BLOCK_SIZE;
extern int MAX_FILES_IN_DATA_BLOCK;

void print_cursor(Cursor * cursor){
  printf("~%s$ ",cursor->path);
  fflush(stdout);
  return ;
}

void cursor_alloc(Cursor * cursor){
  alloc_inode(&cursor->node);
  return ;
}

void cursor_free(Cursor * cursor){
  free_inode(cursor->node);
  return ;
}

void current_dir_rewind(int cfs_fd,Cursor * cursor){
  get_Group_superblock(cfs_fd,0,&cursor->info);
  get_inode(cfs_fd,0,&cursor->node);
  move_fd_to_inode(cfs_fd,0);
  strcpy(cursor->path,"/");
  return ;
}

void update_info(int cfs_fd,Cursor *cursor){
  get_Group_superblock(cfs_fd,cursor->info.group_block_id,&cursor->info);
  get_inode(cfs_fd,cursor->node.nodeid,&cursor->node);
  return ;
}

enum path_type typeofpath(char * path){
  if(path[0]=='/')
    return ABSOLUTE;
  return RELATIVE;
}

int count_files(char * path){
  int count=1;
  int len=strlen(path);
  for(int i=0;i<len;i++)
    if(path[i]=='/')
      count++;
  if(path[len-1]=='/')
    count--;
  return count;
}

int find_path(int cfs_fd,Cursor * cursor,char * path){
  long int offset=lseek(cfs_fd,0,SEEK_CUR);
  enum path_type ptype=typeofpath(path);
  char path_copy[300];
  strcpy(path_copy,path);
  int arr_size;//of names to search;
  char * path_ptr=path_copy;
  char** names_to_search;


  if(ptype==ABSOLUTE)
    path_ptr++;

  arr_size=count_files(path_ptr);
  names_to_search=malloc(arr_size*sizeof(char *));
  names_to_search[0]=strtok(path_ptr,"/");
  //printf("NAMES TO SEARCH: %d\n",arr_size);
  if(names_to_search[0]==NULL){//only one word
    names_to_search[0]=path_ptr;
  }else{
    for(int i=1;i<arr_size;i++)
      names_to_search[i]=strtok(NULL,"/");
    if(names_to_search[arr_size-1]==NULL)
      names_to_search[arr_size-1]=names_to_search[arr_size-2]+strlen(names_to_search[arr_size-2])+1;
    //for(int i=0;i<arr_size;i++)
      //printf("  --%s\n",names_to_search[i]);
  }

  Inode cur;
  alloc_inode(&cur);
  if(ptype==ABSOLUTE){
    get_inode(cfs_fd,0,&cur);
  }else{
    copy_inode(&cur,&cursor->node);
  }
    //print_inode(cur);
  int found;
  char *file_name=malloc(MFNS*sizeof(char));
  int id=-1;
  int entries_in_datablock;
  for(int k=0;k<arr_size;k++){
    found=0;
    //printf("-%s\n",names_to_search[k]);
    if(!strcmp(names_to_search[k],".")){
      id=cur.nodeid;
      continue;
    }else if(!strcmp(names_to_search[k],"..")){
      get_inode(cfs_fd,cur.parent_nodeid,&cur);

      id=cur.nodeid;
      continue;
    }else{
      for(int i=0;i<cur.data.size && !found ;i++){
        move_fd_to_DataBlock(cfs_fd,cur.data.block[i]);
        read(cfs_fd,&entries_in_datablock,sizeof(int));
        for(int j=0;j<entries_in_datablock && !found;j++){
          read(cfs_fd,file_name,MFNS);//read name
          read(cfs_fd,&id,sizeof(int));//and inode id
          if(!strcmp(file_name,names_to_search[k]))
            found=1;
        }
      }
      if(!found){
        free(file_name);
        free_inode(cur);
        free(names_to_search);
        return -1;
      }else{//search inside the other directory
        get_inode(cfs_fd,id,&cur);
        if((cur.type==REGULAR_FILE || cur.type==LINK) && k!=arr_size-1){
          printf("%s is not a directory!\n",cur.filename);
          free(file_name);
          free_inode(cur);
          free(names_to_search);
          return -1;
        }
      }
    }
  }

  char new_path[300];
  if(ptype==RELATIVE)
    strcpy(new_path,cursor->path);
  else
    strcpy(new_path,"/");

  for(int i=0;i<arr_size;i++){
    if(!strcmp(names_to_search[i],".")){
      continue;
    }else if(!strcmp(names_to_search[i],"..")){
      if(strlen(new_path)==1)
        continue;
      int j;
      for(j=strlen(new_path)-1;new_path[j]!='/';j--)
        ;
      if(j!=0)
        new_path[j]='\0';
      else
        new_path[j+1]='\0';

    }else{
      int len=strlen(new_path);
      if(len!=1){
        new_path[len]='/';
        new_path[len+1]='\0';
      }
      strcat(new_path,names_to_search[i]);
    }
  }
  strcpy(path,new_path);
  free(file_name);
  free_inode(cur);
  free(names_to_search);
  lseek(cfs_fd,offset,SEEK_SET);
  return id;
}

void cursor_copy(Cursor * dst,Cursor * src){
  dst->info=src->info;
  copy_inode(&dst->node,&src->node);
  strcpy(dst->path,src->path);
  return ;
}


int go_to_path(int cfs_fd,Cursor * cursor,char * path){//move cursor there for cd
  long int id=find_path(cfs_fd,cursor,path);
  enum path_type ptype=typeofpath(path);
  if(id==-1)
    return -1;
  Inode inode;
  alloc_inode(&inode);
  get_inode(cfs_fd,id,&inode);

  if(inode.type!=DIRECTORY){//inode type must be a directory to go there
    printf("%s not a directory\n",inode.filename);
    free_inode(inode);
    return -1;
  }

  inode.access_time=time(NULL);
  move_fd_to_inode(cfs_fd,inode.nodeid);
  write_inode(cfs_fd,&inode);
  move_fd_to_inode(cfs_fd,inode.nodeid);
  copy_inode(&cursor->node,&inode);
  get_Group_superblock(cfs_fd,id/MAX_INODES,&cursor->info);
  strcpy(cursor->path,path);
  free_inode(inode);
  return 0;
}
