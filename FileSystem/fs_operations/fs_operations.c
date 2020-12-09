#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <math.h>
#include <dirent.h>
#include <math.h>
#include "fs_operations.h"
#include "../inode/inode.h"
#include "../io/io.h"
#include "../sort.h"
#include "../super_blocks/super_blocks.h"

extern Fd_table open_fds;


extern int MDB;
extern int MFNS;
extern const int GROUP_BLOCK_SZ;
extern int MAX_INODES;
extern int DATA_BLOCK_SIZE;
extern int MAX_FILES_IN_DATA_BLOCK;


void move_fd_to_group_block(int cfs_fd,int i){
  lseek(cfs_fd,sizeof(Superblock)+i*GROUP_BLOCK_SZ,SEEK_SET);//go to the correct block group
  return ;
}
void move_fd_to_inode(int cfs_fd,int id){
  move_fd_to_group_block(cfs_fd,id/MAX_INODES);
  lseek(cfs_fd,sizeof(Group_superblock) +(id%MAX_INODES)*sizeofInode(),SEEK_CUR);
  return ;
}

void insert_inode(int cfs_fd,Inode * inode){//go to the right place and put it
//GO TO THE CORRECT PLACE TO PUT IT
  long int curr_offset=lseek(cfs_fd,0,SEEK_CUR);
  move_fd_to_group_block(cfs_fd,inode->nodeid/MAX_INODES);
  Group_superblock gb;
  read(cfs_fd,&gb,sizeof(Group_superblock));
  long int space;
  space=GROUP_BLOCK_SZ-sizeof(Group_superblock)-gb.inode_sum*sizeofInode()-
  gb.data_sum*DATA_BLOCK_SIZE;

  if((int)inode->nodeid<gb.inode_sum-1){
    gb.inode_holes--;//the id assigned to inode fills a hole
  }else{
    gb.free_data_blocks=(space-sizeofInode())/DATA_BLOCK_SIZE;
    gb.inode_sum++;
  }
  /*  printf("---------------------------------------\n");
    print_gsuperblock(gb);
    printf("---------------------------------------\n");
  */
  move_fd_to_group_block(cfs_fd,inode->nodeid/MAX_INODES);
  write(cfs_fd,&gb,sizeof(Group_superblock));
  move_fd_to_inode(cfs_fd,inode->nodeid);

  write_inode(cfs_fd,inode);
  lseek(cfs_fd,curr_offset,SEEK_SET);//move fd to where it was before
  return ;
}

void move_fd_to_DataBlock(int cfs_fd,Data_location there){
  move_fd_to_group_block(cfs_fd,there.block_group);
  lseek(cfs_fd,GROUP_BLOCK_SZ-(there.block+1)*DATA_BLOCK_SIZE,SEEK_CUR);
  return ;
}

int append_GroupBlock(int cfs_fd){//returns id of appended block
  long int curr_offset=lseek(cfs_fd,0,SEEK_CUR);
  lseek(cfs_fd,0,SEEK_SET);
  Superblock sb;
  read(cfs_fd,&sb,sizeof(Superblock));
  sb.GROUP_BLOCKS++;
  lseek(cfs_fd,0,SEEK_SET);
  write(cfs_fd,&sb,sizeof(Superblock));
  Group_superblock gb;
  group_sb_init(sb,&gb);
  // printf("---------------------------------------\n");
  //print_gsuperblock(gb);
  // printf("---------------------------------------\n");

  move_fd_to_group_block(cfs_fd,sb.GROUP_BLOCKS-1);
  write(cfs_fd,&gb,sizeof(Group_superblock));
  lseek(cfs_fd,curr_offset,SEEK_SET);//move fd to where it was before
  return gb.group_block_id;
}

Data_location occupy_data_hole(int cfs_fd,Inode * inode,int group_block){
  //go to inodes group block and search for data blocks
  //if no datablock there go to the last block and search for available data block
  //else create another group block
  int empty;
  Data_location there;
  Data_location x;
  move_fd_to_group_block(cfs_fd,group_block);
  Group_superblock info;
  read(cfs_fd,&info,sizeof(Group_superblock));
  //printf("***************INFO: %d\n",info.free_data_blocks);
  if(info.free_data_blocks!=0){
    there.block_group=group_block;
    there.block=info.data_sum;
    info.data_sum++;
    info.free_data_blocks--;
    // printf("---------------------------------------\n");
    // print_gsuperblock(info);
    // printf("---------------------------------------\n");

    lseek(cfs_fd,-1*sizeof(Group_superblock),SEEK_CUR);
    write(cfs_fd,&info,sizeof(Group_superblock));
  }else if(info.data_holes>0){
    there.block_group=group_block;
    x.block_group=group_block;
    for(int i=0;i<info.data_sum-1;i++){
      x.block=i;
      move_fd_to_DataBlock(cfs_fd,x);
      read(cfs_fd,&empty,sizeof(int));
      if(empty==-1){
        there.block=i;
        break;
      }
    }
    info.data_holes--;
    move_fd_to_group_block(cfs_fd,group_block);
    write(cfs_fd,&info,sizeof(Group_superblock));
  }else{//go to the last group block and if no space append a new group block
    lseek(cfs_fd,0,SEEK_SET);
    Superblock sb;
    read(cfs_fd,&sb,sizeof(Superblock));//get the superblock

    if(sb.GROUP_BLOCKS-1==info.group_block_id){//if im in the last group block and no space append a new one
      int new_block;
      new_block=append_GroupBlock(cfs_fd);
      move_fd_to_group_block(cfs_fd,new_block);
      Group_superblock gb;
      read(cfs_fd,&gb,sizeof(Group_superblock));
      gb.data_sum++;
      gb.free_data_blocks--;
      lseek(cfs_fd,-1*sizeof(Group_superblock),SEEK_CUR);
      write(cfs_fd,&gb,sizeof(Group_superblock));
      there.block_group=new_block;
      there.block=0;
    }else{//if im not in the last group block -> go there and check for space
      Data_location y;
      y=occupy_data_hole(cfs_fd,inode,sb.GROUP_BLOCKS-1);
      return y;
    }
  }
  return there;
}

