#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include "../fs_operations/fs_operations.h"
#include "inode.h"
#include "../super_blocks/super_blocks.h"
#include "../io/io.h"

extern Fd_table open_fds;
extern int MDB;
extern int MFNS;
extern const int GROUP_BLOCK_SZ;
extern int MAX_INODES;
extern int DATA_BLOCK_SIZE;


int sizeofInode(void){
  int size=3*sizeof(unsigned int) +sizeof(enum f_type) +3*sizeof(time_t) +MFNS;
  size+=sizeof(int) + MDB*sizeof(Data_location);//for Datastream struct
  return size;
}

void init_inode(int id,char * name,int parent,enum f_type tp,Inode * i){
  i->nodeid=id;
  strcpy(i->filename,name);
  i->size=0;
  i->type=tp;
  i->parent_nodeid=parent;
  i->data.size=0;
  i->creation_time=time(NULL);
  i->access_time=time(NULL);
  i->modification_time=time(NULL);
  return ;
}

void free_inode(Inode i){
  free(i.filename);
  free(i.data.block);
}

void copy_inode(Inode *dest,Inode *src){
  dest->nodeid=src->nodeid;
  strcpy(dest->filename,src->filename);
  dest->size=src->size;
  dest->type=src->type;
  dest->parent_nodeid=src->parent_nodeid;
  dest->data.size=src->data.size;
  dest->creation_time=src->creation_time;
  dest->access_time=src->access_time;
  dest->modification_time=src->modification_time;
  for(int i=0;i<dest->data.size;i++)
    dest->data.block[i]=src->data.block[i];
  return ;
}

void print_inode(const Inode inode){
  char f0[]="REGULAR_FILE";
  char f1[]="DIRECTORY";
  char f2[]="LINK";
  char * answer;
  if(inode.type==REGULAR_FILE)
    answer=f0;
  else if(inode.type==DIRECTORY)
    answer=f1;
  else
    answer=f2;

  printf("Inode num: %d\n",inode.nodeid);
  printf("File name: %s\n",inode.filename);
  if(inode.type!=DIRECTORY)
    printf("Size: %d\n",inode.size);
  else{
    long int files_in_dir=0;
    if(inode.size!=0)
      files_in_dir=(inode.size-sizeof(int))/(sizeof(int)+MFNS);
    printf("Size: %d bytes (contains %lu files/dir)\n",inode.size,files_in_dir);
  }
  printf("Type: %s\n",answer);
  printf("Parent node id: %d\n",inode.parent_nodeid);
  printf("Creation time: %s",ctime(&inode.creation_time));
  printf("Access_time time: %s",ctime(&inode.access_time));
  printf("Modification_time time: %s",ctime(&inode.modification_time));
  printf("Data (group,block): ");

  for(int i=0;i<inode.data.size;i++)
    printf("(%d,%d) ",inode.data.block[i].block_group,inode.data.block[i].block);
  printf("\n");

  return ;
}

void alloc_inode(Inode * iptr){
  iptr->filename=malloc(MFNS*sizeof(char));
  iptr->data.block=malloc(MDB*sizeof(Data_location));
}
