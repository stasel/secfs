//
//  Created by Stasel
//

#ifndef blockdb_h
#define blockdb_h

#include <uuid/uuid.h>
#include "../utilities/utilities.h"
#include "../security/encryption.h"

#define BLOCK_SIZE 524288 // 512k

typedef struct {
    uuid_t id;
    uuid_t fileId;
    UInt index;
    Byte iv[IV_LENGTH];
} Block;

typedef struct {
    Block **blocks;
    UInt length;
    UInt max;
} BlockDB;

typedef struct {
    String error;
    BlockDB *blockDB;
} LoadBlockDBResult;

typedef struct {
    UInt length;
    Block **blocks;
} BlocksForFileResult;

LoadBlockDBResult load_blockDB (const String path, ByteArray key, ByteArray iv);
Error archive_blockDB (const String path, BlockDB *db, ByteArray key, ByteArray iv);

BlockDB* init_blockDB(void);
Block* generate_block(uuid_t fileId, UInt index);
void add_block(BlockDB *db, Block *block);
void remove_block(BlockDB *db, uuid_t blockId);
Block* search_block(BlockDB *db, uuid_t blockId);
BlocksForFileResult all_blocks_for_file(BlockDB *db, uuid_t fileId);
BlocksForFileResult blocks_for_file(BlockDB *db, uuid_t fileId, UInt offset, ULong size);

#endif /* blockdb_h */
