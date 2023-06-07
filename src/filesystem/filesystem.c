//
//  Created by Stasel
//

#define FUSE_USE_VERSION 31

#include <stdio.h>
#include <fuse.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <pthread.h>
#include "filesystem.h"
#include "../utilities/utilities.h"
#include <fcntl.h>


#define LOCK_DB pthread_mutex_lock(&saveStateLock);
#define UNLOCK_DB pthread_mutex_unlock(&saveStateLock);
#define DB_SAVE_INTERVAL_SEC 10

static Secfs *secfs;
static Bool dbSaveRequested = false;
pthread_t saveStateThreadId;
pthread_mutex_t saveStateLock;

void schedule_db_save(void) {
    dbSaveRequested = true;
}

static void* fs_init(struct fuse_conn_info *connInfo, struct fuse_config *config) {
    debugPrint("fs_init");
    (void) connInfo;
    config->entry_timeout = 0;
    config->attr_timeout = 0;
    config->negative_timeout = 0;
    return NULL;
}

static int fs_getattr(const char *path, struct stat *statOut, struct fuse_file_info *info) {
    (void) info;

//    debugPrint("fs_getattr %s", path);
    Item *item = search_item_path(secfs->indexDB, (const String)path);
    if (item == NULL) {
        return -ENOENT;
    }
    
    memset(statOut, 0, sizeof(struct stat));
    if (item->type == ItemTypeDir) {
        statOut->st_mode = S_IFDIR | 0755;
    }
    else {
        statOut->st_mode = S_IFREG | 0755;
    }
    statOut->st_size = (off_t)item->size;
    statOut->st_nlink = 1;
    return SUCCESS;
}

static int fs_readdir(const char *path, void *out, fuse_fill_dir_t filler, off_t offset,
                      struct fuse_file_info *info, enum fuse_readdir_flags flags) {
    (void) offset;
    (void) info;
    (void) flags;

    debugPrint("fs_readdir %s", path);
    
    filler(out, ".", NULL, 0, 0);
    filler(out, "..", NULL, 0, 0);
    
    ItemArray items = get_dir_items(secfs->indexDB, (const String)path);
    for (UInt i = 0; i < items.length; i++) {
        if (strcmp(path, items.items[i]->path) == 0) {
            continue;
        }

        filler(out, basename(items.items[i]->path), NULL, 0, 0);
    }
    free(items.items);
    return SUCCESS;
}

static int fs_open(const char *path, struct fuse_file_info *fi) {
    debugPrint("fs_open %s", path);
    
    Item *item = search_item_path(secfs->indexDB, (const String)path);
    if (item == NULL) {
        return  -ENOENT;
    }
    
    // Truncate the file to 0 size of flag exist
    if ((fi->flags & O_TRUNC)) {
        purge_item(secfs, item);
        Item *newItem = create_item(ItemTypeFile, (String)path);
        LOCK_DB;
        add_item(secfs->indexDB, newItem);
        UNLOCK_DB;
    }
    
    return SUCCESS;
}

static int fs_read(const char *path, char *out, size_t size, off_t offset, struct fuse_file_info *info) {

    (void)info;
    debugPrint("fs_read %s (size=%d, offset=%d)", path, size, offset);

    // Get item
    Item *file = search_item_path(secfs->indexDB, (const String)path);
    if (file == NULL) {
        return -ENOENT;
    }
    
    if (file->type == ItemTypeDir) {
        return -EISDIR;
    }
    
    // Get all blocks to read
    BlocksForFileResult blocksResult = all_blocks_for_file(secfs->blockDB, file->id);

    debugPrint("Total blocks to read: %d", blocksResult.length);

    // Read from blocks
    ULong maxReadSize = MIN(file->size, (ULong)offset + size);
    UInt lastBlockIndex = (UInt)(maxReadSize / BLOCK_SIZE);
    for (UInt index = (UInt)(offset / BLOCK_SIZE); index <= lastBlockIndex; index ++) {
        debugPrint("    reading block %d",index);

        // Find the correct block in the list
        Block *currentBlock = NULL;
        for (UInt j = 0; j < blocksResult.length; j++) {
            if (blocksResult.blocks[j]->index == index) {
                currentBlock = blocksResult.blocks[j];
                break;
            }
        }
        
        // Read block bytes.
        ByteArray blockData;
        if (currentBlock == NULL) {
            blockData = initByteArray(BLOCK_SIZE);
        }
        else {
            blockData = read_block(secfs, currentBlock).bytes;
        }
        
        // Append to output
        ULong start = MAX((ULong)offset, index*BLOCK_SIZE);
        ULong end = MIN((ULong)offset+size, index*BLOCK_SIZE + BLOCK_SIZE);
        ULong dataStart = start - (ULong)offset;
        ULong dataEnd = end - (ULong)offset;
        
        debugPrint("    Block ranges %d to %d; Data ranges %d to %d", start, end, dataStart, dataEnd);
        memcpy( &out[dataStart], &blockData.bytes[start%BLOCK_SIZE], dataEnd - dataStart);
        
        free(blockData.bytes);
    }
    
    free(blocksResult.blocks);
    return (Int)size;
}

