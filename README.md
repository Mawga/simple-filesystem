# Maga Kim
24 April 2019

## Project Details
The super block holds data such as the number of files, number of active file descriptors, and a file directory.

The file directory contains information on all files:

    Whether it is active
    Name
    The value of the head block
    The number of blocks in the file
    The number of file descriptors accessing it
    
File descriptors have member variables:

    free
    file directory index
    and offset

## Restrictions
Restrictions to this project include:

A virtual disk of 8192 blocks, each block with a capacity of 4KB.

No more than 64 files on a disk.

No more than 32 file descriptors.

A maximum file size of 16 MB.

A maximum of 16 MB of data on the disk (4096 blocks).

## Implementation
I created a disk in which the first block (0 on disk) holds directory/super block information and is loaded/saved on mount/umount.

The second-fourth blocks (1-5) contain metadata of the next block for each file.

The metadata blocks contain contiguous integer values which correspond to the next block value (integer value of block on disk).

Mounting this disk requires the accessing the 0th block with int pointers and accessing metadata is done in the same way.

I created helper functions:

    int find_free_fd();
    int search_file(char *name);
    int find_next_block(int current_block);
    int find_free_block();
    int find_free_block(int current_block);
    int clear_metadata(int current_block);
    
For the purpose of the implemented functions.

find_free_block() causes the metadata of the free block to point to -1.

find_free_block(int current_block) will also change the metadata of current_block to have the value of this next free block.

clear_metadata clears the metadata a certain block and will return the next block in the chain.

## fs.cc
    fs_open:
        Search for file_name and if not found and not at max file descriptors find and assign free fd.
        If a fd is free then update the directory for the file_name in question increasing its num_fd.
        Increment total fd and fd associated with the file.

    fs_close:
        Free the associated fd, decrease number of total fd and fd associated with the file.

    fs_create:
        Check if maximum number of files is reached or if file_name is invalid.
        Create an instance of the file within the directory pointed to by the super_block_ptr.

    fs_delete:
        Check for valid file_name and make sure it has no open fds.
        Clear all associated metadata and directory information.

    fs_write:
        Check for valid fd. 
        Find block the offset is referring to, if no block then assign one.
        Fill a block sized buffer with information byte by byte and write. 
        Update file_size on each byte if writing past previous file_size.
        Assign a new block and continue if necessary.
        Exit when nbytes have been written or number of free blocks has run out.

    fs_read:
        Read byte by byte and exit if offset is at file_size or nbytes have been read.

    fs_get_filesize:
        Return relevant filesize from directory.

    fs_lseek:
        update offset of fd if fd is valid and offset is valid (not < 0  and not > file_size)

    fs_truncate:
        If valid length begins rewriting associated blocks byte by byte beginning at length to '\0'
        Clear metadata as necessary and assign new file_size.