int reserve_DataBlock(int cfs_fd,Inode * inode,int blocks){//this functs find avail data block and append it to Inode datastream
  if(inode->data.size+blocks>MDB)
    return 1;//size of file exceeded
  Data_location there;
  int size=0;
  there.block_group=inode->nodeid/MAX_INODES;
  long int curr_offset=lseek(cfs_fd,0,SEEK_CUR);
  for(int i=1;i<=blocks;i++){
    there=occupy_data_hole(cfs_fd,inode,there.block_group);
    inode->data.block[inode->data.size]=there;
    inode->data.size++;
  }
  inode->modification_time=time(NULL);
  move_fd_to_inode(cfs_fd,inode->nodeid);//WRITE CHANGES TO FILE
  write_inode(cfs_fd,inode);
  move_fd_to_DataBlock(cfs_fd,there);
  write(cfs_fd,&size,sizeof(int));
  lseek(cfs_fd,curr_offset,SEEK_SET);//move fd to where it was before
  return 0;
}

void write_inode(int cfs_fd,Inode * i){
  write(cfs_fd,&i->nodeid,sizeof(unsigned int));
  write(cfs_fd,i->filename,MFNS);
  write(cfs_fd,&i->size,sizeof(unsigned int));
  write(cfs_fd,&i->type,sizeof(enum f_type));
  write(cfs_fd,&i->parent_nodeid,sizeof(unsigned int));
  write(cfs_fd,&i->creation_time,sizeof(time_t));

  write(cfs_fd,&i->access_time,sizeof(time_t));

  write(cfs_fd,&i->modification_time,sizeof(time_t));
  write(cfs_fd,&i->data.size,sizeof(int));
  for(int j=0;j<i->data.size;j++){
    write(cfs_fd,&i->data.block[j],sizeof(Data_location));
  }
}

long int seek_Available_inode_id(int group_bl,int cfs_fd){
  //seek in this group block for free inode spot
  //if this is the last group block and no space
  //append another one and get a fresh inode id
  Superblock sb;
  lseek(cfs_fd,0,SEEK_SET);
  read(cfs_fd,&sb,sizeof(Superblock));
  move_fd_to_group_block(cfs_fd,group_bl);
  Group_superblock gb;
  read(cfs_fd,&gb,sizeof(Group_superblock));
  long int free_space=GROUP_BLOCK_SZ-sizeof(Group_superblock)-gb.inode_sum*sizeofInode()-gb.data_sum*DATA_BLOCK_SIZE;
  if(gb.inode_sum<sb.inodes_in_block_group && free_space>=sizeofInode()){//if not full
    return gb.inode_sum+gb.group_block_id*sb.inodes_in_block_group;
  }else if(gb.inode_holes>0){
    char empty[]="EMPTY";
    char str[6];
    int inode_id_start=gb.group_block_id*sb.inodes_in_block_group;
    for(int i=0;i<gb.inode_sum;i++){
      move_fd_to_inode(cfs_fd,inode_id_start+i);
      read(cfs_fd,str,6);
      if(!strcmp(str,empty))
        return inode_id_start+i;
    }
  }else{//check the last group block and if already in last group block append a new one to host inode ids
    if(gb.group_block_id==sb.GROUP_BLOCKS-1){//if im in the last group block and no space
      //append a new block
      int new_block=append_GroupBlock(cfs_fd);
      return new_block*sb.inodes_in_block_group;
    }else{
      int iid=seek_Available_inode_id(sb.GROUP_BLOCKS-1,cfs_fd);
      return iid;
    }
  }
}

long int get_Available_inode_id(int group_bl,int cfs_fd){
  int curr_offset=lseek(cfs_fd,0,SEEK_CUR);//save current fd location
  long int id;
  id=seek_Available_inode_id(group_bl,cfs_fd);
  lseek(cfs_fd,curr_offset,SEEK_SET);//place the fd back to where it was
  return id;
}

void get_Superblock(int cfs_fd,Superblock *sb){
  long int curr_offset=lseek(cfs_fd,0,SEEK_CUR);
  lseek(cfs_fd,0,SEEK_SET);
  read(cfs_fd,sb,sizeof(Superblock));
  lseek(cfs_fd,curr_offset,SEEK_SET);
  return ;
}

void get_inode(int cfs_fd,int id,Inode * node){
  long int offset=lseek(cfs_fd,0,SEEK_CUR);//
  int group_block_id=id/MAX_INODES;
  move_fd_to_inode(cfs_fd,id);
  read(cfs_fd,&node->nodeid,sizeof(unsigned int));
  read(cfs_fd,node->filename,MFNS);
  read(cfs_fd,&node->size,sizeof(unsigned int));
  read(cfs_fd,&node->type,sizeof(enum f_type));
  read(cfs_fd,&node->parent_nodeid,sizeof(unsigned int));
  read(cfs_fd,&node->creation_time,sizeof(time_t));

  read(cfs_fd,&node->access_time,sizeof(time_t));

  read(cfs_fd,&node->modification_time,sizeof(time_t));

  read(cfs_fd,&node->data.size,sizeof(int));
  for(int j=0;j<node->data.size;j++){
    read(cfs_fd,&node->data.block[j],sizeof(Data_location));
  }

  lseek(cfs_fd,offset,SEEK_SET);//leave it to where it was taken from

  return ;
}

void get_Group_superblock(int cfs_fd,int which,Group_superblock * gb){
  long int offset=lseek(cfs_fd,0,SEEK_CUR);
  move_fd_to_group_block(cfs_fd,which);
  read(cfs_fd,gb,sizeof(Group_superblock));
  lseek(cfs_fd,offset,SEEK_SET);
  return ;
}

//return entry num in block: ex. if 1st in block return 1 if 2nd->2...
int check_if_entity_exists(int cfs_fd,Inode * inode,char * entity_name,Data_location *exist_location){//checks inode if entity_name exists
  int exist=0;
  int id;
  exist_location->block_group=-1;
  exist_location->block=-1;
  int entries;
  char * name;
  name=malloc(MFNS*sizeof(char));

  for(int i=0;i<inode->data.size && !exist ;i++){
    move_fd_to_DataBlock(cfs_fd,inode->data.block[i]);
    read(cfs_fd,&entries,sizeof(int));
    for(int j=0;j<entries;j++){
      read(cfs_fd,name,MFNS);
      read(cfs_fd,&id,sizeof(int));
      if(!strcmp(name,entity_name)){
        exist=j+1;
        *exist_location=inode->data.block[i];
        break;
      }
    }
  }
  free(name);
  return exist;
}

int find_Data_location_for_entity(int cfs_fd,Inode *inode,Data_location *free_location){//find free place for new entry in inode dir
  //if no space append a new data block;
  int id;
  free_location->block_group=-1;
  free_location->block=-1;
  int entries;
  char * name;
  name=malloc(MFNS*sizeof(char));

  for(int i=0;i<inode->data.size;i++){
    move_fd_to_DataBlock(cfs_fd,inode->data.block[i]);
    read(cfs_fd,&entries,sizeof(int));
    if(entries<MAX_FILES_IN_DATA_BLOCK){
      *free_location=inode->data.block[i];
      break;
    }
  }
  if(free_location->block_group==-1){//if not found append a new data block
    if(reserve_DataBlock(cfs_fd,inode,1)){//if all data blocks are full->then cant reserve data block
      free(name);
      return 1;
    }
    *free_location=inode->data.block[inode->data.size-1];
  }
  free(name);
  return 0;
}

