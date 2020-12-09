#include <stdio.h>
#include <string.h>
#include "sort.h"

void swap_ints(int * x,int * y){
  int tmp;
  tmp=*x;
  *x=*y;
  *y=tmp;
  return;
}

void swap_strings(char **x,char **y){
  char * tmp;
  tmp=*x;
  *x=*y;
  *y=tmp;
  return ;
}
//phgh geeks4geeks

void sort(char **names,int * arr_ids,int low,int high){//1st int low,2nd int high,3rd int column to be compared
  low--;//for indexing purposes
  high--;//for indexing purposes
  int pi;
  if (low < high){
        /* pi is partitioning index, arr[pi] is now
           at right place */
        pi = partition(names,arr_ids,low, high);

        sort(names,arr_ids,low+1, pi);  // Before pi
        sort(names,arr_ids,pi + 2, high+1); // After pi
  }
  return ;
}

int partition(char ** names,int * arr_ids,int low,int high){//same
  char * pivot = names[high];    // pivot

  int i = (low - 1);  // Index of smaller element

  for (int j = low; j <= high- 1; j++){
          // If current element is smaller than the pivot
      if (strcmp(names[j],pivot)<0){
          i++;    // increment index of smaller element
          swap_strings(&names[i], &names[j]);
          swap_ints(&arr_ids[i],&arr_ids[j]);
      }
  }
  swap_strings(&names[i + 1], &names[high]);
  swap_ints(&arr_ids[i + 1], &arr_ids[high]);

  return (i + 1);

}
