#include <stdio.h>
#include "io.h"
#include "../inode/inode.h"
#include <unistd.h>
#include <string.h>
#include "../fs_operations/fs_operations.h"
#include <stdlib.h>

extern Fd_table open_fds;
extern int MDB;
extern int MFNS;
extern const int GROUP_BLOCK_SZ;
extern int MAX_INODES;
extern int DATA_BLOCK_SIZE;

void fd_table_init(void){
  for(int i=0;i<MAX_OPEN_FDS;i++)
    open_fds.table[i].offset=-1;//-1 indicates unused fd
}

void print_entry(int my_fd){
  printf("offset: %d\n",(int)open_fds.table[my_fd].offset);
  printf("data_block: %d\n",(int)open_fds.table[my_fd].cur_data_block);
  printf("inode id: %d\n",(int)open_fds.table[my_fd].inode_id);
  return ;
}

int get_Available_fd(void){//returns -1 if no fd available
  for(int i=0;i<MAX_OPEN_FDS;i++)
    if(open_fds.table[i].offset==-1)
      return i;
  return -1;
}

void insert_fd_entry(int index,long int iid,int cur_data_block){//insert inode id to open_fds.table[index]
  open_fds.table[index].offset=0;
  open_fds.table[index].cur_data_block=cur_data_block;//if file has no data blocks APPENDED
  open_fds.table[index].inode_id=iid;
  return ;
}

int my_open(int cfs_fd,Cursor * cursor,char * file_path){//for reading and writing no rights implemented...
  long int offset=lseek(cfs_fd,0,SEEK_CUR);
  int index=get_Available_fd();
  char file[300];
  strcpy(file,file_path);
  if(index==-1){
    lseek(cfs_fd,offset,SEEK_SET);
    return -1;//indicates failure-> ALL MAX_OPEN_FDS ARE USED
  }

  int inode_id;
  inode_id=find_path(cfs_fd,cursor,file);
  if(inode_id==-1){
    lseek(cfs_fd,offset,SEEK_SET);
    return -1;//indicates that file doesnt exist
  }
  Inode tmp;
  alloc_inode(&tmp);
  get_inode(cfs_fd,inode_id,&tmp);
  if(tmp.type==DIRECTORY){
    free_inode(tmp);
    lseek(cfs_fd,offset,SEEK_SET);
    return -1;
  }

  if(tmp.size==0)
    insert_fd_entry(index,inode_id,-1);
  else
    insert_fd_entry(index,inode_id,0);


  free_inode(tmp);
  lseek(cfs_fd,offset,SEEK_SET);
  return index;
}

void my_close(int my_fd){
  open_fds.table[my_fd].offset=-1;
  return ;
}

size_t my_read(int cfs_fd,int my_fd,void * buffer,int ssize){
  long int offset=lseek(cfs_fd,0,SEEK_CUR);
  int data_read=0;
  int new_offset=open_fds.table[my_fd].offset;
  int cur_data_block=open_fds.table[my_fd].cur_data_block;
  if(cur_data_block==-1){
    return 0;//nothing read because file is empty
  }
  Inode temp_inode;
  alloc_inode(&temp_inode);
  get_inode(cfs_fd,open_fds.table[my_fd].inode_id,&temp_inode);
  int sum_of_bytes_read=0;
  int bytes_in_block;

//printf("Offset begin: %d\n",new_offset);

  while(sum_of_bytes_read<ssize){
    move_fd_to_DataBlock(cfs_fd,temp_inode.data.block[cur_data_block]);
    read(cfs_fd,&bytes_in_block,sizeof(int));
    //printf("bytes %d in block %d\n",bytes_in_block,cur_data_block);

    if(bytes_in_block==0){
      open_fds.table[my_fd].offset=0;
      open_fds.table[my_fd].cur_data_block=cur_data_block;
      break;
    }
    if(sum_of_bytes_read==0)
      lseek(cfs_fd,new_offset,SEEK_CUR);
    if((bytes_in_block-new_offset<(ssize-sum_of_bytes_read)) && cur_data_block!=temp_inode.data.size-1){

      data_read=0;

      data_read+=read(cfs_fd,((uint8_t *)buffer+sum_of_bytes_read),bytes_in_block-new_offset);
      sum_of_bytes_read+=data_read;
      cur_data_block++;
      //printf("%d\n",cur_data_block);

      new_offset=0;//else offset 0 in next data block
    }else{

      if(ssize-sum_of_bytes_read>bytes_in_block-new_offset){
        ssize=bytes_in_block-new_offset;
      }else{
        ssize=ssize-sum_of_bytes_read;
      }
      //printf("%d\n",cur_data_block);

      data_read=read(cfs_fd,((uint8_t *)buffer+sum_of_bytes_read),ssize);
      new_offset+=data_read;
      sum_of_bytes_read+=data_read;
      open_fds.table[my_fd].offset=new_offset;
      open_fds.table[my_fd].cur_data_block=cur_data_block;
      break;
    }
  }

  //printf("Offset end: %d\n",new_offset);
  update_access_time(cfs_fd,&temp_inode);

  free_inode(temp_inode);
  lseek(cfs_fd,offset,SEEK_SET);
  return sum_of_bytes_read;
}