int insert_entry_in_DataBlock(int cfs_fd,Inode * inode,char* entity_name,int id,Data_location there){
  int entries;
  char * name;
  name=malloc(MFNS*sizeof(char));

  if(name==NULL)
    return 1;

  move_fd_to_DataBlock(cfs_fd,there);
  read(cfs_fd,&entries,sizeof(int));
  lseek(cfs_fd,entries*(MFNS + sizeof(int)),SEEK_CUR);
  strncpy(name,entity_name,MFNS);
  write(cfs_fd,name,MFNS);
  write(cfs_fd,&id,sizeof(int));
  entries++;
  move_fd_to_DataBlock(cfs_fd,there);
  write(cfs_fd,&entries,sizeof(int));//update the entry num in start of data block

  if(entries==1)//if first appearance of data in current dir;
    inode->size+=sizeof(int);
  inode->size+=sizeof(int) + MFNS;
  move_fd_to_inode(cfs_fd,inode->nodeid);
  write_inode(cfs_fd,inode);
  free(name);
  return 0;
}


int my_mkdir(int cfs_fd,Cursor * cursor,char * dir_name){//returns 1 if fails,returns 2 if entity exists in dir
  Inode node;
  alloc_inode(&node);
  int id;
  Data_location there;
  Data_location exist_location;

  if(check_if_entity_exists(cfs_fd,&cursor->node,dir_name,&exist_location)){
    free_inode(node);
    return 2;//indicated existance of dir or file
  }else{
    if(find_Data_location_for_entity(cfs_fd,&cursor->node,&there)){
      printf("Dir size exceeded!\n");
      free_inode(node);
      return 1;
    }
    id=get_Available_inode_id(cursor->info.group_block_id,cfs_fd);
    init_inode(id,dir_name,cursor->node.nodeid,DIRECTORY,&node);
    insert_inode(cfs_fd,&node);

    insert_entry_in_DataBlock(cfs_fd,&cursor->node,dir_name,id,there);
    update_modification_time(cfs_fd,&cursor->node);//update mod time of current dir
    free_inode(node);

    update_info(cfs_fd,cursor);
    move_fd_to_inode(cfs_fd,cursor->node.nodeid);
    return 0;
  }
}

int cfs_touch(int cfs_fd,Cursor * cursor,char * file_name,char * options){
  int id;
  Data_location where_to_put;
  Data_location exist_there;
  int exist;

  exist=check_if_entity_exists(cfs_fd,&cursor->node,file_name,&exist_there);

  Inode new_file;
  alloc_inode(&new_file);
  if(!exist){
    if(options==NULL){//CREATE THE FILE
      if(find_Data_location_for_entity(cfs_fd,&cursor->node,&where_to_put)){
        //printf("%d %d\n",where_to_put.block_group,where_to_put.block);
        printf("Dir '%s' size exceeded!\n",cursor->node.filename);
        return 1;
      }
      id=get_Available_inode_id(where_to_put.block_group,cfs_fd);
      init_inode(id,file_name,cursor->node.nodeid,REGULAR_FILE,&new_file);
      insert_inode(cfs_fd,&new_file);//insert in filesystem the new inode struct
      insert_entry_in_DataBlock(cfs_fd,&cursor->node,file_name,id,where_to_put);
      update_info(cfs_fd,cursor);
    }else if(!strcmp(options,"-a")){
      printf("File must exist to modify access time!\n");
    }else{
      printf("File must exist to modify modification time!\n");
    }
  }else{
    move_fd_to_DataBlock(cfs_fd,exist_there);
    lseek(cfs_fd,sizeof(int)+(exist-1)*(sizeof(int) + MFNS),SEEK_CUR);
    lseek(cfs_fd,MFNS,SEEK_CUR);
    read(cfs_fd,&id,sizeof(int));
    get_inode(cfs_fd,id,&new_file);
    if(options==NULL){
      printf("File %s already exists (Can only modify access and modification_time)\n",file_name);
    }else if(!strcmp(options,"-a")){
      update_access_time(cfs_fd,&new_file);
      update_modification_time(cfs_fd,&cursor->node);
    }else{
      update_modification_time(cfs_fd,&new_file);
      update_modification_time(cfs_fd,&cursor->node);
    }
  }

  free_inode(new_file);
  move_fd_to_inode(cfs_fd,cursor->node.nodeid);
  return 0;
}

void update_access_time(int cfs_fd,Inode * inode){
  inode->access_time=time(NULL);
  move_fd_to_inode(cfs_fd,inode->nodeid);
  write_inode(cfs_fd,inode);
  return;
}
void update_modification_time(int cfs_fd,Inode * inode){
  inode->modification_time=time(NULL);
  move_fd_to_inode(cfs_fd,inode->nodeid);
  write_inode(cfs_fd,inode);
  return;
}
void update_creation_time(int cfs_fd,Inode * inode){
  inode->creation_time=time(NULL);
  move_fd_to_inode(cfs_fd,inode->nodeid);
  write_inode(cfs_fd,inode);
  return;
}

int get_entity_from_dir(int cfs_fd,int i,Inode *dir_to_traverse){
  int id=-1;
  int sum=0;
  int entries;
  for(int j=0;j<dir_to_traverse->data.size;j++){
    move_fd_to_DataBlock(cfs_fd,dir_to_traverse->data.block[j]);
    read(cfs_fd,&entries,sizeof(int));
    sum+=entries;
    if(sum>i){
      //printf("entries %d i %d\n",entries,i);
      lseek(cfs_fd,i*(MFNS+sizeof(int)) + MFNS,SEEK_CUR);
      read(cfs_fd,&id,sizeof(int));
      break;
    }else{
      i-=sum;
      sum=0;
    }
  }
  return id;
}

