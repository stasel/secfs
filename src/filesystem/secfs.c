//
//  Created by Stasel
//

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "secfs.h"
#include "../security/encryption.h"

#define UUID_STRING_LENGTH 37
#define INIT_HANDLE_ERROR(err)    if ((err)) {\
        result.error = (err);\
        free(result.secfs);\
        result.secfs = NULL;\
        return result;\
    }

Error archive_secfs(Secfs *secfs) {
    char indexDBPath[PATH_MAX_LENGTH];
    char blockDBPath[PATH_MAX_LENGTH];
    snprintf(indexDBPath, sizeof blockDBPath, "%s%s", secfs->dataPath, INDEX_DB_NAME);
    snprintf(blockDBPath, sizeof blockDBPath, "%s%s", secfs->dataPath, BLOCK_DB_NAME);
    
    debugPrint("Archive secdb to %s", secfs->dataPath);
    Error error = archive_indexDB(indexDBPath, secfs->indexDB, secfs->key, secfs->iv);
    if (error) {
        return error;
    }
    
    error = archive_blockDB(blockDBPath, secfs->blockDB, secfs->key, secfs->iv);
    if (error) {
        return error;
    }
    
    return NULL;
}

Bool is_existing_secfs(String dataPath) {
    // make sure all files exist
    char indexDBPath[PATH_MAX_LENGTH];
    char blockDBPath[PATH_MAX_LENGTH];
    char ivFilePath[PATH_MAX_LENGTH];
    char encryptedIVFilePath[PATH_MAX_LENGTH];
    snprintf(indexDBPath, sizeof indexDBPath, "%s%s", dataPath, INDEX_DB_NAME);
    snprintf(blockDBPath, sizeof blockDBPath, "%s%s", dataPath, BLOCK_DB_NAME);
    snprintf(ivFilePath, sizeof ivFilePath, "%s%s", dataPath, IV_FILE_NAME);
    snprintf(encryptedIVFilePath, sizeof encryptedIVFilePath, "%s%s", dataPath, ENCRYPTED_IV_FILE_NAME);
    return isFileExists(indexDBPath) && isFileExists(blockDBPath) && isFileExists(ivFilePath) &&isFileExists(encryptedIVFilePath);
}


LoadSecfsResult load_secfs(String dataPath, ByteArray key) {
    LoadSecfsResult result = { NULL, NULL };
    char indexDBPath[PATH_MAX_LENGTH];
    char blockDBPath[PATH_MAX_LENGTH];
    char ivFilePath[PATH_MAX_LENGTH];
    snprintf(indexDBPath, sizeof blockDBPath, "%s%s", dataPath, INDEX_DB_NAME);
    snprintf(blockDBPath, sizeof blockDBPath, "%s%s", dataPath, BLOCK_DB_NAME);
    snprintf(ivFilePath, sizeof ivFilePath, "%s%s", dataPath, IV_FILE_NAME);

    LoadIVResult ivResult = load_iv(dataPath);
    if (ivResult.error) {
        result.error = ivResult.error;
        return result;
    }
    
    LoadIndexDBResult indexResult = load_indexDB(indexDBPath, key, ivResult.iv);
    if (indexResult.error) {
        result.error = indexResult.error;
        return result;
    }
    
    LoadBlockDBResult blockResult = load_blockDB(blockDBPath, key, ivResult.iv);
    if (blockResult.error) {
        result.error = blockResult.error;
        return result;
    }
        
    result.secfs = ALLOC(Secfs);
    result.secfs->dataPath = malloc(strlen(dataPath) + 1);
    strcpy(result.secfs->dataPath, dataPath);
    result.secfs->indexDB = indexResult.indexDB;
    result.secfs->blockDB = blockResult.blockDB;
    result.secfs->iv = ivResult.iv;
    result.secfs->key = key;
    return result;
}

LoadSecfsResult init_secfs(String dataPath, ByteArray key, ByteArray iv) {

    char indexDBPath[PATH_MAX_LENGTH];
    char blockDBPath[PATH_MAX_LENGTH];
    char ivFilePath[PATH_MAX_LENGTH];
    char encryptedIVFilePath[PATH_MAX_LENGTH];

    snprintf(indexDBPath, sizeof indexDBPath, "%s%s", dataPath, INDEX_DB_NAME);
    snprintf(blockDBPath, sizeof blockDBPath, "%s%s", dataPath, BLOCK_DB_NAME);
    snprintf(ivFilePath, sizeof ivFilePath, "%s%s", dataPath, IV_FILE_NAME);
    snprintf(encryptedIVFilePath, sizeof encryptedIVFilePath, "%s%s", dataPath, ENCRYPTED_IV_FILE_NAME);

    LoadSecfsResult result;
    result.secfs = ALLOC(Secfs);
    result.secfs -> iv = iv;
    result.secfs -> key = key;
    result.secfs -> indexDB = init_indexDB();
    result.secfs -> blockDB = init_blockDB();
    result.secfs -> dataPath = malloc(strlen(dataPath) + 1);
    result.error = NULL;
    strcpy(result.secfs->dataPath, dataPath);
    
    WriteFileResult writeIVResult = writeFile(ivFilePath, iv);
    INIT_HANDLE_ERROR(writeIVResult.error);

    // encrypt and save encrypted IV for later verification
    AESEncryptResult encryptionResult = aes_encrypt(iv, key, iv);
    INIT_HANDLE_ERROR(encryptionResult.error);
    WriteFileResult encryptedIVWriteResult = writeFile(encryptedIVFilePath, encryptionResult.cipher);
    INIT_HANDLE_ERROR(encryptedIVWriteResult.error);
    
    // Create initial root folder for the file system
    Item *root = create_item(ItemTypeDir, "/");
    add_item(result.secfs->indexDB, root);
    
    Error error = archive_secfs(result.secfs);
    INIT_HANDLE_ERROR(error);

    return result;
}