static int fs_write(const char *path, const char *data, size_t size, off_t offset, struct fuse_file_info *info) {

    (void)info;
    
    debugPrint("fs_write %s (size=%d, offset=%d)", path, size, offset);

    // Get item
    Item *file = search_item_path(secfs->indexDB, (const String)path);
    if (file == NULL) {
        return -ENOENT;
    }
    
    if (file->type == ItemTypeDir) {
        return -EISDIR;
    }
    
    // Get all blocks to write
    BlocksForFileResult blocksResult = all_blocks_for_file(secfs->blockDB, file->id);
    debugPrint("    Found %d blocks for the file", blocksResult.length);
    
    // Write data to blocks:
    for (UInt index = (UInt)(offset / BLOCK_SIZE); index <= (UInt)(((ULong)offset + size) / BLOCK_SIZE); index ++) {
        
        debugPrint("    Writing block %d",index);

        // Find the correct block in the list
        Block *currentBlock = NULL;
        for (UInt j = 0; j < blocksResult.length; j++) {
            if (blocksResult.blocks[j]->index == index) {
                currentBlock = blocksResult.blocks[j];
                break;
            }
        }
        
        // Read block bytes. If block doesn't exist, we will create a new one filled with zeros as the data
        ByteArray blockData;
        if (currentBlock == NULL) {
            debugPrint("   Creating new block index %d", index);
            currentBlock = generate_block(file->id, index);
            blockData = initByteArray(BLOCK_SIZE);
            LOCK_DB;
            add_block(secfs->blockDB, currentBlock);
            UNLOCK_DB;
        }
        else {
            ReadBlockResult readResult = read_block(secfs, currentBlock);
            blockData = readResult.bytes;
        }
    
        // Partially modify block according to the data
        ULong start = MAX((ULong)offset, index*BLOCK_SIZE);
        ULong end = MIN((ULong)offset+size, index*BLOCK_SIZE + BLOCK_SIZE);
        ULong dataStart = start - (ULong)offset;
        ULong dataEnd = end - (ULong)offset;
        debugPrint("   Block ranges %d to %d; Data ranges %d to %d", start, end, dataStart, dataEnd);
        memcpy( &blockData.bytes[start%BLOCK_SIZE], &data[dataStart], dataEnd - dataStart);
        
        // Write block back to disk
        Error writeError = write_block(secfs, currentBlock, blockData);
        if(writeError) {
            debugPrint("[Warning] couldn't write block on disk: %s", writeError);
        }
        free(blockData.bytes);
    }
    
    // Update file size
    file->size = MAX(file->size, size + (ULong)offset);
    
    // Finalize
    free(blocksResult.blocks);
    schedule_db_save();
    return (Int)size;
}

static int fs_truncate(const char *path, off_t size, struct fuse_file_info *info) {
    (void)info;
    debugPrint("fs_truncate %s to %d", path, size);
    
    Item *file = search_item_path(secfs->indexDB, (const String)path);
    if (file == NULL) {
        return -ENOENT;
    }
    
    if (file->type == ItemTypeDir) {
        return -EISDIR;
    }
    
    file->size = (ULong)size;
    
    schedule_db_save();
    return SUCCESS;
}

