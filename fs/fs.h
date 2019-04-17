#define MAXIMUM_FILES			64
#define MAXIMUM_FILENAME_LENGTH		15
#define MAXIMUM_FILE_DESCRIPTORS	32

struct file_descriptor{
    bool free = true;               
    int file = -1;                 
    int offset = 0;
};

struct file_info {
    	int active;	
	char name[MAXIMUM_FILENAME_LENGTH+1];
   	int file_size;
    	int next_block;
    	int num_blocks;
	int num_fd;
	
	file_info() : name(""), file_size(0), next_block(-1) {
		num_blocks = 0;
		active = 0;
	}
};

struct super_block {
	int num_files;
	int num_fd;
	file_info directory[MAXIMUM_FILES];
	super_block() : num_files(0), num_fd(0) {}
};



int make_fs(char *disk_name);
int mount_fs(char *disk_name); // Open disk and load file information from super_block (block 0).
int umount_fs(char *disk_name); // Write super_block information to disk and close it.

// Helper functions.
int find_free_fd();
int search_file(char *name);
int find_next_block(int current_block);
int find_free_block();
int find_free_block(int current_block);
int clear_metadata(int current_block);

int fs_open(char *name);
int fs_close(int fildes);
int fs_create(char *name);
int fs_delete(char *name);
int fs_read(int fildes, void *buf, size_t nbyte);
int fs_write(int fildes, void *buf, size_t nbyte);
int fs_get_filesize(int fildes);
int fs_lseek(int fildes, off_t offset);
int fs_truncate(int fildes, off_t length);
