//
//  Created by Stasel
//

#ifndef secfs_h
#define secfs_h

#include "../utilities/utilities.h"
#include "../db/indexdb.h"
#include "../db/blockdb.h"

#define INDEX_DB_NAME ".secfs"
#define BLOCK_DB_NAME ".secfs_blocks"
#define IV_FILE_NAME  ".secfs.iv"
#define ENCRYPTED_IV_FILE_NAME  ".secfs.iv.enc"

typedef struct {
    IndexDB *indexDB;
    BlockDB *blockDB;
    String dataPath;
    ByteArray key;
    ByteArray iv; // used for database encryption only. All other files will have their own iv
}Secfs;

typedef struct {
    String error;
    Secfs *secfs;
} LoadSecfsResult;

typedef struct {
    String error;
    ByteArray bytes;
} ReadBlockResult;

typedef struct {
    String error;
    ByteArray iv;
} LoadIVResult;

LoadSecfsResult init_secfs(String dataPath, ByteArray key, ByteArray iv);
LoadSecfsResult load_secfs(String dataPath, ByteArray key);
Bool is_existing_secfs(String dataPath);
Error archive_secfs(Secfs *secfs);

ReadBlockResult read_block(Secfs *secfs, Block *block);
Error write_block(Secfs *secfs, Block *block, ByteArray data);
Error delete_block_from_disk(Secfs *secfs, Block *block);
void purge_item(Secfs *secfs, Item *item);
Bool verify_key(ByteArray key, ByteArray iv, String dataPath);
LoadIVResult load_iv(String dataPath);

#endif /* secfs_h */
