#include "stdio.h"
#include <cstring>
#include <iostream>
#include <cstdlib>
#include "disk.h"
#include "fs.h"

super_block *super_block_ptr;
file_descriptor file_descriptors[MAXIMUM_FILE_DESCRIPTORS];

int make_fs(char *disk_name) {
	if (make_disk(disk_name) == -1) return -1;
	if (open_disk(disk_name) == -1) return -1;

	super_block *super_block_tmp = new super_block();
	if (super_block_tmp == nullptr) return -1;

    // The first block of the disk contains file information listed below.
    // The data for each file is stored in 36 byte chunks.
	char buf[BLOCK_SIZE];
	int *buf_ptr;
	memset(buf, 0, BLOCK_SIZE);
	buf_ptr = (int*)(&buf[0]);
	*(buf_ptr++) = super_block_tmp->num_files;
	*(buf_ptr++) = super_block_tmp->num_fd;
	for (int i = 0; i < MAXIMUM_FILES; ++i) {
		*(buf_ptr++) = 0; // active
		*(buf_ptr++) = 0; // file size
		*(buf_ptr++) = -1; //next block
		*(buf_ptr++) = 0; // num blocks
		snprintf(&buf[(24+(32*i))], MAXIMUM_FILENAME_LENGTH+1, "000000000000000");
		buf_ptr += 4;
	}
	if (block_write(0, buf) == -1) return -1;
	delete super_block_tmp;
	close_disk();

	return 0;
}

int mount_fs(char *disk_name) {
	if (disk_name == nullptr) return -1;
	if (open_disk(disk_name) == -1) return -1;

	super_block_ptr = new super_block();

	char buf[BLOCK_SIZE];
	memset(buf, 0, BLOCK_SIZE);
	block_read(0, buf);
	int* buf_ptr = (int*)(&buf);
	super_block_ptr->num_files = *(buf_ptr++);
	super_block_ptr->num_fd = *(buf_ptr++);
	for (int i = 0; i < MAXIMUM_FILES; ++i) {
		super_block_ptr->directory[i].active = *(buf_ptr++);
		super_block_ptr->directory[i].file_size = *(buf_ptr++);
		super_block_ptr->directory[i].next_block = *(buf_ptr++);
		super_block_ptr->directory[i].num_blocks = *(buf_ptr++);
		super_block_ptr->directory[i].num_fd = 0;
		snprintf(super_block_ptr->directory[i].name, MAXIMUM_FILENAME_LENGTH+1, "%s", buf+(24+(32*i)));
		buf_ptr += 4;
	}

    // Reset state of all file descriptors.
	for (int i = 0; i < MAXIMUM_FILE_DESCRIPTORS; ++i) {
		file_descriptors[i].free= true;
		file_descriptors[i].file = -1;
		file_descriptors[i].offset = 0;
	}

	return 0;
}

int umount_fs(char *disk_name) {
	if (disk_name == nullptr) return -1;
	char buf[BLOCK_SIZE];
	int *buf_ptr;

	memset(buf, 0, BLOCK_SIZE);
    if (block_read(0, buf) == -1) return -1;
    memset(buf, 0, BLOCK_SIZE);

	buf_ptr = (int*)(&buf);
	*(buf_ptr++) = super_block_ptr->num_files;
	*(buf_ptr++) = 0; // num_fd
	for (int i = 0; i < MAXIMUM_FILES; ++i) {
		*(buf_ptr++) = super_block_ptr->directory[i].active; // active
		*(buf_ptr++) = super_block_ptr->directory[i].file_size; // file size
		*(buf_ptr++) = super_block_ptr->directory[i].next_block; //next block
		*(buf_ptr++) = super_block_ptr->directory[i].num_blocks; // num blocks
		snprintf(buf+(24+(31*i)), MAXIMUM_FILENAME_LENGTH+1, "%s", super_block_ptr->directory[i].name);
		buf_ptr += 4;
	}
	block_write(0, buf);

	for (int i = 0; i < MAXIMUM_FILE_DESCRIPTORS; ++i) {
		file_descriptors[i].free= true;
		file_descriptors[i].file = -1;
		file_descriptors[i].offset = 0;
	}
	delete super_block_ptr;
	close_disk();
	return 0;
}