ReadBlockResult read_block(Secfs *secfs, Block *block) {
    
    ReadBlockResult result;
    result.error = NULL;
    result.bytes.length = 0;
    
    char uuidString[UUID_STRING_LENGTH];
    char blockPath[PATH_MAX_LENGTH];
    uuid_unparse_lower(block->id, uuidString);
    snprintf(blockPath, sizeof blockPath, "%s%s", secfs->dataPath, uuidString);

    debugPrint("Reading data from block %s", blockPath);
    ReadFileResult readResult = readFile(blockPath);
    if (readResult.error) {
        result.error = readResult.error;
        return result;
    }
    
    // Decrypt block before returning to FUSE
    ByteArray blockIV = { block->iv, IV_LENGTH };
    AESDecryptResult decryptResult = aes_decrypt(readResult.contents, secfs->key, blockIV);
    if (decryptResult.error) {
        result.error = decryptResult.error;
        return result;
    }
    
    free(readResult.contents.bytes);
    result.bytes = decryptResult.plainText;
    return result;
}

Error write_block(Secfs *secfs, Block *block, ByteArray data) {
    char uuidString[UUID_STRING_LENGTH];
    char blockPath[PATH_MAX_LENGTH];
    uuid_unparse_lower(block->id, uuidString);
    snprintf(blockPath, sizeof blockPath, "%s%s", secfs->dataPath, uuidString);
    
    // Encrypt block before writing to disk
    ByteArray blockIV = { block->iv, IV_LENGTH };
    AESEncryptResult encryptResult = aes_encrypt(data, secfs->key, blockIV);
    if (encryptResult.error) {
        return encryptResult.error;
    }
    
    // Write encrypted data to file
    debugPrint("Writing %d bytes of data to block %s", data.length, blockPath);
    WriteFileResult writeResult = writeFile(blockPath, encryptResult.cipher);
    if(writeResult.error) {
        return writeResult.error;
    }
    
    free(encryptResult.cipher.bytes);
    return NULL;
}

Error delete_block_from_disk(Secfs *secfs, Block *block) {
    char uuidString[37];
    char blockPath[PATH_MAX_LENGTH];
    uuid_unparse_lower(block->id, uuidString);
    snprintf(blockPath, sizeof blockPath, "%s%s", secfs->dataPath, uuidString);

    debugPrint("Deleting block %s from disk", blockPath);
    if(unlink(blockPath) != 0) {
        return strerror(errno);
    }
    
    return NULL;
}

void purge_item(Secfs *secfs, Item *item) {
    if (item->type == ItemTypeFile) {
        BlocksForFileResult result = all_blocks_for_file(secfs->blockDB, item->id);
        for (UInt i = 0 ; i < result.length; i++) {
            Block *block = result.blocks[i];
            delete_block_from_disk(secfs, block);
            remove_block(secfs->blockDB, block->id);
        }
    }
    else if (item->type == ItemTypeDir) {
        ItemArray descendants = get_dir_descendants(secfs->indexDB, item->path);
        for (UInt i = 0; i < descendants.length; i++) {
            if (descendants.items[i]->type == ItemTypeFile) {
                purge_item(secfs, descendants.items[i]);
            }
            else {
                remove_item(secfs->indexDB, descendants.items[i]->id);
            }
        }
        free(descendants.items);
    }
    remove_item(secfs->indexDB, item->id);
}

Bool verify_key(ByteArray key, ByteArray iv, String dataPath) {
    char encryptedIVPath[PATH_MAX_LENGTH];
    snprintf(encryptedIVPath, sizeof encryptedIVPath, "%s%s", dataPath, ENCRYPTED_IV_FILE_NAME);
    ReadFileResult readResult = readFile(encryptedIVPath);
    if(readResult.error) {
        return false;
    }
    
    AESDecryptResult decryptResult = aes_decrypt(readResult.contents, key, iv);
    if (decryptResult.error) {
        return false;
    }
    UInt compareSize = MIN(iv.length, decryptResult.plainText.length);
    Int compareResult = memcmp(decryptResult.plainText.bytes, iv.bytes, compareSize);
    free(decryptResult.plainText.bytes);
    free(readResult.contents.bytes);
    return compareResult == 0;
}

LoadIVResult load_iv(String dataPath) {
    LoadIVResult result;
    result.error = NULL;
    char ivFilePath[PATH_MAX_LENGTH];
    snprintf(ivFilePath, sizeof ivFilePath, "%s%s", dataPath, IV_FILE_NAME);
    ReadFileResult readResult = readFile(ivFilePath);
    if (result.error) {
        result.error = readResult.error;
    }
    result.iv = readResult.contents;
    return result;
}