static int fs_create(const char *path, mode_t mode, struct fuse_file_info *info) {
    (void)mode;
    (void)info;

    if (path == NULL) {
        return -EEXIST;
    }
    
    debugPrint("fs_create %s", path);
    Item *existingItem = search_item_path(secfs->indexDB, (String)path);
    if (existingItem != NULL) {
        return -EEXIST;
    }
    
    Item *newItem = create_item(ItemTypeFile, (String)path);

    LOCK_DB;
    add_item(secfs->indexDB, newItem);
    UNLOCK_DB;
    
    schedule_db_save();
    return SUCCESS;
}

static int fs_mkdir(const char *path, mode_t mode) {
    (void)mode;
    debugPrint("fs_mkdir %s", path);
    Item *exisitngDir = search_item_path(secfs->indexDB, (String)path);
    if (exisitngDir != NULL) {
        return -EEXIST;
    }
    
    LOCK_DB;
    Item *newDir = create_item(ItemTypeDir, (String)path);
    UNLOCK_DB;
    add_item(secfs->indexDB, newDir);
    
    schedule_db_save();
    return SUCCESS;
}


static int fs_unlink(const char *path) {
    debugPrint("fs_unlink %s", path);

    Item *item = search_item_path(secfs->indexDB, (String)path);
    if (item == NULL) {
        return -ENOENT;
    }

    // Delete all saved blocks for file
    LOCK_DB;
    purge_item(secfs, item);
    UNLOCK_DB;
    
    schedule_db_save();
    return SUCCESS;
}

static int fs_rename(const char *sourcePath, const char *destinationPath, unsigned int flags) {
    debugPrint("fs_rename %s -> %s", sourcePath, destinationPath);

    if (flags) {
        return -EINVAL;
    }
    
    Item *sourceItem = search_item_path(secfs->indexDB, (String)sourcePath);
    if (sourceItem == NULL) {
        return -ENOENT;
    }
    
    Item *destinationItem = search_item_path(secfs->indexDB, (String)destinationPath);
    if (destinationItem != NULL && destinationItem->type == ItemTypeDir && sourceItem->type == ItemTypeFile){
        // Trying to rename file into folder
        return -EISDIR;
    }
    if (destinationItem != NULL && destinationItem->type == ItemTypeFile && sourceItem->type == ItemTypeDir){
        // Trying to rename folder into existing file
        return -ENOTDIR;
    }
    
    // remove old item
    if (destinationItem) {
        LOCK_DB;
        purge_item(secfs, destinationItem);
        UNLOCK_DB;
    }
    
    // Rename all descendants for directories
    ItemArray descendants = get_dir_descendants(secfs->indexDB, sourceItem->path);
    ULong sourceItemLength = strlen(sourceItem->path);
    for (UInt i = 0; i < descendants.length; i++) {
        Item *descendant = descendants.items[i];
        char newName[PATH_MAX_LENGTH];
        char suffix[PATH_MAX_LENGTH];
        ULong suffixLength = strlen(descendant->path) - sourceItemLength;
        strncpy(suffix, descendant->path + sourceItemLength, suffixLength);
        suffix[suffixLength] = '\0';
        snprintf(newName, sizeof newName, "%s%s", destinationPath, suffix);
        LOCK_DB;
        strcpy(descendant->path, newName);
        UNLOCK_DB;
    }
    free(descendants.items);
    
    LOCK_DB;
    strcpy(sourceItem->path,destinationPath);
    UNLOCK_DB;

    schedule_db_save();
    return 0;
}

static int fs_rmdir(const char *path)
{
    debugPrint("fs_mkdir %s", path);
    Item *dir = search_item_path(secfs->indexDB, (String)path);
    if (dir == NULL) {
        return -ENOENT;
    }
    
    LOCK_DB;
    purge_item(secfs, dir);
    UNLOCK_DB;

    schedule_db_save();
    return SUCCESS;
}

static int fs_statfs(const char *path, struct statvfs *stbuf) {
    debugPrint("fs_statfs %s", path);

    Int res = statvfs(secfs->dataPath, stbuf);
    if (res == ERROR) {
        return -errno;
    }

    return SUCCESS;
}