size_t my_write(int cfs_fd,int my_fd,void *buffer,int ssize){
  long int offset=lseek(cfs_fd,0,SEEK_CUR);
  int data_written=0;
  int new_offset=open_fds.table[my_fd].offset;
  int cur_data_block=open_fds.table[my_fd].cur_data_block;

  Inode temp_inode;
  alloc_inode(&temp_inode);
  get_inode(cfs_fd,open_fds.table[my_fd].inode_id,&temp_inode);
  //print_inode(temp_inode);

  if(cur_data_block==-1){
    //if no block appended just ask it
    reserve_DataBlock(cfs_fd,&temp_inode,1);
    cur_data_block=0;
  }

  int bytes_in_block;
  int sum_of_bytes_written=0;
  int size=0;
  //printf("Offset begin: %d\n",new_offset);
  while(sum_of_bytes_written<ssize){
    move_fd_to_DataBlock(cfs_fd,temp_inode.data.block[cur_data_block]);
    read(cfs_fd,&bytes_in_block,sizeof(int));

    lseek(cfs_fd,new_offset,SEEK_CUR);

    if(DATA_BLOCK_SIZE-sizeof(int)-new_offset<ssize-sum_of_bytes_written){
      data_written+=write(cfs_fd,((uint8_t *)buffer+sum_of_bytes_written),DATA_BLOCK_SIZE-sizeof(int)-new_offset);
      sum_of_bytes_written+=data_written;
      new_offset+=data_written;

      if(new_offset>bytes_in_block){
        //if file made larger
        move_fd_to_DataBlock(cfs_fd,temp_inode.data.block[cur_data_block]);
        bytes_in_block=new_offset;
        write(cfs_fd,&bytes_in_block,sizeof(int));
        lseek(cfs_fd,new_offset,SEEK_CUR);//go back to where you where
      }

      cur_data_block++;
      if(cur_data_block==temp_inode.data.size){
        //NEED A NEW BLOCK
        if(reserve_DataBlock(cfs_fd,&temp_inode,1)==-1)//means size of file exceeded
          break;
      }

      new_offset=0;//else offset 0 in next data block
      data_written=0;
    }else{
      data_written=write(cfs_fd,((uint8_t *)buffer+sum_of_bytes_written),ssize-sum_of_bytes_written);
      new_offset+=data_written;
      sum_of_bytes_written+=data_written;
      open_fds.table[my_fd].offset=new_offset;
      open_fds.table[my_fd].cur_data_block=cur_data_block;
      if(new_offset>bytes_in_block){
        //if file made larger
        move_fd_to_DataBlock(cfs_fd,temp_inode.data.block[cur_data_block]);
        bytes_in_block=new_offset;
        write(cfs_fd,&bytes_in_block,sizeof(int));
      }
    }
  }

  temp_inode.size=0;
  for(int i=0;i<temp_inode.data.size;i++){
    move_fd_to_DataBlock(cfs_fd,temp_inode.data.block[i]);
    read(cfs_fd,&bytes_in_block,sizeof(int));
    temp_inode.size+=bytes_in_block;
  }

  //printf("Offset end: %d\n",new_offset);


  move_fd_to_inode(cfs_fd,temp_inode.nodeid);
  temp_inode.access_time=time(NULL);
  temp_inode.modification_time=time(NULL);
  write_inode(cfs_fd,&temp_inode);

  free_inode(temp_inode);
  lseek(cfs_fd,offset,SEEK_SET);
  return sum_of_bytes_written;
}
