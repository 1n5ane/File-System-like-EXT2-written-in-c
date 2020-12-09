#ifndef __INPUT_CHECK_H__
#define __INPUT_CHECK_H__
int cfs_workwith_check(char * buff);
int cfs_touch_check(char * buff);
int cfs_ls_check(char * buff);
int cfs_cp_check(char * buff);
int cfs_cat_check(char * buff);
int cfs_ln_check(char * buff);
int cfs_mv_check(char * buff);
int cfs_import_check(char * buff);
int cfs_export_check(char * buff);
int cfs_create_check(char * buff);
int check_if_number(char * buff);
int cfs_ls_check_options(char * options);
#endif