static int fs_access(const char *path, int mask) {
    debugPrint("fs_access %s", path);

    Item *item = search_item_path(secfs->indexDB, (String)path);
    if (item == NULL) {
        return -ENOENT;
    }
    
    Int res = access(secfs->dataPath, mask);
    if (res == ERROR) {
        return -errno;
    }

    return SUCCESS;
}

// Not implemented
static int fs_readlink(const char *path, char *buf, size_t size) {
    (void)buf;
    debugPrint("fs_readlink %s size=%d", path, size);
    return -ENOSYS;
}

static int fs_mknod(const char *path, mode_t mode, dev_t rdev) {
    debugPrint("fs_mknod %s, mode=%d, rdev=%d", path, mode, rdev);
    return -ENOSYS;
}

static int fs_symlink(const char *from, const char *to) {
    debugPrint("fs_symlink %s -> %s", from, to);
    return -ENOSYS;
}

static int fs_link(const char *from, const char *to) {
    debugPrint("fs_link %s -> %s", from, to);
    return -ENOSYS;
}


static int fs_chmod(const char *path, mode_t mode, struct fuse_file_info *fi) {
    (void)fi;
    debugPrint("fs_chmod %s, mode=%d", path,mode);
    return -ENOSYS;
}

static int fs_chown(const char *path, uid_t uid, gid_t gid, struct fuse_file_info *fi) {
    (void)fi;
    debugPrint("fs_chown %s uid=%d, gid=%d", path, uid, gid);
    return -ENOSYS;
}

static int fs_release(const char *path, struct fuse_file_info *fi) {
    (void)fi;
    debugPrint("fs_release %s", path);
    return -ENOSYS;
}

static int fs_fsync(const char *path, int isdatasync, struct fuse_file_info *fi) {
    (void)fi;
    debugPrint("fs_fsync %s, isdatasync=%d", path,isdatasync);
    return -ENOSYS;
}

static off_t fs_lseek(const char *path, off_t off, int whence, struct fuse_file_info *fi) {
    (void)fi;
    debugPrint("fs_lseek %s off=%d, whence=%d", path, off, whence);
    return -ENOSYS;
}

// Define all possible supported operations in the file system
static const struct fuse_operations secfs_operations = {
    .init            = fs_init,
    .getattr         = fs_getattr,
    .access          = fs_access,
    .mkdir           = fs_mkdir,
    .readdir         = fs_readdir,
    .rmdir           = fs_rmdir,
    .create          = fs_create,
    .open            = fs_open,
    .read            = fs_read,
    .write           = fs_write,
    .truncate        = fs_truncate,
    .unlink          = fs_unlink,
    .rename          = fs_rename,
    .statfs          = fs_statfs,
    
    .readlink        = fs_readlink,
    .mknod           = fs_mknod,
    .symlink         = fs_symlink,
    .link            = fs_link,
    .chmod           = fs_chmod,
    .chown           = fs_chown,
    .release         = fs_release,
    .fsync           = fs_fsync,
    .lseek           = fs_lseek,
};


// Save database to file every x seconds on a different thread
void* save_state(void* arg) {
    (void)arg;
    while (true) {
        if (dbSaveRequested) {
            LOCK_DB;
            archive_secfs(secfs);
            UNLOCK_DB;
            dbSaveRequested = false;
            
        }
        sleep(DB_SAVE_INTERVAL_SEC);
    }
}

void fs_start(Secfs *secfsRef, String mountPath) {
    secfs = secfsRef;
    
    //schedule database saving in a different thread
    pthread_mutex_init(&saveStateLock, NULL);
    pthread_create(&saveStateThreadId, NULL, save_state, NULL);
    
    String args[4];
    Int argc = 0;
    args[argc++] = "secfs";
    
    // Run in foreground intead of background
    args[argc++] = "-f";
    
    // Disable multithreading for easier debugging and developing
    args[argc++] = "-s";
    args[argc++] = mountPath;

    struct fuse_args fuse_args = FUSE_ARGS_INIT(argc, args);
    Int returnCode = fuse_main(fuse_args.argc, fuse_args.argv, &secfs_operations, NULL);
    
    fuse_opt_free_args(&fuse_args);
    pthread_mutex_destroy(&saveStateLock);
    exit(returnCode);
}
