# MiniVSFS: A C-based VSFS Image Generator

A very small file system for operating system. Done in CSE 321 : Operating System in Brac Universitty

# Introduction 

We will implement mkfs_builder, a C program that creates a raw disk image for a
small, inode-based file system MiniVSFS. The program takes some parameters from
the command line input, and emits a byte-exact image as a binary .img file.
You will also implement mkfs_adder, another C program that takes a raw MiniVSFS
file system image, and a file to be added to that file system. mkfs_adder will find the
file in the working directory and add it to the root directory (/) of the file system.
Finally, it will save the output image in the new file specified by the output flag.
To understand how to start working on the project, refer to the Project Starting Point
section.

# MiniVSFS
MiniVSFS, based on VSFS, is fairly simple – a block-based file system structure with a
superblock, inode and data bitmaps, inode tables, and data blocks. Compared to
the regular VSFS, MiniVSFS cuts a few corners:
●​ Indirect pointer mechanism is not implemented
●​ Only supported directory is the root (/) directory
●​ Only one block each for the inode and data bitmap
●​ Limited size and inode count

# What You’ll Build
# MKFS_BUILDER

mkfs_builder \​
--image out.img \​
--size-kib <180..4096> \​
--inodes <128..512>

●​ image: the name of the output image
●​ size-kib: the total size of the image in kilobytes (multiple of 4)
●​ inodes: number of inodes in the file system

# MKFS_ADDER

mkfs_adder \​
--input out.img
--output out2.img​
--file <file>

●​ input: the name of the input image
●​ output: name of the output image
●​ file: the file to be added to the file system

# Output
●​ the updated output binary image with the file added

