# Secfs
Secfs is a secure file system built on top of FUSE. When running, it will create a virtual folder where all the files will be stored encrypted.

For more information, please check the [design document](design-document.pdf).

## Requirements
* Unix based OS. Developed and tested on Ubuntu 20.04 (macOS not supported)
* Fuse3 (install with `sudo apt install fuse3` on debian based systems)

## Build

To build the software simply run `make` from the root directory. A binary under the name `secfs` will be created.

## Run

Run example: `./secfs <data directory> <mount point>`
- Data directory is where all the encrypted binary data will be stored on disk.
- Mount point is the directory where to mount the virtual decrypted folder.

### First run
In the first run, you will be asked to create a unique password. This password will not be stored anywhere and meant to be secret. If you forget your password, you will lose access to all the encrypted files.

### Stoping the software

There are two options to stop the software:

1. Manually unmount the mounted secfs driver
2. Stop the program runing in the terminal

After stopping the program, all files will remain encrypted and it will be impossible to read the data without running the software again.