int fs_open(char *name) {
	int file_index = search_file(name);
	if (file_index == -1) return -1;
	if (super_block_ptr->num_fd >= 32) return -1;
	int fd = find_free_fd();
	if (fd != -1) {
		super_block_ptr->directory[file_index].num_fd++;
		file_descriptors[fd].file = file_index;
		++super_block_ptr->num_fd;
		return fd;
	}
	return -1;	
}

int find_free_fd() {
	for (int i = 0; i < MAXIMUM_FILE_DESCRIPTORS; ++i) {

		if (file_descriptors[i].free) {
			file_descriptors[i].free = false;
			file_descriptors[i].offset = 0;
			return i;
		}
	}
	return -1;
}

// Traverse all files in loaded directory to find a file with matching name.
// For a larger use case could sort directory alphabetically by name or load file info into a hashmap.
int search_file(char *name) {
	for (int i = 0; i < MAXIMUM_FILES; ++i) {
		if (strcmp(super_block_ptr->directory[i].name, name) == 0) {
			return i;
		}
	}
	return -1;
}

int fs_close(int fildes) {
	if (fildes < 0 || fildes >= MAXIMUM_FILE_DESCRIPTORS || file_descriptors[fildes].free) return -1;
	--super_block_ptr->num_fd;
	file_descriptors[fildes].free = true;
	super_block_ptr->directory[file_descriptors[fildes].file].num_fd--;
	file_descriptors[fildes].file = -1;
	file_descriptors[fildes].offset = 0;
	return 0;
}

int fs_create(char *name) {
	int file_index = search_file(name);
	if (file_index != -1 || strlen(name) > (MAXIMUM_FILENAME_LENGTH) || strlen(name) == 0 || super_block_ptr->num_files >= MAXIMUM_FILES) return -1;
	for (int i = 0; i < MAXIMUM_FILES; ++i) {
		if (super_block_ptr->directory[i].active == 0) {
			++super_block_ptr->num_files;
			super_block_ptr->directory[i].active = 1;
			super_block_ptr->directory[i].file_size = 0;
			super_block_ptr->directory[i].next_block = -1;
			super_block_ptr->directory[i].num_blocks = 0;
			super_block_ptr->directory[i].num_fd = 0;
			snprintf(super_block_ptr->directory[i].name, MAXIMUM_FILENAME_LENGTH+1, "%s", name);
			return 0;
		}
	}
	return -1;
}

int fs_write(int fildes, void *buf, size_t nbyte) {
	if (fildes < 0 || fildes >= MAXIMUM_FILE_DESCRIPTORS || nbyte < 0 || file_descriptors[fildes].free) {
		return -1;
	}
	if (nbyte == 0) return 0;
	/*
	   for (int i = 0; i < MAXIMUM_FILES; ++i) {
	   if (super_block_ptr->directory[i].active) n_free_bytes += super_block_ptr->directory[i].file_size;
	   }
	   n_free_bytes = (4096*4096) - n_free_bytes;
	   if (n_free_bytes == 0) return 0;
	   */
	int n_bytes_written = nbyte;
	int offset = file_descriptors[fildes].offset;
	int file_index = file_descriptors[fildes].file;

	file_info *file = &(super_block_ptr->directory[file_index]);


	int block_to_write = file->next_block;
	int last_set = file->file_size;
    
    // If no data has been stored from this file, find and allocate a data block for it.
	if (block_to_write == -1) {
		file->next_block = find_free_block();
		block_to_write = file->next_block;
		if (file->next_block == -1) {
			return 0;
		}
		file->num_blocks++;
	}

	if (offset >= (4096*4096+1)) return 0;

	while (offset >= BLOCK_SIZE) {
		int tmp_block = find_next_block(block_to_write);
		offset -= BLOCK_SIZE;
		if (tmp_block == -1) {
			tmp_block = find_free_block(block_to_write);
			if (tmp_block == -1) return 0;
			file->num_blocks++;
		}
		block_to_write = tmp_block;
	}
	char *current_buf_index = (char*)buf;
	while (n_bytes_written > 0) {

		char block[BLOCK_SIZE];
		memset(block, 0, BLOCK_SIZE);
		block_read(block_to_write, block);
		for (int i = offset; i < BLOCK_SIZE; ++i) {
			if (n_bytes_written == 0) break;
			block[i] = *(current_buf_index++);
            ++file_descriptors[fildes].offset;
			--n_bytes_written;
			if (last_set > file_descriptors[fildes].offset) last_set--;
			else {
				file->file_size++;
			}
		}

		block_write(block_to_write, block);
		offset = 0;

		if (n_bytes_written > 0) {
			int block_tmp = find_next_block(block_to_write);
			if (block_tmp == -1) {
				block_tmp = find_free_block(block_to_write);
				if (block_tmp == -1) break;
				file->num_blocks++;
			}
			block_to_write = block_tmp;
		}
	}

	return (nbyte - n_bytes_written);

}