void dir_traverse(int cfs_fd,Inode *dir_to_traverse,int **inode_id,enum ls_options how,enum ls_options order){
  int files_in_dir=(dir_to_traverse->size-sizeof(int))/(sizeof(int)+MFNS);
  int * tmp=malloc(files_in_dir*sizeof(int));
  char ** names;
  Inode temp_inode;
  alloc_inode(&temp_inode);
  names=malloc(files_in_dir*sizeof(char *));
  for(int i=0;i<files_in_dir;i++)
    names[i]=malloc(MFNS*sizeof(char));

  for(int i=0;i<files_in_dir;i++){
    *((*inode_id)+i)=get_entity_from_dir(cfs_fd,i,dir_to_traverse);
    get_inode(cfs_fd,*((*inode_id)+i),&temp_inode);
    //print_inode(temp_inode);
    strncpy(names[i],temp_inode.filename,MFNS);
  }
  if(order==INORDER)
    sort(names,*inode_id,1,files_in_dir);


  if(how==RECURSIVE){//from the end to start->recursive
    for(int k=files_in_dir-1;k>=0;k--)
      tmp[files_in_dir-1-k]=*((*inode_id)+k);

    free(*inode_id);
    (*inode_id)=tmp;
  }else
    free(tmp);

  for(int i=0;i<files_in_dir;i++)
    free(names[i]);
  free(names);
  free_inode(temp_inode);
  return ;
}


void cfs_ls(int cfs_fd,Cursor * cursor,char * file_name,
enum ls_options which_files,enum ls_options how,
enum ls_options detail,enum ls_options order,enum ls_options type){

  Inode dir_to_traverse;
  alloc_inode(&dir_to_traverse);
  if(which_files==ALL_FILES || which_files==NORMAL_FILES){
    copy_inode(&dir_to_traverse,&cursor->node);
  }else{
    char * tmp_name=malloc(strlen(file_name)+1);
    if(tmp_name==NULL){
      printf("ls failed cause of memory alloc problem!\n");
      free_inode(dir_to_traverse);
      return ;
    }
    strcpy(tmp_name,file_name);
    int id=find_path(cfs_fd,cursor,tmp_name);
    if(id==-1){
      printf("File/dir %s doesn't exist!\n",file_name);
      free_inode(dir_to_traverse);
      return ;
    }
    get_inode(cfs_fd,id,&dir_to_traverse);
    free(tmp_name);
  }
  //TRAVERSE DIRECTORY IF ITS ONE
  if(dir_to_traverse.type!=DIRECTORY){
    if(detail==DETAILED){
      char * times;
      //print_inode(dir_to_traverse);
      times=strtok(ctime(&dir_to_traverse.creation_time),"\n");
      printf("%d %s",dir_to_traverse.size,times);
      times=strtok(ctime(&dir_to_traverse.access_time),"\n");
      printf(" %s",times);
      printf(" %s\n",ctime(&dir_to_traverse.modification_time));
    }else{
      printf("%s\n",dir_to_traverse.filename);
    }
  }else{
    //print_inode(dir_to_traverse);
    int * inode_id;
    int files_in_dir=(dir_to_traverse.size-sizeof(int))/(sizeof(int)+MFNS);
    if(dir_to_traverse.size!=0){
      inode_id=malloc(files_in_dir*sizeof(int));
      if(inode_id==NULL){
        printf("ls failed cause of memory alloc prob!\n");
        free_inode(dir_to_traverse);
        return ;
      }
      dir_traverse(cfs_fd,&dir_to_traverse,&inode_id,how,order);
    }else{
      free_inode(dir_to_traverse);
      return ;
    }

    if(type==ALL_FILES){
      if(detail==DETAILED)
        printf("SIZE\tCREATION\t\t  ACCESS\t\t\tMODIFICATION   NAME\n");
      for(int i=0;i<files_in_dir;i++){
        //printf("%d\n",inode_id[i]);
        get_inode(cfs_fd,inode_id[i],&dir_to_traverse);

        if(which_files==NORMAL_FILES && dir_to_traverse.filename[0]=='.')
          continue;

        if(detail==DETAILED){
          char * times;
          //print_inode(dir_to_traverse);
          times=strtok(ctime(&dir_to_traverse.creation_time),"\n");
          printf("%d   %s",dir_to_traverse.size,times);
          times=strtok(ctime(&dir_to_traverse.access_time),"\n");
          printf("  %s",times);
          times=strtok(ctime(&dir_to_traverse.modification_time),"\n");
          printf(" %s",times);
          printf(" %s\n",dir_to_traverse.filename);
        }else{
          printf("%s ",dir_to_traverse.filename);
        }

      }
      printf("\n");
    }else{
      if(detail==DETAILED)
        printf("SIZE\tCREATION\t\t  ACCESS\t\t\tMODIFICATION   NAME\n");
      for(int i=0;i<files_in_dir;i++){
        get_inode(cfs_fd,inode_id[i],&dir_to_traverse);
        //print_inode(dir_to_traverse);
        if(dir_to_traverse.type==type){

          if(which_files==NORMAL_FILES && dir_to_traverse.filename[0]=='.')
            continue;

          if(detail==DETAILED){
            char * times;
            //print_inode(dir_to_traverse);
            times=strtok(ctime(&dir_to_traverse.creation_time),"\n");
            printf("%d   %s",dir_to_traverse.size,times);
            times=strtok(ctime(&dir_to_traverse.access_time),"\n");
            printf("  %s",times);
            times=strtok(ctime(&dir_to_traverse.modification_time),"\n");
            printf(" %s",times);
            printf(" %s\n",dir_to_traverse.filename);
          }else{
            printf("%s ",dir_to_traverse.filename);
          }
        }

      }
      printf("\n");
    }
    free(inode_id);
  }
  free_inode(dir_to_traverse);
  return ;
}

