#include "defrag.h"
#include <string.h>

void 
print_error(char *str1, char *str2, int err_code)
{
	fprintf(stdout, "%s%s", str1, str2);
	if (err_code > 0)
		fprintf(stdout, ": %s\n", strerror(err_code));
	exit(err_code);
}

/* function to check filesystem for various pre-requisites */
void 
validate_fs(int fd, struct super_block * sb, char *fs)
{
	/*
	 * first read the super block so that we can read the SB
	 * datastructure
	 */

	/* seek by SUPER_BLOCK_BYTES: define in fs/const.h */
	if (lseek(fd, SUPER_BLOCK_BYTES, SEEK_SET) == -1)
		print_error(fs, " : Unable to seek to super block", ERR_GENERIC);

	/* now read the superblock */
	if (read(fd, sb, SUPER_BLOCK_BYTES) < SUPER_BLOCK_BYTES)
		print_error(fs, " : Unable to read the super block", ERR_GENERIC);

	/* now read the super block datastructure and check various values */

	/* check for V3 fs or SUPER_V3 0x4d5a */
	if (sb->s_magic != SUPER_V3)
		print_error(fs, " : Specified filesystem is MINIX V3 version", ERR_NOT_MINIX_V3_FS);

	/* check for block size */
	if (sb->s_block_size != BLOCK_SIZE) {
		/*
		 * print_error(fs, " : Block size is not 1KB",
		 * ERR_WRONG_BLOCK_SIZE);
		 */
	}
#ifdef DEBUG
		printf("block size %d\n", sb->s_block_size);
#endif
	/* check for byte ordering */
	if (sb->s_disk_version != 0)
		print_error(fs, " : Reverse byte ordering not supported", ERR_GENERIC);
}

/* function to read the inode and zone bitmaps */
unsigned char  *
read_bitmap(char *fs, int fd, int blocks)
{
	unsigned char  *bitmap = (unsigned char *) malloc(blocks);

	if ((read(fd, bitmap, blocks)) < blocks)
		print_error("Unable to read bitmap from :", fs, ERR_GENERIC);

	return bitmap;
}

/* function to check the number of *set* bits in given bitmap */
unsigned int num_set_bits (unsigned char* bitmap, int blocks)
{
 	unsigned int i,j,k;
	unsigned int count=0;
	for(i=0; i<blocks; i++)
	{
		for(j=1; j<=8; j++)
		{
			k=1<<(j-1);
			if(bitmap[i]&k == 0x0)
				count++;
		}
	}
}
int 
main(int argc, char **argv)
{
	int             src_fs_fd, dst_fs_fd;
	struct super_block *src_super_block, *dst_super_block;
	short           dst_inode_blocks, dst_zone_blocks, src_inode_blocks,
	                src_zone_blocks;
	unsigned char  *dst_inode_bitmap, *dst_zone_bitmap, *src_inode_bitmap,
	               *src_zone_bitmap;
	unsigned int set_bits_count;

	/* number of arguments provided should be exactly 3 */
	if (argc != 3) {
		print_error(argv[0], ": Invalid number of arguments", ERR_INVL_ARG);
	}
	src_file_system = argv[1];
	/* now open the source file system */
	if ((src_fs_fd = open(src_file_system, O_RDONLY)) == -1)
		print_error("Cannot open specified source filesystem: ", src_file_system, ERR_FILE_NOT_FOUND);

	dst_file_system = argv[2];
	/* open the destination file system */
	if ((dst_fs_fd = open(dst_file_system, O_RDWR)) == -1)
		print_error("Cannot open specififed destination filesystem: ", dst_file_system, ERR_FILE_NOT_FOUND);


	/* allocate the super block pointers */
	if ((src_super_block = (struct super_block *) malloc(sizeof(struct super_block))) == NULL)
		print_error("Error while allocation memory: ", "malloc", ERR_GENERIC);

	if ((dst_super_block = (struct super_block *) malloc(sizeof(struct super_block))) == NULL)
		print_error("Error while allocating memory: ", "malloc", ERR_GENERIC);

	/* validate the filesystems */
	validate_fs(src_fs_fd, src_super_block, src_file_system);
	validate_fs(dst_fs_fd, dst_super_block, dst_file_system);

	dst_inode_blocks = dst_super_block->s_imap_blocks;
	dst_zone_blocks = dst_super_block->s_zmap_blocks;

	src_inode_blocks = src_super_block->s_imap_blocks;
	src_zone_blocks = src_super_block->s_zmap_blocks;
#ifdef DEBUG
	printf("dst_inode_blocks: %d, dst_zone_blocks: %d\n", dst_inode_blocks, dst_zone_blocks);
	printf("src_inode_blocks: %d, src_zone_blocks: %d\n", src_inode_blocks, src_zone_blocks);
#endif
	/* read the inode bitmap from destination file system */
	dst_inode_bitmap = read_bitmap(dst_file_system, dst_fs_fd, BLOCK_SIZE * dst_inode_blocks);
	dst_zone_bitmap = read_bitmap(dst_file_system, dst_fs_fd, BLOCK_SIZE * dst_zone_blocks);

	inode_set_bits_count = num_set_bits(dst_inode_bitmap, BLOCK_SIZE*dst_inode_blocks);
	zone_set_bits_count  = num_set_bits(dst_zone_bitmap, BLOCK_SIZE*dst_zone_blocks);

#ifdef DEBUG
	printf("inode_count %d zone_count %d\n",inode_set_bits_count, zone_set_bits_count);
#endif
	/* now check whether destination filesystem is empty or not */
	close(src_fs_fd);
	close(dst_fs_fd);
	return 0;
}
