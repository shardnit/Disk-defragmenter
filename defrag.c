#include "defrag.h"
#include <string.h>
#include <assert.h>

_PROTOTYPE(int check_bit, (int number, int bit_pos));
_PROTOTYPE(int set_bit, (int number, int bit_pos));

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
unsigned int
num_set_bits(unsigned char *bitmap, int blocks)
{
	unsigned int    i, j, k;
	unsigned int    count = 0;
	for (i = 0; i < blocks; i++) {
		for (j = 1; j <= 8; j++) {
			if(check_bit(bitmap[i], j))
				count++;
		}
	}
	return count;
}
int
src_last_inode(unsigned char *bitmap, int blocks)
{
	int             i, j, k;
	int             inode_number = 0;
	for (i = 0; i < blocks; i++) {
		for (j = 1; i <= 8; j++) {
			if(check_bit(bitmap[i], j))
				inode_number = j + (8 * i);
		}
	}
	return inode_number;
}
int check_bit (int number, int bit_pos)
{
	int i = 1 << (bit_pos-1);
	if(number & i)
		return 1;
	else
		return 0;
}
int set_bit (int number, int bit_pos)
{
	int i = 1 << (bit_pos-1);
	return(number | i);
}
void read_block( int fd, int blk, unsigned char* buffer)
{
	long ret_value = lseek(fd, BLOCK_SIZE * blk, SEEK_SET);
	if(read(fd, buffer, BLOCK_SIZE) < BLOCK_SIZE)
		print_error("Error: ", "While reading block", ERR_GENERIC);
}
void write_block(int fd, int blk, unsigned char *buffer)
{
	long ret_value = lseek(fd, BLOCK_SIZE * blk, SEEK_SET);
	if(write(fd, buffer, BLOCK_SIZE) < BLOCK_SIZE)
		print_error("Error: ", "While writing block", ERR_GENERIC);
}
int direct_block_copy(int src_fs_fd, zone_t src_blk, int dst_fs_fd, zone_t dst_blk, struct super_block *src_sb, struct super_block *dst_sb)
{
	zone_t src_zone_no = src_blk - src_sb->s_firstdatazone + 1;

	/* now check whether corresponding bit in zonemap is true or not */
	if(!check_bit(src_zone_bitmap[src_zone_no/8], (src_zone_no - (8 * (src_zone_no/8)) +1)))
	{
		print_error("Source File System Inconsistent: ", "run fsck(1)", ERR_INCONSISTENT_SOURCE_FS);
	}

	/* now read the source block into buffer */
	read_block(src_fs_fd, src_blk, global_buffer);

	/* now set the appropriate bit in the zonemap of destination file system */
	dst_zone_bitmap[(written_blocks/8)] = set_bit(dst_zone_bitmap[written_blocks/8], (written_blocks - (8 * (written_blocks/8))+1));

	/* now write the block to destination file system */
	write_block(dst_fs_fd, dst_blk, global_buffer);

	return 1;
}
void first_indirect_block_copy(int src_fs_fd, zone_t src_blk, int dst_fs_fd, zone_t dst_blk, struct super_block *src_sb, struct super_block *dst_sb)
{
	int i, ret_value;
	zone_t src_zone_no = src_blk - src_sb->s_firstdatazone + 1;

	/* buffer for holding the indirect blocks */
	zone_t src_indirect_buf[V2_INDIRECTS(BLOCK_SIZE)], dst_indirect_buf[V2_INDIRECTS(BLOCK_SIZE)];

	/* now check whether corresponding bit in zonemap is true or not */
	if(!check_bit(src_zone_bitmap[src_zone_no/8], (src_zone_no - (8 * (src_zone_no/8)) +1)))
	{
	    print_error("Source File System Inconsistent: ", "run fsck(1)", ERR_INCONSISTENT_SOURCE_FS);
	}

	/* now read the source block into buffer */
	read_block(src_fs_fd, src_blk, global_buffer);

	/* copy the contents into source's indirect block buffer */
	memcpy(src_indirect_buf, global_buffer, BLOCK_SIZE);

	/* set the bit for block holding indirect block in destination zone bitmap*/
	dst_zone_bitmap[(written_blocks/8)] = set_bit(dst_zone_bitmap[written_blocks/8], (written_blocks - (8 * (written_blocks/8))+1));
	written_blocks++;

	/* copy the direct blocks pointed by indirect block and update the indirect buffer also */
	for(i=0; i< V2_INDIRECTS(BLOCK_SIZE); i++)
	{
		if(src_indirect_buf[i] != NO_ZONE)
		{
			dst_indirect_buf[i] = (dst_sb->s_firstdatazone+written_blocks-1);
			ret_value = direct_block_copy(src_fs_fd, src_indirect_buf[i], dst_fs_fd, dst_indirect_buf[i], src_sb, dst_sb);
			written_blocks = written_blocks + ret_value;
		}
	}

	assert(i== V2_INDIRECTS(BLOCK_SIZE));

	/* now write the block back */
	memcpy(global_buffer, dst_indirect_buf, BLOCK_SIZE);
	write_block(dst_fs_fd, dst_blk, global_buffer);
}
void double_indirect_block_copy(int src_fs_fd, zone_t src_blk, int dst_fs_fd, zone_t dst_blk, struct super_block *src_sb, struct super_block *dst_sb)
{
	int i;
	zone_t src_zone_no = src_blk - src_sb->s_firstdatazone + 1;

	/* buffer for holding the indirect blocks */
        zone_t src_indirect_buf[V2_INDIRECTS(BLOCK_SIZE)], dst_indirect_buf[V2_INDIRECTS(BLOCK_SIZE)];

	/* now check whether corresponding bit in zonemap is true or not */
	if(!check_bit(src_zone_bitmap[src_zone_no/8], (src_zone_no - (8 * (src_zone_no/8)) +1)))
	{
	    print_error("Source File System Inconsistent: ", "run fsck(1)", ERR_INCONSISTENT_SOURCE_FS);
	}

	/* now read the source block into buffer */
	read_block(src_fs_fd, src_blk, global_buffer);

	/* copy the contents into source's indirect block buffer */
	memcpy(src_indirect_buf, global_buffer, BLOCK_SIZE);

	/* set the bit for block holding indirect block in destination zone bitmap*/
	dst_zone_bitmap[(written_blocks/8)] = set_bit(dst_zone_bitmap[written_blocks/8], (written_blocks - (8 * (written_blocks/8))+1));
	written_blocks++;

	/* copy the direct blocks pointed by indirect block and update the indirect buffer also */
	for(i=0; i< V2_INDIRECTS(BLOCK_SIZE); i++)
	{
		if(src_indirect_buf[i] != NO_ZONE)
		{
			dst_indirect_buf[i] = (dst_sb->s_firstdatazone+written_blocks-1);
			first_indirect_block_copy(src_fs_fd, src_indirect_buf[i], dst_fs_fd, dst_indirect_buf[i], src_sb, dst_sb);
		}
	}

	assert(i== V2_INDIRECTS(BLOCK_SIZE));

	/* now write the block back */
	memcpy(global_buffer, dst_indirect_buf, BLOCK_SIZE);
	write_block(dst_fs_fd, dst_blk, global_buffer);
}
int
main(int argc, char **argv)
{
	int             src_fs_fd, dst_fs_fd;
	struct super_block *src_super_block, *dst_super_block;
	short           dst_imap_blocks, dst_zmap_blocks, src_imap_blocks,
	                src_zmap_blocks;
	unsigned int    inode_set_bits_count;
	unsigned int    zone_set_bits_count;
	long src_inode_fd, dst_inode_fd;
	d2_inode *src_inode, *dst_inode;
	int i, j, inode_num, ret_value;

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
	if ((src_super_block = (struct super_block *) malloc(SUPER_SIZE)) == NULL)
		print_error("Error while allocation memory: ", "malloc", ERR_GENERIC);
	if ((dst_super_block = (struct super_block *) malloc(SUPER_SIZE)) == NULL)
		print_error("Error while allocating memory: ", "malloc", ERR_GENERIC);
	/* validate the filesystems */
	validate_fs(src_fs_fd, src_super_block, src_file_system);
	validate_fs(dst_fs_fd, dst_super_block, dst_file_system);

	dst_imap_blocks = dst_super_block->s_imap_blocks;
	dst_zmap_blocks = dst_super_block->s_zmap_blocks;

	src_imap_blocks = src_super_block->s_imap_blocks;
	src_zmap_blocks = src_super_block->s_zmap_blocks;
#ifdef DEBUG
	printf("dst_inode_blocks: %d, dst_zone_blocks: %d\n", dst_imap_blocks, dst_zmap_blocks);
	printf("src_inode_blocks: %d, src_zone_blocks: %d\n", src_imap_blocks, src_zmap_blocks);
#endif
	/* read the inode bitmap from destination file system */
	dst_inode_bitmap = read_bitmap(dst_file_system, dst_fs_fd, BLOCK_SIZE * dst_imap_blocks);
	dst_zone_bitmap = read_bitmap(dst_file_system, dst_fs_fd, BLOCK_SIZE * dst_zmap_blocks);

	/* now check whether destination filesystem is empty or not */
	inode_set_bits_count = num_set_bits(dst_inode_bitmap, BLOCK_SIZE * dst_imap_blocks);
	zone_set_bits_count = num_set_bits(dst_zone_bitmap, BLOCK_SIZE * dst_zmap_blocks);

#ifdef DEBUG
	printf("inode_count %d zone_count %d\n", inode_set_bits_count, zone_set_bits_count);
#endif
	if (inode_set_bits_count > 2 || zone_set_bits_count > 2)
		print_error("Destination file system is not empty: ", dst_file_system, ERR_NON_EMPTY_TARGET_FS);

	/* store the inode position in destination file system */
	dst_inode_fd = lseek(dst_fs_fd, BLOCK_SIZE*(START_BLOCK+(dst_super_block->s_imap_blocks)+(dst_super_block->s_zmap_blocks)), SEEK_SET);

	dst_inode = (d2_inode *)malloc(sizeof(d2_inode));
	/*
	 * The size of source filesystem should not be more than destination
	 * file system
	 */

	/*
	 * comparing inodes 1. total number of inodes in source fs <= total
	 * inodes in destination fs OR 2. last inode number of source fs <=
	 * total inodes in destination. fs Not sure! (for time being
	 * considering second approach)
	 */

	src_inode_bitmap = read_bitmap(src_file_system, src_fs_fd, BLOCK_SIZE * src_imap_blocks);
	if (src_last_inode(src_inode_bitmap, BLOCK_SIZE * src_imap_blocks) > (8 * BLOCK_SIZE * (dst_super_block->s_imap_blocks))) {
#ifdef DEBUG
		printf("last inode in src: %d", src_last_inode(src_inode_bitmap, BLOCK_SIZE * src_imap_blocks));
		printf(" Dest inodes: %d\n", (8 * BLOCK_SIZE * (dst_super_block->s_imap_blocks)));
#endif
		print_error("Source file system larger than Destination FS:", dst_file_system, ERR_SMALLER_TARGET_FS);
	}
	/*
	 * comparing number of zones number of filled data zones in source FS
	 * less than or equal to data zone in dest FS = (s_zones -
	 * s_firstdatazone) << s_log_zone_size
	 */

	src_zone_bitmap = read_bitmap(src_file_system, src_fs_fd, BLOCK_SIZE * src_zmap_blocks);
	if (num_set_bits(src_zone_bitmap, BLOCK_SIZE * src_zmap_blocks) < (((dst_super_block->s_zones - dst_super_block->s_firstdatazone) << dst_super_block->s_log_zone_size) + 1)) {
#ifdef DEBUG
		printf("filled zones in src FS: %d", num_set_bits(src_zone_bitmap, BLOCK_SIZE * src_zmap_blocks));
		printf(" Dest Zones: %d\n", (((dst_super_block->s_zones - dst_super_block->s_firstdatazone) << dst_super_block->s_log_zone_size) + 1));
#endif
		print_error("Source file system larger than Destination FS:", dst_file_system, ERR_SMALLER_TARGET_FS);
	}
	
	/* store the inode position in source file system */
	src_inode_fd = lseek(src_fs_fd, BLOCK_SIZE*(START_BLOCK+(src_super_block->s_imap_blocks)+(src_super_block->s_zmap_blocks)), SEEK_SET);
	src_inode = (d2_inode *)malloc(sizeof(d2_inode));

	global_buffer = (unsigned char*)malloc(BLOCK_SIZE);

	/* Now start the actual work of copying */
	for(i=1; i<= src_super_block->s_ninodes; i++)
	{

			/* if this inode exists i.e. if this bit is *SET* */
			if(check_bit(src_inode_bitmap[(i+1)/8], (i+1)%8))
			{
				inode_num = i;

				/* now read this inode */
				if(lseek(src_fs_fd, src_inode_fd+(inode_num*INODE_SIZE), SEEK_SET)==-1)
					print_error("Unable to seek to inode: ", src_file_system, ERR_GENERIC);
				
				if(read(src_fs_fd, src_inode, INODE_SIZE) < INODE_SIZE)
					print_error("Unable to read inode: ", src_file_system, ERR_GENERIC);

				/* inode should be a regular file or directory */
				if(((src_inode->d2_mode & I_TYPE) == I_REGULAR) || ((src_inode->d2_mode & I_TYPE) == I_DIRECTORY))
				{
					/* copy inode info to destination file system inode */
					dst_inode->d2_mode = src_inode->d2_mode;
					dst_inode->d2_nlinks = src_inode->d2_nlinks;
					dst_inode->d2_uid = src_inode->d2_uid;
					dst_inode->d2_gid = src_inode->d2_gid;
					dst_inode->d2_size = src_inode->d2_size;
					dst_inode->d2_atime = src_inode->d2_atime;
					dst_inode->d2_ctime = src_inode->d2_ctime;
					dst_inode->d2_mtime = src_inode->d2_mtime;

					/* now copy the data BLOCKS! */
					
					/* first the direct blocks */
					for(j=0; j< V2_NR_DZONES; j++)
					{
						if(src_inode->d2_zone[j]!= NO_ZONE)
						{
							dst_inode->d2_zone[j]=dst_super_block->s_firstdatazone+written_blocks-1;
							ret_value = direct_block_copy(src_fs_fd, src_inode->d2_zone[j], dst_fs_fd, dst_inode->d2_zone[j], src_super_block, dst_super_block);
							written_blocks = written_blocks+ret_value ;
						}
					}
	
					assert (j==V2_NR_DZONES);

					/* copy first indirect block, if not empty */
					if(src_inode->d2_zone[V2_NR_DZONES] != NO_ZONE)
					{
						dst_inode->d2_zone[V2_NR_DZONES]=dst_super_block->s_firstdatazone+written_blocks-1;
						first_indirect_block_copy(src_fs_fd, src_inode->d2_zone[V2_NR_DZONES], dst_fs_fd, dst_inode->d2_zone[V2_NR_DZONES], src_super_block, dst_super_block);
					}

					/* copy the double indirect block, if not empty */
					if(src_inode->d2_zone[V2_NR_DZONES+1] != NO_ZONE)
					{
						dst_inode->d2_zone[V2_NR_DZONES+1]=dst_super_block->s_firstdatazone+written_blocks-1;
						double_indirect_block_copy(src_fs_fd, src_inode->d2_zone[V2_NR_DZONES+1], dst_fs_fd, dst_inode->d2_zone[V2_NR_DZONES+1], src_super_block, dst_super_block);
					}

				}/* regular file or directory checking ends here */

				else
					dst_inode = src_inode;

				/* now write the inode back to destination file system */
				lseek(dst_fs_fd, dst_inode_fd + (inode_num*INODE_SIZE), SEEK_SET);
				if(write(dst_fs_fd, dst_inode, INODE_SIZE) < INODE_SIZE)
					print_error("Error while writing inode: ", "destination FS", ERR_GENERIC);

				/* appropriately modify the destinations FS's inode bitmap */
				dst_inode_bitmap[(inode_num/8)] = set_bit((inode_num/8), (inode_num - (8 * (inode_num/8))+1)); 
			}
			/* else the inode is empty */
	}

	/* now write back the destination FS's zone bitmap and inode bitmap at appropriate location */
	lseek(dst_fs_fd, START_BLOCK*BLOCK_SIZE, SEEK_SET);
	if(write(dst_fs_fd, dst_inode_bitmap, (dst_super_block->s_imap_blocks)*BLOCK_SIZE) < (dst_super_block->s_imap_blocks)*BLOCK_SIZE)
		print_error("Error while writing inode bitmap: ", "dst_FS", ERR_GENERIC);

	if(write(dst_fs_fd, dst_zone_bitmap, (dst_super_block->s_zmap_blocks)*BLOCK_SIZE) < (dst_super_block->s_zmap_blocks)*BLOCK_SIZE)
		print_error("Error while writing zone bitmap: ", "dst_FS", ERR_GENERIC);

	close(src_fs_fd);
	close(dst_fs_fd);

	printf("Defragmented file system written to %s\n", dst_file_system);
	return 0;
}