int cfs_import(int cfs_fd,Cursor * cur_cursor,char * file_path,char * dir){
  long int old_offset=lseek(cfs_fd,0,SEEK_CUR);
  char temp_filepath[300];
  strcpy(temp_filepath,file_path);
//kaiiiiiiiiiiiiiiiii ta deleteeeeeeeeeeeeeeeeeeeeeee
  Cursor cursor;
  cursor_alloc(&cursor);
  cursor_copy(&cursor,cur_cursor);

  if(strcmp(dir,"./"))
    if(go_to_path(cfs_fd,&cursor,dir)==-1){
      cursor_free(&cursor);
      lseek(cfs_fd,old_offset,SEEK_SET);
      return 3;
    }

  Inode dir_node;
  alloc_inode(&dir_node);
  copy_inode(&dir_node,&cursor.node);

  if(dir_node.type!=DIRECTORY){
    cursor_free(&cursor);
    free_inode(dir_node);
    lseek(cfs_fd,old_offset,SEEK_SET);
    return 1;//indicates file isnt a dir
  }

  struct stat statbuf;
  if(stat(temp_filepath,&statbuf)==-1){
    perror("Error");
    cursor_free(&cursor);
    free_inode(dir_node);
    lseek(cfs_fd,old_offset,SEEK_SET);
    return 0;
  }

  char *filename=temp_filepath;
  char * strtok_ptr=strtok(temp_filepath,"/");
  while(strtok_ptr!=NULL){
    filename=strtok_ptr;
    strtok_ptr=strtok(NULL,"/");
  }

  if(S_ISDIR(statbuf.st_mode)){//if directory
    int ret_val=my_mkdir(cfs_fd,&cursor,filename);//ftiaxnw to directory
    if(ret_val==1){
      cursor_free(&cursor);
      free_inode(dir_node);
      lseek(cfs_fd,old_offset,SEEK_SET);
      printf("All data blocks are full!\n\tCan't create files/dirs in this directory\n");
      return 0;
    }else if(ret_val==2){
      cursor_free(&cursor);
      free_inode(dir_node);
      lseek(cfs_fd,old_offset,SEEK_SET);
      //printf("Directory/File '%s' exists in dir!\n",filename);
      return 0;
    }
    go_to_path(cfs_fd,&cursor,filename);
    printf("%s\n",cursor.path);
    DIR * dir;
    struct dirent * dent;//directory entity
    dir=opendir(file_path);
    char files_in_dir_path[300];
    sprintf(files_in_dir_path,"%s",file_path);
    int len=strlen(files_in_dir_path);
    files_in_dir_path[len]='/';
    files_in_dir_path[len+1]='\0';
    len++;

    while((dent=readdir(dir))){
      if(!strcmp(dent->d_name,".")||!strcmp(dent->d_name,".."))
        continue;
      strncpy(files_in_dir_path+len,dent->d_name,strlen(dent->d_name)+1);
      cfs_import(cfs_fd,&cursor,files_in_dir_path,"./");
      update_info(cfs_fd,&cursor);
    }
    closedir(dir);
  }else{
    int linux_fd;
    int my_fd;
    char buff[100];
    int bytes_read;
    Inode my_file;
    alloc_inode(&my_file);
    if(cfs_touch(cfs_fd,&cursor,filename,NULL)==1){
      cursor_free(&cursor);
      free_inode(dir_node);
      free_inode(my_file);
      lseek(cfs_fd,old_offset,SEEK_SET);
      return 2;
    }
    linux_fd=open(file_path,O_RDONLY);
    my_fd=my_open(cfs_fd,&cursor,filename);
    bytes_read=read(linux_fd,buff,100);
    while(bytes_read!=0){
       /*for(int i=0;i<bytes_read;i++){
         printf("%c",buff[i]);
       }*/
      my_write(cfs_fd,my_fd,buff,bytes_read);

      bytes_read=read(linux_fd,buff,100);
    }
     //printf("\n");

    //get_inode(cfs_fd,open_fds.table[my_fd].inode_id,&my_file);
    //print_inode(my_file);
    close(linux_fd);
    my_close(my_fd);
    free_inode(my_file);
  }
  //printf("%d\n",dir_id);
  cursor_free(&cursor);
  free_inode(dir_node);
  lseek(cfs_fd,old_offset,SEEK_SET);
  return 0;
}

int cfs_export(int cfs_fd,Cursor * cur_cursor,char * file_path,char * dir){
  long int old_offset=lseek(cfs_fd,0,SEEK_CUR);
  struct stat statbuf;
  char temp_filepath[300];
  strcpy(temp_filepath,file_path);

  if(stat(dir,&statbuf)==-1){
    lseek(cfs_fd,old_offset,SEEK_SET);
    return 1;
  }

  if(!S_ISDIR(statbuf.st_mode)){//if not a directory
    lseek(cfs_fd,old_offset,SEEK_SET);//go back to where you where
    return 1;
  }

  int cfs_id=find_path(cfs_fd,cur_cursor,temp_filepath);
  if(cfs_id==-1){
    lseek(cfs_fd,old_offset,SEEK_SET);//go back to where you where
    return 2;
  }
  Inode cfs_file;
  alloc_inode(&cfs_file);
  get_inode(cfs_fd,cfs_id,&cfs_file);
  char path_to_create_file[300];
  strcpy(path_to_create_file,dir);
  int len=strlen(path_to_create_file);
  path_to_create_file[len]='/';
  path_to_create_file[len+1]='\0';
  len++;
  strcpy(path_to_create_file+len,cfs_file.filename);
  if(cfs_file.type==DIRECTORY){
    char temp_path[300];
    strcpy(temp_path,temp_filepath);
    int temp_len=strlen(temp_path);
    temp_path[temp_len]='/';
    temp_path[temp_len+1]='\0';
    temp_len++;
    int ret_val;
    ret_val=mkdir(path_to_create_file,S_IRWXU);//read write execute for owner
    if(ret_val==-1){
      printf("Failed to create dir %s in linux fs!\n",cfs_file.filename);
      free_inode(cfs_file);
      lseek(cfs_fd,old_offset,SEEK_SET);
      return 0;
    }
    long int files_in_dir=0;
    if(cfs_file.size!=0)
      files_in_dir=(cfs_file.size-sizeof(int))/(sizeof(int)+MFNS);


    int entity_id;
    Inode entity;
    alloc_inode(&entity);
    for(int i=0;i<files_in_dir;i++){
      entity_id=get_entity_from_dir(cfs_fd,i,&cfs_file);
      get_inode(cfs_fd,entity_id,&entity);\
      strcpy(temp_path+temp_len,entity.filename);
      printf("%s\n",temp_path);
      cfs_export(cfs_fd,cur_cursor,temp_path,path_to_create_file);
    }
    free_inode(entity);
  }else{//export only for regular files-->NO LINKS EXPORT
    int linux_fd=open(path_to_create_file,O_WRONLY|O_CREAT,S_IRWXU);
    int my_fd=my_open(cfs_fd,cur_cursor,temp_filepath);
    int bytes_read;
    char buff[100];
    bytes_read=my_read(cfs_fd,my_fd,buff,100);
    while(bytes_read>0){
      write(linux_fd,buff,bytes_read);
      bytes_read=my_read(cfs_fd,my_fd,buff,100);
    }
    close(linux_fd);
    my_close(my_fd);
  }
  free_inode(cfs_file);
  lseek(cfs_fd,old_offset,SEEK_SET);
  return 0;
}

