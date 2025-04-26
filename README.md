# FAT32 File System Utility

For this project, the group must focus on working with a basic file-system, how to implement capabilities, and understanding how the FAT32 file system works. Utilizing the FAT32 system, we must create a shell-like system that allows the user to interact with the file system, giving them the ability to manipulate it without corrupting it.

## Group Members
- **Isabella Baez**: igb22b@fsu.edu
- **Jordan Freyman**: js19@fsu.edu
- **Fiana Frukhtman**: fmf21@fsu.edu
## Division of Labor

### Part 1: Mounting the Image
- **Responsibilities**: For this part, the user will need to mount the file system using commandline arguments when running the program, allowing for opening and closing the file system, as well as being able to dump information important to the system.
- **Assigned to**: Isabella Baez, Jordan Freyman, Fiana Frukhtman

### Part 2: Navigation
- **Responsibilities**: For this part, we must implement two basic navigation functions: cd and ls. CD must changed the current working directory to the one specified while ls must print all the files in the current working directory.
- **Assigned to**: Isabella Baez, Jordan Freyman, Fiana Frukhtman

### Part 3: Create
- **Responsibilities**: For this part, we must implement two creation commands: mkdir and creat. They will need to create a new directory and a new file respectively in the current working directory.
- **Assigned to**: Jordan Freyman, Fiana Frukhtman, Isabella Baez

### Part 4: Read
- **Responsibilities**: For this part, we need to implement four read commands: open, close, lsof, lseek, and read. Open and Close go together, allowing for the user to open and close a specific file. Additionally, lsof lists all of the currently opened files. Lseek sets the offset (in bytes) of a specified file for further reading or writing. Finally, read allows a user to read the data from a file in the current working directory with the specified file name.
- **Assigned to**: Jordan Freyman, Fiana Frukhtman

### Part 5: Update
- **Responsibilities**: For this part, we need to implement two update commands: write and mv. For the write command, a user needs to be able to write to a specified file with the given string. The mv command should allow a user to move a file or directory into either a new directory or be used as a tool to rename the file.
- **Assigned to**: Jordan Freyman, Fiana Frukhtman

## File Listing
```
filesys/
│
├── src/
│ ├── lexer.c
│ ├── fat32.c
│ └── filesys.c
│
├── obj/
│ ├── lexer.o
│ ├── fat32.o
│ └── filesys.o
|
├── include/
│ └── lexer.h
│ └── fat32.h
│
├── README.md
└── Makefile
```
## How to Compile & Execute

### Requirements
- **Compiler**: `gcc` for C/C++

### Compilation
For a C/C++ example:
```bash
make
```
This will build the executable in the "bin" folder
### Execution
```bash
make run
```
This will run the program

## Bugs
- **Bug 1**: ...

## Considerations
This project assumes that the FAT32 system is already downloaded on a user's system. Additionally, we assume that no more than 10 files will be opened at a given time, as well as that a file name will be no more than 11 characters long. A STRING used with the write command will always be enclosed with quotation marks.