int find_next_block(int current_block) {
	char buf[BLOCK_SIZE];
	int *buf_ptr;
	memset(buf, 0, BLOCK_SIZE);
	block_read((current_block-5)/1024+1, buf);
	buf_ptr = (int*)(&buf);
	buf_ptr += (current_block-5)%1024;
	return *buf_ptr;
}

// Data blocks are accessed from block 5 onwards.
int find_free_block() {
	for (int i = 0; i < 4; ++i) {
		char buf[BLOCK_SIZE];
		int *buf_ptr;
		memset(buf, 0, BLOCK_SIZE);
		block_read(i+1, buf);
		buf_ptr = (int*)(&buf[0]);
		for (int j = 1; j <= 1024; ++j) {
			if (*buf_ptr == 0) {
				*buf_ptr = -1;
				block_write(i+1, buf);
				return (1024*i + j + 4);
			}
			buf_ptr++;
		}
	}
	return -1;
}

int find_free_block(int current_block) {
	for (int i = 0; i < 4; ++i) {
		char buf[BLOCK_SIZE];
		int *buf_ptr;
		memset(buf, 0, BLOCK_SIZE);
		block_read(i+1, buf);
		buf_ptr = (int*)(&buf[0]);
		for (int j = 1; j <= 1024; ++j) {
			if (*buf_ptr == 0) {	
				*buf_ptr = -1;
				block_write(i+1, buf);
				memset(buf, 0, BLOCK_SIZE);
				block_read((current_block-5)/1024+1, buf);
				buf_ptr = (int*)(&buf);
				buf_ptr += (current_block-5)%1024;

				*buf_ptr = (1024*i + j + 4);

				block_write((current_block-5)/1024+1, buf);
				return (1024*i + j + 4);
			}
			buf_ptr++;
		}
	}
	return -1;
}

int fs_delete(char *name) {
	int file_index = search_file(name);
	if (file_index == -1 || super_block_ptr->directory[file_index].num_fd != 0) return -1;

	int block_index = super_block_ptr->directory[file_index].next_block;
	while (block_index != -1) {
		char buf[BLOCK_SIZE];
		memset(buf, 0, BLOCK_SIZE);
		block_write(block_index, buf);
		block_index = clear_metadata(block_index);
	}

	super_block_ptr->num_files--;

	super_block_ptr->directory[file_index].active = 0;
	memset(super_block_ptr->directory[file_index].name, 0, MAXIMUM_FILENAME_LENGTH+1);
	super_block_ptr->directory[file_index].file_size = 0;
	super_block_ptr->directory[file_index].next_block = -1;
	super_block_ptr->directory[file_index].num_blocks = 0;
	super_block_ptr->directory[file_index].num_fd = 0;

	return 0;
}

int clear_metadata(int current_block) {
	char buf[BLOCK_SIZE];
	int *buf_ptr;
	memset(buf, 0, BLOCK_SIZE);
	block_read((current_block-5)/1024+1, buf);
	buf_ptr = (int*)(&buf[0]);
	buf_ptr += (current_block-5)%1024;
	int next_block = *buf_ptr;
	*buf_ptr = 0;
	block_write((current_block-5)/1024+1, buf);
	return next_block;
}