int cfs_cat(int cfs_fd,Cursor * cur_cursor,char **files,int num_of_files){//if file doesnt exist create it in current dir
  //num of files indicates num of files to append on output files[num_of_files+1]
  int existing_id;
  existing_id=find_path(cfs_fd,cur_cursor,files[num_of_files+1]);
  int output_fd;
  Inode output_file;
  alloc_inode(&output_file);

  if(existing_id==-1){//file doesn't exist so create it
    char * name=files[num_of_files+1];
    char * strtok_ptr=strtok(files[num_of_files+1],"/");
    while(strtok_ptr!=NULL){
      name=strtok_ptr;
      strtok_ptr=strtok(NULL,"/");
    }

    int ret_val;
    ret_val=cfs_touch(cfs_fd,cur_cursor,name,NULL);
    if(ret_val==1){//dir size exceeded
      free_inode(output_file);
      return 0;
    }
    update_info(cfs_fd,cur_cursor);
    existing_id=find_path(cfs_fd,cur_cursor,name);
    output_fd=my_open(cfs_fd,cur_cursor,name);
    get_inode(cfs_fd,existing_id,&output_file);
  }else{//if exists
    get_inode(cfs_fd,existing_id,&output_file);
    for(int i=0;i<output_file.data.size;i++){//discard all the data blocks;
      free_DataBlock(cfs_fd,output_file.data.block[i]);
    }
    output_file.data.size=0;
    output_file.size=0;
    move_fd_to_inode(cfs_fd,output_file.nodeid);
    write_inode(cfs_fd,&output_file);//update inode info
    output_fd=my_open(cfs_fd,cur_cursor,files[num_of_files+1]);
  }

  //print_inode(output_file);//teeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeest

  update_creation_time(cfs_fd,&output_file);
  int total_bytes_written=0;
  int read_fd;
  int bytes_read;
  char buff[100];
  int bytes_written;
  for(int i=0;i<num_of_files;i++){
    read_fd=my_open(cfs_fd,cur_cursor,files[i]);
    //testttttttt
    //Inode tmp;
    //alloc_inode(&tmp);
    //get_inode(cfs_fd,open_fds.table[read_fd].inode_id,&tmp);
    //print_inode(tmp);
    if(read_fd==-1){
      printf("Error opening %s!\n",files[i]);
      continue;
    }
    bytes_read=my_read(cfs_fd,read_fd,buff,100);
    while(bytes_read>0){
      bytes_written=my_write(cfs_fd,output_fd,buff,bytes_read);
      total_bytes_written+=bytes_written;
      get_inode(cfs_fd,existing_id,&output_file);
      bytes_read=my_read(cfs_fd,read_fd,buff,100);
      //printf("Bytes read: %d\n",bytes_read);
      //printf("cur data block:%d\n",open_fds.table[read_fd].cur_data_block);
    //  printf("offset:%d\n",open_fds.table[read_fd].offset);

    }
    my_close(read_fd);
  }

  move_fd_to_inode(cfs_fd,output_file.nodeid);
  write_inode(cfs_fd,&output_file);
  my_close(output_fd);
  //for testing purposes
  //get_inode(cfs_fd,existing_id,&output_file);
  //print_inode(output_file);
  free_inode(output_file);
  return 0;
}

int free_DataBlock(int cfs_fd,Data_location x){
  Group_superblock info;
  get_Group_superblock(cfs_fd,x.block_group,&info);
  // print_gsuperblock(info);

  move_fd_to_DataBlock(cfs_fd,x);
  int empty=-1;//indicated empty and unused datablock
  write(cfs_fd,&empty,sizeof(int));
  if(x.block<=info.data_sum-2)
    info.data_holes++;
  else if(x.block==info.data_sum-1){
    info.data_sum--;
  }
  move_fd_to_group_block(cfs_fd,x.block_group);
  write(cfs_fd,&info,sizeof(Group_superblock));
  //teeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeest
  // get_Group_superblock(cfs_fd,x.block_group,&info);
  // printf("********************\n");
  // print_gsuperblock(info);
  return 1;
}

int cfs_cp(int cfs_fd,Cursor * cur_cursor,char **files,int num_of_files,enum cp_options how,enum cp_options ask){
  long int old_offset=lseek(cfs_fd,0,SEEK_CUR);
  int existing_id;
  existing_id=find_path(cfs_fd,cur_cursor,files[num_of_files-1]);
  if(existing_id==-1)
    return 1;//indicates files or dir doesn't exist
  Inode output_file;
  alloc_inode(&output_file);
  get_inode(cfs_fd,existing_id,&output_file);
  if(output_file.type==REGULAR_FILE){
    if(num_of_files>2){
      free_inode(output_file);
      return 2;//indicates that user tries to copy multiple entities to a regular file
    }
    Inode source_file;
    alloc_inode(&source_file);
    existing_id=find_path(cfs_fd,cur_cursor,files[num_of_files-2]);
    if(existing_id==-1){
      free_inode(output_file);
      free_inode(source_file);
      return 1;
    }
    get_inode(cfs_fd,existing_id,&source_file);
    if(source_file.type==DIRECTORY){
      free_inode(output_file);
      free_inode(source_file);
      return 3;//CANT COPY DIR IN FILE
    }
    if(ask==ASK){
      printf("Copy '%s' to '%s'?(y/n):\n",source_file.filename,output_file.filename);
      char ans;
      read(0,&ans,sizeof(char));
      getchar();
      if(ans!='y'){
        free_inode(output_file);
        free_inode(source_file);
        return 0;//if anything given except y
      }
    }
    int read_fd=my_open(cfs_fd,cur_cursor,files[num_of_files-2]);
    int write_fd=my_open(cfs_fd,cur_cursor,files[num_of_files-1]);
    char buff[100];
    int bytes_read;
    bytes_read=my_read(cfs_fd,read_fd,buff,100);
    while(bytes_read>0){
      my_write(cfs_fd,write_fd,buff,bytes_read);
      bytes_read=my_read(cfs_fd,read_fd,buff,100);
    }
    my_close(read_fd);
    my_close(write_fd);
    free_inode(source_file);
  }else{
    Inode source_file;
    alloc_inode(&source_file);
    Cursor tmp_cursor;
    cursor_alloc(&tmp_cursor);
    int read_fd;
    int write_fd;
    int ret_val;
    char path_copy[300];
    go_to_path(cfs_fd,&tmp_cursor,files[num_of_files-1]);//move cursor to dir
    for(int i=0;i<num_of_files-1;i++){
      strcpy(path_copy,files[i]);
      existing_id=find_path(cfs_fd,cur_cursor,path_copy);
      if(existing_id==-1){
        printf("'%s' doesn't exist!\n",files[i]);
        continue;
      }
      get_inode(cfs_fd,existing_id,&source_file);
      if(ask==ASK){
        printf("Copy '%s' to '%s'?(y/n):\n",source_file.filename,output_file.filename);
        char ans;
        read(0,&ans,sizeof(char));
        getchar();
        if(ans!='y')
          continue;

      }
      if(source_file.type==DIRECTORY){
        ret_val=my_mkdir(cfs_fd,&tmp_cursor,source_file.filename);
        if(ret_val==1||ret_val==2)
          continue;//couldnt make dir there

        if(how==REC){//then i need to cp all of its contents
          Inode dir_entity;
          alloc_inode(&dir_entity);
          char ** names;
          int entities=(source_file.size-sizeof(int))/(MFNS+sizeof(int));
          if(entities<0){
            free_inode(dir_entity);
            continue;
          }
          names=malloc((entities+1)*sizeof(char *));
          for(int j=0;j<entities;j++){
            names[j]=malloc(300*sizeof(char));
            existing_id=get_entity_from_dir(cfs_fd,j,&source_file);
            get_inode(cfs_fd,existing_id,&dir_entity);
            strcpy(names[j],files[i]);
            strcat(names[j],"/");
            strcat(names[j],dir_entity.filename);
            //printf("%s\n",names[j]);
          }
          names[entities]=malloc(300*sizeof(char));//holds the path
          //printf("%s\n",names[entities]);
          strcpy(names[entities],files[num_of_files-1]);
          strcat(names[entities],"/");
          strcat(names[entities],source_file.filename);
          cfs_cp(cfs_fd,cur_cursor,names,entities+1,how,ask);
          free_inode(dir_entity);
          for(int j=0;j<entities+1;j++)
            free(names[j]);
          free(names);
        }
      }else{
        ret_val=cfs_touch(cfs_fd,&tmp_cursor,source_file.filename,NULL);
        if(ret_val==1)//dir size exceeded
          break;
        read_fd=my_open(cfs_fd,cur_cursor,files[i]);
        write_fd=my_open(cfs_fd,&tmp_cursor,source_file.filename);
        if(write_fd<0)//file may not be created from cfs_touch
          break;
        char buff[100];
        int bytes_read=my_read(cfs_fd,read_fd,buff,100);
        while(bytes_read>0){
          my_write(cfs_fd,write_fd,buff,bytes_read);
          bytes_read=my_read(cfs_fd,read_fd,buff,100);
        }
        my_close(read_fd);
        my_close(write_fd);
      }
      update_info(cfs_fd,&tmp_cursor);
    }
    free_inode(source_file);
    cursor_free(&tmp_cursor);
  }
  free_inode(output_file);
  lseek(cfs_fd,old_offset,SEEK_SET);
  return 0;
}

