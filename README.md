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

