#include <sys/types.h>
#include <sys/dir.h>
#include <sys/stat.h>
#include <limits.h>
#include <errno.h>
#include <time.h> /* ctime et al */
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
/* Regular minix headers */
#include <minix/config.h>
#include <minix/const.h>
#include <minix/type.h>

/* Include minix fs-headers for definitions of various fs datastructures */
#undef EXTERN
#define EXTERN /* get rid of extern by making it NIL */
/* Avoid problems defining printf (it is defined to printk in const.h) */
#undef printf
#include "/usr/src/servers/fs/const.h"
#include "/usr/src/servers/fs/type.h"
#include "/usr/src/servers/fs/super.h"
#include "/usr/src/servers/fs/inode.h"

#undef printf /* define printf to normal printf */ 
#include <stdio.h>

/* Various constants */
#define BLOCK_SIZE 1024 /* block size = zone size = 1KB */
#define ZONE_SIZE 1024
#define INODE_SIZE 64 

/* Error Return Values */
#define ERR_INVL_ARG 1
#define ERR_FILE_NOT_FOUND 2
#define ERR_NOT_MINIX_V3_FS 3
#define ERR_WRONG_BLOCK_SIZE 4 
#define ERR_NON_EMPTY_TARGET_FS 5
#define ERR_SMALLER_TARGET_FS 6
#define ERR_INCONSISTENT_SOURCE_FS 7
#define ERR_GENERIC 8

/* Global values */
char *src_file_system, *dst_file_system;
