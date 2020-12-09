#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "input_check.h"


int check_if_number(char * buff){
  for(int i=0;i<strlen(buff);i++)
    if(buff[i]<'0' || buff[i]>'9')
      return 0;
  return 1;
}

int cfs_workwith_check(char * buff){
  const char s[2] = " ";
  char *token;

  int count=1;
  token=strtok(buff,s);
   /* walk through other tokens */
  while( token != NULL ) {
    if(token[0]!=' ' && token[0]!='\n'){
      count++;
    }
    token=strtok(NULL,s);
  }
  if(count==2) return 0;
  return 1;
}

int cfs_touch_check(char * buff){
  const char s[2] = " ";
  char *token;
   /* get the first token */
  token = strtok(buff, s);
  int count=1;

  token=strtok(NULL,s);
  printf("%d\n",token[2]);
  while(token!=NULL || token[0]==' ' || token[0]=='\n'){

    if(memcmp(token,"-a",2) && memcmp(token,"-m",2)){
      //printf("%s\n",token);
      return 1;
    }else{
      count++;
      token=strtok(NULL,s);//twra to token periexei to onoma tou file
      if(token==NULL || token[0]=='\n' || token[0]==' '){
        return 1;
      }
      count++;
    }
    strtok(NULL,s);
  }
  if(count%2==0)
    return 1;
  return 0;
}
int cfs_ls_check(char * buff);
int cfs_cp_check(char * buff);
int cfs_cat_check(char * buff);
int cfs_ln_check(char * buff);
int cfs_mv_check(char * buff);
int cfs_import_check(char * buff);
int cfs_export_check(char * buff);
int cfs_create_check(char * buff){
  const char s[2] = " ";
  char *token;
  int count=1;
  int flag[3]={0};
  token=strtok(buff,s);
  while(token!=NULL){
    if(!flag[0] && !strcmp(token,"-bs")){
      count++;
      flag[0]=1;
    }else if(!flag[1] && !strcmp(token,"-fns")){
      count++;
      flag[1]=1;
    }else if(!flag[2] && !strcmp(token,"-cfs")){
      count++;
      flag[2]=1;
    }else{
      if(flag[0] && flag[1] && flag[2])
        return 0;
      return 1;
    }
    token=strtok(NULL,s);
    if(check_if_number(token)==0)
      return 1;
    count++;
    token=strtok(NULL,s);
  }
  if(count==8)
    return 0;
}

int cfs_ls_check_options(char * options){

}