int fs_read(int fildes, void *buf, size_t nbyte) {
	if (fildes < 0 || fildes >= MAXIMUM_FILE_DESCRIPTORS || file_descriptors[fildes].free) return -1;

	int n_bytes_read = nbyte;
	int offset = file_descriptors[fildes].offset;
	int file_index = file_descriptors[fildes].file;

	file_info *file = &(super_block_ptr->directory[file_index]);

	int block_to_read = file->next_block;
	int file_size = file->file_size;

	if (block_to_read == -1) return 0;
	if (offset >= (4096*4096)) return 0;

	while (offset >= BLOCK_SIZE) {
		int tmp_block = find_next_block(block_to_read);
		offset -= BLOCK_SIZE;
		if (tmp_block == -1) return 0;
		block_to_read = tmp_block;
	}
	char *current_buf_index = (char*)buf;

	while (n_bytes_read > 0 && file_descriptors[fildes].offset < file_size) {
		char block[BLOCK_SIZE];
		memset(block, 0, BLOCK_SIZE);
		block_read(block_to_read, block);
		for (int i = offset; i < BLOCK_SIZE; ++i) {
			if (file_descriptors[fildes].offset >= file->file_size) {
				return nbyte-n_bytes_read;
			}
			if (n_bytes_read == 0) break;
			*(current_buf_index++) = block[i];
			--n_bytes_read;
			file_descriptors[fildes].offset++;
		}

		offset = 0;
		if (file_descriptors[fildes].offset >= file->file_size) break;
		if (n_bytes_read > 0) {
			block_to_read = find_next_block(block_to_read);
			if (block_to_read == -1) break;
		}
	}
	return (nbyte - n_bytes_read);
}

int fs_get_filesize(int fildes) {
	if (fildes < 0 || fildes >= MAXIMUM_FILE_DESCRIPTORS || file_descriptors[fildes].free) return -1;

	return (super_block_ptr->directory[file_descriptors[fildes].file]).file_size;
}

int fs_lseek(int fildes, off_t offset) {
	if (fildes < 0 || fildes >= MAXIMUM_FILE_DESCRIPTORS || file_descriptors[fildes].free) return -1;

	int file_index = file_descriptors[fildes].file;
	file_info *file = &(super_block_ptr->directory[file_index]);

	if (offset < 0 || offset > file->file_size) return -1;

	file_descriptors[fildes].offset = offset;
	return 0;
}
int fs_truncate(int fildes, off_t length) {
	if (fildes < 0 || fildes >= MAXIMUM_FILE_DESCRIPTORS || file_descriptors[fildes].free) return -1;
	int file_index = file_descriptors[fildes].file;
	file_info *file = &(super_block_ptr->directory[file_index]);
	if (length > file->file_size || length < 0) return -1;

	int offset = length;
	int block_to_truncate = file->next_block;

	while (offset >= BLOCK_SIZE) {
		int tmp_block = find_next_block(block_to_truncate);
		offset -= BLOCK_SIZE;
		if (tmp_block == -1) return -1;
		block_to_truncate = tmp_block;
	}

	while (block_to_truncate != -1) {
		char block[BLOCK_SIZE];
		memset(block, 0, BLOCK_SIZE);
		block_read(block_to_truncate, block);
		if (offset > 0) {
			for (int i = offset; i < BLOCK_SIZE; ++i) {
				block[i] = '\0';
			}
		}
		else {
			for (int i = 0; i < BLOCK_SIZE; ++i) {
                                block[i] = '\0';
                        }
		}
		
		block_write(block_to_truncate, block);
		int tmp_block = block_to_truncate;
		block_to_truncate = find_next_block(block_to_truncate);
		
		if (offset > 0) {
			char buf[BLOCK_SIZE];
			int *buf_ptr;
			memset(buf, 0, BLOCK_SIZE);
			block_read((tmp_block-5)/1024+1, buf);
			buf_ptr = (int*)(&buf[0]);
			buf_ptr += (tmp_block-5)%1024;
			*buf_ptr = -1;
			block_write((tmp_block-5)/1024+1, buf);
		}
		else {
			if (offset == 0) {
				file->next_block = -1;
			}
			file->num_blocks--;
			clear_metadata(tmp_block);
		}

		offset = -1;
	}

	file->file_size = length;
	if (file_descriptors[fildes].offset > file->file_size) file_descriptors[fildes].offset = file->file_size;

	return 0;
}
