//
//  Created by Stasel
//

#include "blockdb.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <uuid/uuid.h>
#include "../utilities/utilities.h"

Error archive_blockDB (const String path, BlockDB *db, ByteArray key, ByteArray iv) {
    ByteArray archivedData = initByteArray(db->length * (UInt)sizeof(Block));
    for (UInt i = 0; i < db->length; i++) {
        memcpy(&archivedData.bytes[i * sizeof(Block)], db->blocks[i], sizeof(Block));
    }
    
    // Encrypt
    ByteArray encryptedData = initByteArray(0);
    if (archivedData.length > 0) {
        AESEncryptResult encryptResult = aes_encrypt(archivedData, key, iv);
        if (encryptResult.error) {
            free(archivedData.bytes);
            return encryptResult.error;
        }
        encryptedData = encryptResult.cipher;
    }
    
    WriteFileResult writeResult = writeFile(path, encryptedData);
    free(encryptedData.bytes);
    free(archivedData.bytes);
    return writeResult.error;

    return NULL;
}

LoadBlockDBResult load_blockDB (const String path, ByteArray key, ByteArray iv) {
    LoadBlockDBResult result;
    result.error = NULL;
    result.blockDB = NULL;
    
    // Read encrypted file
    ReadFileResult readResult = readFile(path);
    if (readResult.error) {
        result.error = readResult.error;
        return result;
    }
    
    // Decrypt data if exist
    ByteArray data;
    if (readResult.contents.length > 0) {
        AESDecryptResult decryptResult = aes_decrypt(readResult.contents, key, iv);
        if (decryptResult.error) {
            result.error = decryptResult.error;
            return result;
        }
        data = decryptResult.plainText;
    }
    else {
        data = readResult.contents; // 0 bytes
    }
    
    // Extract data
    result.blockDB = init_blockDB();
    Block *readBlock = ALLOC(Block);
    for (ULong offset = 0; offset < data.length; offset += sizeof(Block)) {
        memcpy(readBlock, &data.bytes[offset], sizeof(Block));
        add_block(result.blockDB, readBlock);
        readBlock = ALLOC(Block);
    }
    
    free(readBlock);
    free(data.bytes);

    return result;
}

BlockDB* init_blockDB(void) {
    BlockDB *newDB = ALLOC(BlockDB);
    newDB->length = 0;
    newDB->max = 0;
    newDB->blocks = NULL;
    return newDB;
}

Block* generate_block(uuid_t fileId, UInt index) {
    Block *newBlock = ALLOC(Block);
    uuid_copy(newBlock->fileId, fileId);
    uuid_generate(newBlock->id);
    newBlock->index = index;
    ByteArray randomBytes = get_random_bytes(IV_LENGTH);
    memcpy(newBlock->iv, randomBytes.bytes, randomBytes.length);
    free(randomBytes.bytes);
    return newBlock;
}

void add_block(BlockDB *db, Block *block) {
    // Increase memory to store blocks if it's full
    while (db->length >= db->max) {
        if (db -> max == 0) {
            db -> blocks = malloc(sizeof(Block)*1);
            db -> max = 1;
        }
        else {
            db->max = db->max * 2;
            db->blocks = realloc(db->blocks, sizeof(Block) * db->max);
        }
    }
    // Add block and increase index
    db->blocks[db->length] = block;
    (db->length)++;
}

Int search_block_index(BlockDB *db, uuid_t blockId) {
    for (UInt i = 0; i < db->length; i++) {
        if(uuid_compare(db->blocks[i]->id, blockId) == 0) {
            return (Int)i;
        }
    }
    return -1;
}

void remove_block(BlockDB *db, uuid_t blockId) {
    // find block index
    Int index = search_block_index(db, blockId);
    if (index == -1) {
        // block was not found, nothing to remove
        return;
    }
    
    // free and shift array to the left
    free(db->blocks[index]);
    for (Int i = index; i < (Int)db->length - 1; i++) {
        db->blocks[i] = db->blocks[i+1];
    }
    
    (db->length)--;
}

Block* search_block(BlockDB *db, uuid_t blockId) {
    Int index = search_block_index(db, blockId);
    if (index == -1) {
        // block was not found
        return NULL;
    }
    return db->blocks[index];
}

BlocksForFileResult all_blocks_for_file(BlockDB *db, uuid_t fileId) {
    BlocksForFileResult result;
    result.length = 0;
    result.blocks = malloc(sizeof(Block) * db->length);
    for (UInt i = 0; i < db->length; i++) {
        Block *block = db->blocks[i];
        if (uuid_compare(block->fileId, fileId) == 0) {
            result.blocks[result.length++] = block;
        }
    }
    
    // compact array
    result.blocks = realloc(result.blocks, sizeof(Block) * result.length);
    return result;
}

BlocksForFileResult blocks_for_file(BlockDB *db, uuid_t fileId, UInt offset, ULong size) {
    
    BlocksForFileResult result;
    result.length = 0;
    result.blocks = malloc(sizeof(Block) * db->length);
    for (UInt i = 0; i < db->length; i++) {
        Block *block = db->blocks[i];
        Bool matchFileId = uuid_compare(block->fileId, fileId) == 0;
        UInt blockStart = block->index * BLOCK_SIZE;
        UInt blockEnd = blockStart + BLOCK_SIZE;
        Bool matchRange = (blockStart >= offset && blockStart <= size+offset) || ( blockEnd >= offset && blockEnd <= size + offset);

        if (matchFileId && matchRange) {
            result.blocks[result.length++] = block;
        }
    }
    
    // compact array
    result.blocks = realloc(result.blocks, sizeof(Block) * result.length);
    return result;
}