int cfs_mv(int cfs_fd,Cursor * cur_cursor,char **files,int num_of_files,enum cp_options ask){
  int old_offset=lseek(cfs_fd,0,SEEK_CUR);
  int existing_id;
  existing_id=find_path(cfs_fd,cur_cursor,files[num_of_files-1]);
  if(existing_id==-1)
    return 1;//indicates tha destination doesnt exist
  Inode output_file;
  alloc_inode(&output_file);
  get_inode(cfs_fd,existing_id,&output_file);
  if(output_file.type!=DIRECTORY){
    free_inode(output_file);
    return 2;//indicates tha output not a drectory
  }
  Inode source_file;
  alloc_inode(&source_file);
  Inode parent_dir;
  alloc_inode(&parent_dir);
  char tmp_name[300];
  for(int i=0;i<num_of_files-1;i++){
    strcpy(tmp_name,files[i]);
    existing_id=find_path(cfs_fd,cur_cursor,tmp_name);
    if(existing_id==-1){
      printf("%s doesn't exist!\n",files[i]);
      continue;
    }
    get_inode(cfs_fd,existing_id,&source_file);
    get_inode(cfs_fd,source_file.parent_nodeid,&parent_dir);
    if(ask==ASK){
      printf("Copy '%s' to '%s'?(y/n):\n",source_file.filename,output_file.filename);
      char ans;
      read(0,&ans,sizeof(char));
      getchar();
      if(ans!='y')
        continue;
    }
    Data_location exist_location;
    check_if_entity_exists(cfs_fd,&output_file,source_file.filename,&exist_location);
    if(exist_location.block_group!=-1){//if exists
      printf("%s already exists in %s!\n",source_file.filename,output_file.filename);
      continue;
    }
    Data_location free_location;
    if(find_Data_location_for_entity(cfs_fd,&output_file,&free_location)){
      printf("Dir size exceeded!\n");
      free_inode(parent_dir);
      free_inode(source_file);
      free_inode(output_file);
      lseek(cfs_fd,old_offset,SEEK_SET);
      return 3;
    }
    rm_entry(cfs_fd,&source_file,&parent_dir);
    source_file.parent_nodeid=output_file.nodeid;
    move_fd_to_inode(cfs_fd,source_file.nodeid);
    write_inode(cfs_fd,&source_file);
    insert_entry_in_DataBlock(cfs_fd,&output_file,source_file.filename,source_file.nodeid,free_location);
    move_fd_to_inode(cfs_fd,output_file.nodeid);
    write_inode(cfs_fd,&output_file);
    move_fd_to_inode(cfs_fd,parent_dir.nodeid);;
    write_inode(cfs_fd,&parent_dir);
    update_info(cfs_fd,cur_cursor);
  }

  free_inode(parent_dir);
  free_inode(source_file);
  free_inode(output_file);
  lseek(cfs_fd,old_offset,SEEK_SET);
  return 0;
}

