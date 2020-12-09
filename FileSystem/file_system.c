#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include "super_blocks/super_blocks.h"
#include "inode/inode.h"
#include "cursor/cursor.h"
#include "input_check/input_check.h"
#include "fs_operations/fs_operations.h"
#include "io/io.h"

Fd_table open_fds;//global struct for openfds


//group block size 1MB
const int GROUP_BLOCK_SZ=1048576;
int MDB;//MAX DATA BLOCKS PER FILE
int MFNS;//MAX FILENAME SIZE
int MAX_INODES;
int DATA_BLOCK_SIZE;
int MAX_FILES_IN_DATA_BLOCK;

int main(void){
  fd_table_init();
  char buff[100];

  int term_index;
  char * operation;
  char c;
  char * file_name;
  Superblock super_block;
  char * tmp;
  int cfs_fd;
  while (1){
    printf("> ");
    fflush(stdout);
    term_index=read(0,buff,99);
    buff[term_index]='\0';
    operation=strtok(buff," ");
    if(operation==NULL || operation[0]==' ' || operation[0]=='\n')
      continue;

    if(!strcmp(operation,"cfs_workwith")){
      if(cfs_workwith_check(buff+strlen(buff)+1)){
        printf("Wrong syntax for cfs_workwith!\n");
        continue;
      }
      file_name=buff + strlen(buff)+1;
      file_name[strlen(file_name)-1]='\0';
      cfs_fd=open(file_name,O_RDWR);
      if(cfs_fd<0){
        printf("Failed to open file %s\n\tFile may not exist\n",file_name);
        continue;
      }
      close(cfs_fd);
      break;
    }else if(!strcmp(operation,"cfs_create")){
       if(cfs_create_check(buff+strlen(buff)+1)){
         printf("Wrong syntax for cfs_create!\n");
         continue;
       }
       tmp=buff + strlen(buff)+1;
       int flag[3]={0};
       for(int i=1;i<=3;i++){
         if(!flag[0] && !strcmp(tmp,"-bs")){
           super_block.DATA_BLOCK_SIZE=atoi(tmp+strlen(tmp)+1);
           flag[0]=1;
         }else if(!flag[1] && !strcmp(tmp,"-fns")){
           super_block.FILENAME_SIZE=atoi(tmp+strlen(tmp)+1);
           flag[1]=1;
         }else if(!flag[2] && !strcmp(tmp,"-cfs")){
           super_block.MAX_DATA_BLOCKS=atoi(tmp+strlen(tmp)+1);
           flag[2]=1;
         }
         tmp=tmp+strlen(tmp)+1;//now temp point to a number
         tmp=tmp+strlen(tmp)+1;//so pass the number
       }
      file_name=tmp;
      file_name[strlen(tmp)-1]='\0';

      DATA_BLOCK_SIZE=super_block.DATA_BLOCK_SIZE;
      MFNS=super_block.FILENAME_SIZE;
      MDB=super_block.MAX_DATA_BLOCKS;
      super_block.GROUP_BLOCK_SIZE=GROUP_BLOCK_SZ;
      super_block.GROUP_BLOCKS=0;
      super_block.INODE_SIZE=sizeofInode();
      super_block.inodes_in_block_group=compute_inodes_in_block_group(super_block.DATA_BLOCK_SIZE,
        super_block.GROUP_BLOCK_SIZE,super_block.INODE_SIZE);
      MAX_INODES=super_block.inodes_in_block_group;
      super_block.MAX_FILES_IN_DATA_BLOCK=(DATA_BLOCK_SIZE-sizeof(int))/(MFNS+sizeof(int));
      MAX_FILES_IN_DATA_BLOCK=super_block.MAX_FILES_IN_DATA_BLOCK;
      cfs_fd=open(file_name,O_CREAT|O_RDWR,0777);
      if(cfs_fd<0){
        perror("Error creating file (file may exist)");
        continue;
      }
     print_superblock(super_block);
      write(cfs_fd,&super_block,sizeof(super_block));
      append_GroupBlock(cfs_fd);
      Inode root;
      alloc_inode(&root);
      init_inode(0,"/",0,DIRECTORY,&root);
      // write_inode(cfs_fd,sizeof(super_block)+sizeof(Group_superblock),&root);
      //reserve_DataBlock(cfs_fd,&root);
      insert_inode(cfs_fd,&root);
      free_inode(root);
      close(cfs_fd);
    }else{
      printf("Enter 'cfs_workwith [FILE]' to work with an existing cfs or create one with 'cfs_create [FILE]' to begin!\n");
      printf("\t Can't do any operation unless the cfs file specified\n");
    }
  }

  cfs_fd=open(file_name,O_RDWR);
  if(cfs_fd<0){
    perror("Error opening file:");
    exit(1);
  }
  get_Superblock(cfs_fd,&super_block);
  DATA_BLOCK_SIZE=super_block.DATA_BLOCK_SIZE;
  MDB=super_block.MAX_DATA_BLOCKS;
  MFNS=super_block.FILENAME_SIZE;
  MAX_INODES=super_block.inodes_in_block_group;
  MAX_FILES_IN_DATA_BLOCK=super_block.MAX_FILES_IN_DATA_BLOCK;
  print_superblock(super_block);
//print_gsuperblock(b);
  Cursor current_dir;
  cursor_alloc(&current_dir);
  current_dir_rewind(cfs_fd,&current_dir);
  char buffer[300];
  Inode node;//for creating new files
  alloc_inode(&node);
  int total_read;
  char * strtok_ptr;
  while(1){
    // update_info(cfs_fd,&current_dir);
    //print_inode(current_dir.node);
    print_cursor(&current_dir);
    total_read=read(0,buffer,299);
    if(total_read==1)
      continue;
    buffer[total_read-1]='\0';
    strtok_ptr=strtok(buffer," ");
  //--------------CFS MKDIR ------------------------------------
  //************************************************************
    if(!strcmp(strtok_ptr,"cfs_mkdir")){
      strtok_ptr=strtok(NULL," ");
      int ret_val;
      while(strtok_ptr!=NULL){
        ret_val=my_mkdir(cfs_fd,&current_dir,strtok_ptr);
        if(ret_val==1){
          printf("All data blocks are full!\n\tCan't create files/dirs in this directory\n");
          break;
        }else if(ret_val==2){
          printf("Directory/File '%s' exists in current dir!\n",strtok_ptr);
        }
        strtok_ptr=strtok(NULL," ");
      }
  //-----------------CFS TOUCH ----------------------------------
  //*************************************************************
    }else if(!strcmp(strtok_ptr,"cfs_touch")){
      strtok_ptr=strtok(NULL," ");
      //need to check input
      while(strtok_ptr!=NULL){
        if(!strcmp(strtok_ptr,"-a")){
          strtok_ptr=strtok(NULL," ");
          cfs_touch(cfs_fd,&current_dir,strtok_ptr,"-a");
        }else if(!strcmp(strtok_ptr,"-m")){
          strtok_ptr=strtok(NULL," ");
          cfs_touch(cfs_fd,&current_dir,strtok_ptr,"-m");
        }else{
          cfs_touch(cfs_fd,&current_dir,strtok_ptr,NULL);
        }
        strtok_ptr=strtok(NULL," ");
      }

    }else if(!strcmp(strtok_ptr,"cfs_pwd")){
      printf("%s\n",current_dir.path);
    }else if(!strcmp(strtok_ptr,"cfs_cd")){
      int ret_val;
      strtok_ptr=strtok(NULL," ");
      if(strtok_ptr==NULL || (strtok_ptr[0]=='/' && strtok_ptr[1]=='\0'))
        current_dir_rewind(cfs_fd,&current_dir);
      else{
        ret_val=go_to_path(cfs_fd,&current_dir,strtok_ptr);
        if(ret_val==-1)
          printf("No such path!\n");
      }
  //----------------------CFS LS -------------------------------------
  //******************************************************************
    }else if(!strcmp(strtok_ptr,"cfs_ls")){
      strtok_ptr=strtok(NULL," ");
      char * file_name=NULL;
      //default values
      enum ls_options which_files;//ALL_FILES|SINGLE_FILE
      enum ls_options how=NON_RECURSIVE;//RECURSIVE|NON_RECURSIVE
      enum ls_options detail=NON_DETAILED;//DETAILED|NON_DETAILED
      enum ls_options order=INORDER;//INORDER|NOORDER
      enum ls_options type=ALL_FILES;//ONLY_DIRS|ONLY_LINKS|ALL_FILES
      int flag=0;
      while(strtok_ptr!=NULL){
        if(!strcmp(strtok_ptr,"-l")){
          detail=DETAILED;
        }else if(!strcmp(strtok_ptr,"-a")){
          which_files=ALL_FILES;
          flag=1;
        }else if(!strcmp(strtok_ptr,"-r")){
          how=RECURSIVE;
        }else if(!strcmp(strtok_ptr,"-u")){
          order=NOORDER;
        }else if(!strcmp(strtok_ptr,"-d")){
          type=ONLY_DIRS;
        }else if(!strcmp(strtok_ptr,"-h")){
          type=ONLY_LINKS;
        }else{
          file_name=strtok_ptr;
          break;
        }
        strtok_ptr=strtok(NULL," ");
      }
      if(file_name==NULL){
        if(flag==0)
          which_files=NORMAL_FILES;
      }else
        which_files=SINGLE_FILE;
      cfs_ls(cfs_fd,&current_dir,file_name,which_files,how,detail,order,type);
  //-------------------CFS CP------------------------------
  //*******************************************************
    }else if(!strcmp(strtok_ptr,"cfs_cp")){
      strtok_ptr=strtok(NULL," ");
      int sum=0;
      char * start=strtok_ptr;
      while(strtok_ptr!=NULL){
        sum++;
        strtok_ptr=strtok(NULL," ");
      }
      //default parameters
      enum cp_options how=NON_REC;
      enum cp_options ask=DONTASK;
      if(sum<2){
        printf("Usage: cfs_cp <OPTIONS> <SOURCE> <DESTINATION>|<OPTIONS> <SOURCES> ... <DIRECTORY>\n");
        printf("Options: -i (questions asked)\n");
        printf("         -r (also copy all data contained in dir)\n");
        printf("Default behavior (no options specified): Don't ask questions and copy just the dirs inside the dir\n");
        printf("IMPORTANT: <DESTINATION> OR <DIRECTORY> MUST EXIST\n");
        continue;
      }
      char **files=malloc(sum*sizeof(char *));
      if(files==NULL){
        printf("Memory alloc problem for cfs_cp\n");
        continue;
      }
      files[0]=start;
      for(int i=1;i<sum;i++)
        files[i]=files[i-1]+strlen(files[i-1])+1;


      int flag[2]={0};
      int files_start=0;
      for(int i=0;i<2;i++)
        if(files[i][0]=='-')
          if(!flag[0] && !strcmp(files[i],"-i")){
            ask=ASK;
            files_start++;
            flag[0]=1;
          }else if(!flag[1] && !strcmp(files[i],"-r")){
            how=REC;
            files_start++;
            flag[1]=1;
          }else{
            break;
          }
          //printf("%d\n",sum);
          if(sum-files_start<2){
            //printf("--------------------------------\n");

            printf("Usage: cfs_cp <OPTIONS> <SOURCE> <DESTINATION>|<OPTIONS> <SOURCES> ... <DIRECTORY>\n");
            printf("Options: -i (questions asked)\n");
            printf("         -r (also copy all data contained in dir)\n");
            printf("Default behavior (no options specified): Ask questions and copy just the dirs inside the dir\n");
            printf("IMPORTANT: <DESTINATION> OR <DIRECTORY> MUST EXIST\n");
            free(files);
            continue;
          }
          int ret_val;
          ret_val=cfs_cp(cfs_fd,&current_dir,files+files_start,sum-files_start,how,ask);
          if(ret_val==1){
            printf("Output may not exist!\n");
            printf("If output exists then source file may not exist\n");
          }else if(ret_val==2){
            printf("Can't copy multiple entities in a regular file!\n");
          }else if(ret_val==3){
            printf("Can't copy dir in file!\n");
          }
          free(files);
  //---------------------CFS CAT------------------------------
  //**********************************************************
    }else if(!strcmp(strtok_ptr,"cfs_cat")){
      strtok_ptr=strtok(NULL," ");
      int sum=0;
      char * start=strtok_ptr;
      while(strtok_ptr!=NULL){
        sum++;
        strtok_ptr=strtok(NULL," ");
      }

      if(start==NULL || sum<3){
        printf("Usage: cfs_cat [files] -o [existing file|file_to_create]\n");
        continue;
      }

      char **files=malloc(sum*sizeof(char *));
      if(files==NULL){
        printf("Memory alloc problem for cfs_cat\n");
        continue;
      }
      files[0]=start;
      int output_index;
      for(int i=1;i<sum;i++){
        files[i]=files[i-1]+strlen(files[i-1])+1;
        if(!strcmp(files[i],"-o"))
          output_index=i+1;
      }
      if(output_index>=sum){
        printf("Usage: cfs_cat [files] -o [existing file|file_to_create]\nOutput file must be specified!\n");
        free(files);
        continue;
      }
      int ret_val;

      if((ret_val=cfs_cat(cfs_fd,&current_dir,files,sum-2))==1){
        printf("File %s doesn't exist!\n",files[output_index]);
      }else if(ret_val==2){//neeeds to be dooooooooooneeeeee
        printf("Size exceeded in %s!\n",files[output_index]);
      }
      free(files);
  //---------------------CFS LN---------------------------------
  //************************************************************
    }else if(!strcmp(strtok_ptr,"cfs_ln")){
  //---------------------CFS MV-----------------------------------
  //**************************************************************
    }else if(!strcmp(strtok_ptr,"cfs_mv")){
      strtok_ptr=strtok(NULL," ");
      int sum=0;
      char * start=strtok_ptr;
      while(strtok_ptr!=NULL){
        sum++;
        strtok_ptr=strtok(NULL," ");
      }
      //default parameters
      enum cp_options ask=DONTASK;
      if(sum<2){
        printf("Usage: cfs_mv <OPTIONS> <SOURCE> <DESTINATION>|<OPTIONS> <SOURCES> ... <DIRECTORY>\n");
        printf("Options: -i (questions asked)\n");
        printf("Default behavior (no options specified): Don't ask questions\n");
        printf("IMPORTANT: <DESTINATION> OR <DIRECTORY> MUST EXIST AND CAN ONLY BE DIRECTORIES\n");
        continue;
      }
      char **files=malloc(sum*sizeof(char *));
      if(files==NULL){
        printf("Memory alloc problem for cfs_mv\n");
        continue;
      }
      files[0]=start;
      for(int i=1;i<sum;i++)
        files[i]=files[i-1]+strlen(files[i-1])+1;


      int files_start=0;

      if(files[0][0]=='-')
        if(!strcmp(files[0],"-i")){
          ask=ASK;
          files_start++;
        }

          //printf("%d\n",sum);
      if(sum-files_start<2){
        printf("Usage: cfs_mv <OPTIONS> <SOURCE> <DESTINATION>|<OPTIONS> <SOURCES> ... <DIRECTORY>\n");
        printf("Options: -i (questions asked)\n");
        printf("Default behavior (no options specified): Don't ask questions\n");
        printf("IMPORTANT: <DESTINATION> OR <DIRECTORY> MUST EXIST AND CAN ONLY BE DIRECTORIES\n");
        free(files);
        continue;
      }
      int ret_val;
      ret_val=cfs_mv(cfs_fd,&current_dir,files+files_start,sum-files_start,ask);
      if(ret_val==1){
        printf("Destination doesn't exist!\n");
      }else if(ret_val==2){
        printf("Output_not a directory!\n");
      }else if(ret_val==3){
        printf("Output size exceeded!\n");
      }
      update_info(cfs_fd,&current_dir);
      free(files);
  //---------------------CFS RM-----------------------------------
  //**************************************************************
    }else if(!strcmp(strtok_ptr,"cfs_rm")){
      strtok_ptr=strtok(NULL," ");
      int sum=0;
      char * start=strtok_ptr;
      while(strtok_ptr!=NULL){
        sum++;
        strtok_ptr=strtok(NULL," ");
      }
      //default parameter
      enum cp_options ask=DONTASK;
      if(sum<1){
        printf("Usage: cfs_rm <OPTIONS> <DESTINATIONS>\n");
        printf("Options: -i (questions asked)\n");
        printf("Default behavior (no options specified): Don't ask questions and remove everything inside the dir if it's a dir\n");
        printf("IF -i IS SPECIFIED THEN USER CAN CHOOSE WHAT TO BE DELETED BY ANSWERING!\n");
        continue;
      }
      char **files=malloc(sum*sizeof(char *));
      if(files==NULL){
        printf("Memory alloc problem for cfs_rm\n");
        continue;
      }
      files[0]=start;
      for(int i=1;i<sum;i++)
        files[i]=files[i-1]+strlen(files[i-1])+1;


      int files_start=0;

      if(files[0][0]=='-')
        if(!strcmp(files[0],"-i")){
          ask=ASK;
          files_start++;
        }

      if(sum-files_start<1){
        printf("Usage: cfs_rm <OPTIONS> <DESTINATIONS>\n");
        printf("Options: -i (questions asked)\n");
        printf("Default behavior (no options specified): Don't ask questions and remove everything inside the dir if it's a dir\n");
        printf("IF -i IS SPECIFIED THEN USER CAN CHOOSE WHAT TO BE DELETED BY ANSWERING!\n");
        free(files);
        continue;
      }
      cfs_rm(cfs_fd,&current_dir,files+files_start,sum-files_start,ask);
      free(files);
  //---------------CFS IMPORT --------------------------------------
  //****************************************************************
    }else if(!strcmp(strtok_ptr,"cfs_import")){
      strtok_ptr=strtok(NULL," ");
      int sum=0;
      char * start=strtok_ptr;
      while(strtok_ptr!=NULL){
        sum++;
        strtok_ptr=strtok(NULL," ");
      }

      if(start==NULL || sum<3){
        printf("Usage: cfs_import [files|dirs] -o [Directory in cfs]\n");
        continue;
      }

      char **files=malloc(sum*sizeof(char *));
      if(files==NULL){
        printf("Memory alloc problem for cfs_import\n");
        continue;
      }
      files[0]=start;
      int output_index;
      for(int i=1;i<sum;i++){
        files[i]=files[i-1]+strlen(files[i-1])+1;
        if(!strcmp(files[i],"-o"))
          output_index=i+1;
      }
      if(output_index>=sum){
        printf("Usage: cfs_import [files|dirs] -o [Directory in cfs]\nDirectory must be specified!\n");
        free(files);
        continue;
      }
      int ret_val;
      for(int i=0;i<output_index-1;i++){
        if((ret_val=cfs_import(cfs_fd,&current_dir,files[i],files[output_index]))==1){
          printf("%s not a directory!\n",files[output_index]);
          break;
        }else if(ret_val==2){
          printf("Size exceeded in %s!\n",files[output_index]);
          break;
        }else if(ret_val==3){
          printf("Dir %s doesn't exist!\n",files[output_index]);
          break;
        }
        update_info(cfs_fd,&current_dir);//update cursor-> usefull if changes are made in current cursor path
      }
      free(files);
  //--------------CFS EXPORT-------------------------------------
  //*************************************************************
    }else if(!strcmp(strtok_ptr,"cfs_export")){
      strtok_ptr=strtok(NULL," ");
      int sum=0;
      char * start=strtok_ptr;
      while(strtok_ptr!=NULL){
        sum++;
        strtok_ptr=strtok(NULL," ");
      }

      if(start==NULL || sum<3){
        printf("Usage: cfs_export [files|dirs] -o [Directory in linux]\n");
        continue;
      }

      char **files=malloc(sum*sizeof(char *));
      if(files==NULL){
        printf("Memory alloc problem for cfs_import\n");
        continue;
      }
      files[0]=start;
      int output_index;
      for(int i=1;i<sum;i++){
        files[i]=files[i-1]+strlen(files[i-1])+1;
        if(!strcmp(files[i],"-o"))
          output_index=i+1;
      }
      if(output_index>=sum){
        printf("Usage: cfs_export [files|dirs] -o [Directory in cfs]\nDirectory must be specified!\n");
        free(files);
        continue;
      }
      int ret_val;
      for(int i=0;i<output_index-1;i++){
        if((ret_val=cfs_export(cfs_fd,&current_dir,files[i],files[output_index]))==1){
          printf("No such directory in linux fs (%s)!\n",files[output_index]);
          break;
        }else if(ret_val==2){
          printf("No such file/dir in cfs fs (%s)!\n",files[i]);
        }
      }
      free(files);
  //-----------------CFS EXIT---------------------------
  //****************************************************
    }else if(!strcmp(strtok_ptr,"cfs_exit")){
      break;
    }else{
      printf("No such operation '%s'\n",strtok_ptr);
    }

  }

  //print_inode(node);
  free_inode(node);
  cursor_free(&current_dir);
  close(cfs_fd);
  return 0;
}