void rm_entry(int cfs_fd,Inode * file_to_rm,Inode * parent_dir){
  int old_offset=lseek(cfs_fd,0,SEEK_CUR);
  int files_in_dir=(parent_dir->size-sizeof(int))/(MFNS+sizeof(int));
  int id;
  int position;
  for(int i=0;i<files_in_dir;i++){
    id=get_entity_from_dir(cfs_fd,i,parent_dir);
    if(id==file_to_rm->nodeid){
      position=i;
      break;
    }
  }

  int position_datablock=position/MAX_FILES_IN_DATA_BLOCK;
  //printf("position %d\n",position);
  //printf("datablock %d\n",position_datablock);
  position=position%MAX_FILES_IN_DATA_BLOCK;
  Data_location there;
  there=parent_dir->data.block[parent_dir->data.size-1];//move to last datablock
  move_fd_to_DataBlock(cfs_fd,there);
  int files_in_datablock;
  char * new_file=malloc(MFNS*sizeof(char));
  int new_id;
  read(cfs_fd,&files_in_datablock,sizeof(int));
  //printf("files in last datablock %d\n",files_in_datablock);
  lseek(cfs_fd,(files_in_datablock-1)*(MFNS +sizeof(int)),SEEK_CUR);
  //get the name and if of last entry in dir to fill the hole
  read(cfs_fd,new_file,MFNS);
  read(cfs_fd,&new_id,sizeof(int));
  files_in_datablock--;

  if(files_in_datablock==0){
    free_DataBlock(cfs_fd,there);
    parent_dir->data.size--;
    if(new_id==file_to_rm->nodeid){//if dir has only one file and want to remove it
      parent_dir->size=0;
      move_fd_to_inode(cfs_fd,parent_dir->nodeid);
      write_inode(cfs_fd,parent_dir);
      free(new_file);
      lseek(cfs_fd,old_offset,SEEK_SET);
      return ;
    }
  }else{
    move_fd_to_DataBlock(cfs_fd,there);
    write(cfs_fd,&files_in_datablock,sizeof(int));
    parent_dir->size=parent_dir->size - MFNS - sizeof(int);
    //print_inode(*parent_dir);
    if(new_id==file_to_rm->nodeid){//if the entry to be removed is the last
      move_fd_to_inode(cfs_fd,parent_dir->nodeid);
      write_inode(cfs_fd,parent_dir);
      free(new_file);
      lseek(cfs_fd,old_offset,SEEK_SET);
      return ;
    }
    move_fd_to_inode(cfs_fd,parent_dir->nodeid);
    write_inode(cfs_fd,parent_dir);
  }
  //now time to write the changes (fill the hole)
  //printf("OOK\n");
  there=parent_dir->data.block[position_datablock];
  move_fd_to_DataBlock(cfs_fd,there);
  lseek(cfs_fd,sizeof(int),SEEK_CUR);//pass the int in the start of data block;
  lseek(cfs_fd,position*(MFNS+sizeof(int)),SEEK_CUR);//go to the right spot
  write(cfs_fd,new_file,MFNS);
  write(cfs_fd,&new_id,sizeof(int));
  //printf("%s %d\n",new_file,new_id);

  free(new_file);
  lseek(cfs_fd,old_offset,SEEK_SET);
  return ;
}

int cfs_rm(int cfs_fd,Cursor * cur_cursor,char **files,int num_of_files,enum cp_options ask){
  int old_offset=lseek(cfs_fd,0,SEEK_CUR);
  Inode parent_dir;
  int count=0;
  alloc_inode(&parent_dir);
  Inode to_be_rm;
  alloc_inode(&to_be_rm);
  int existing_id;
  char temp_name[300];
  for(int i=0;i<num_of_files;i++){
    strcpy(temp_name,files[i]);
    existing_id=find_path(cfs_fd,cur_cursor,temp_name);
    if(existing_id==-1){
      printf("'%s' doesn't exist!\n",files[i]);
      continue;
    }
    get_inode(cfs_fd,existing_id,&to_be_rm);
    if(!strcmp(to_be_rm.filename,"/")){
      printf("Cant delete the root directory!\n");
      continue;
    }
    if(ask==ASK){
      printf("Remove '%s'?(y/n):\n",to_be_rm.filename);
      char ans;
      read(0,&ans,sizeof(char));
      getchar();
      if(ans!='y')
        continue;
    }
    count++;
    get_inode(cfs_fd,to_be_rm.parent_nodeid,&parent_dir);
    if(to_be_rm.type!=DIRECTORY){
      rm_entry(cfs_fd,&to_be_rm,&parent_dir);
      for(int j=0;j<to_be_rm.data.size;j++)//clear all of its data blocks
        free_DataBlock(cfs_fd,to_be_rm.data.block[j]);
      rm_inode(cfs_fd,&to_be_rm);
      update_modification_time(cfs_fd,&parent_dir);
      move_fd_to_inode(cfs_fd,parent_dir.nodeid);
      write_inode(cfs_fd,&parent_dir);
      update_info(cfs_fd,cur_cursor);//update the info contained in cursor
    }else{
      Inode dir_entity;
      alloc_inode(&dir_entity);
      char ** names;
      int files_deleted=0;
      int entities=(to_be_rm.size-sizeof(int))/(MFNS+sizeof(int));
      if(entities>0){

        names=malloc(entities*sizeof(char *));
        for(int j=0;j<entities;j++){
          names[j]=malloc(300*sizeof(char));
          existing_id=get_entity_from_dir(cfs_fd,j,&to_be_rm);
          get_inode(cfs_fd,existing_id,&dir_entity);
          strcpy(names[j],files[i]);
          strcat(names[j],"/");
          strcat(names[j],dir_entity.filename);
          //printf("%s\n",names[j]);
        }
        files_deleted=cfs_rm(cfs_fd,cur_cursor,names,entities,ask);
        update_info(cfs_fd,cur_cursor);
      }
      //now rm the directory itself if it doesn't contain anything
      //printf("------------------------%d\n",files_deleted);
      if(files_deleted==entities && entities!=0 || to_be_rm.size==0){
        update_modification_time(cfs_fd,&parent_dir);
        move_fd_to_inode(cfs_fd,parent_dir.nodeid);
        write_inode(cfs_fd,&parent_dir);
        rm_entry(cfs_fd,&to_be_rm,&parent_dir);
        for(int j=0;j<to_be_rm.data.size;j++)//clear all of its data blocks
          free_DataBlock(cfs_fd,to_be_rm.data.block[j]);
        rm_inode(cfs_fd,&to_be_rm);
      }
      update_info(cfs_fd,cur_cursor);//update the info contained in cursor

      free_inode(dir_entity);
      if(entities>0){
        for(int j=0;j<entities;j++)
          free(names[j]);
        free(names);
      }
    }
  }
  free_inode(parent_dir);
  free_inode(to_be_rm);
  lseek(cfs_fd,old_offset,SEEK_SET);
  return count;
}

void rm_inode(int cfs_fd,Inode * to_be_rm){
  Group_superblock info;
  get_Group_superblock(cfs_fd,to_be_rm->nodeid/MAX_INODES,&info);
  move_fd_to_inode(cfs_fd,to_be_rm->nodeid);
  char empty[]="EMPTY";//indicated empty and unused spot for inode
  write(cfs_fd,&empty,6);
  if(to_be_rm->nodeid<=info.inode_sum-2)
    info.inode_holes++;
  else if(to_be_rm->nodeid==info.inode_sum-1){
    //printf("yes\n");
    info.inode_sum--;
  }
  move_fd_to_group_block(cfs_fd,info.group_block_id);
  write(cfs_fd,&info,sizeof(Group_superblock));
  //teeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeest
  // get_Group_superblock(cfs_fd,to_be_rm->nodeid/MAX_INODES,&info);
  // print_gsuperblock(info);
  return ;
}
