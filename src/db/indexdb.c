//
//  Created by Stasel
//

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "indexdb.h"
#include "../utilities/utilities.h"
#include "../security/encryption.h"

IndexDB* init_indexDB(void) {
    IndexDB* newDB = ALLOC(IndexDB);
    newDB->items = NULL;
    newDB->length = 0;
    newDB->max = 0;
    return newDB;
}

Error archive_indexDB(const String path, IndexDB *db, ByteArray key, ByteArray iv) {
    
    // Export to binary data
    ByteArray archivedData = initByteArray(db->length * (UInt)sizeof(Item));
    for (UInt i = 0; i < db->length; i++) {
        memcpy(&archivedData.bytes[i * sizeof(Item)], db->items[i], sizeof(Item));
    }
    
    // Encrypt
    AESEncryptResult encryptResult = aes_encrypt(archivedData, key, iv);
    if (encryptResult.error) {
        free(archivedData.bytes);
        return encryptResult.error;
    }
    
    WriteFileResult writeResult = writeFile(path, encryptResult.cipher);
    free(archivedData.bytes);
    free(encryptResult.cipher.bytes);
    return writeResult.error;
}

LoadIndexDBResult load_indexDB (const String path, ByteArray key, ByteArray iv) {
    LoadIndexDBResult result;
    result.error = NULL;
    result.indexDB = NULL;
    
    // Read encrypted file
    ReadFileResult readResult = readFile(path);
    if (readResult.error) {
        result.error = readResult.error;
        return result;
    }
    
    // Decrypt data
    AESDecryptResult decryptResult = aes_decrypt(readResult.contents, key, iv);
    if (decryptResult.error) {
        result.error = decryptResult.error;
        return result;
    }
    ByteArray data = decryptResult.plainText;
    
    // Extract data
    result.indexDB = init_indexDB();
    Item *readItem = ALLOC(Item);
    for (ULong offset = 0; offset < data.length; offset += sizeof(Item)) {
        memcpy(readItem, &data.bytes[offset], sizeof(Item));
        add_item(result.indexDB, readItem);
        readItem = ALLOC(Item);
    }
    
    free(readItem);
    free(data.bytes);

    return result;
}

Item* create_item(ItemType type, String path) {
    Item *newItem = ALLOC(Item);
    uuid_generate(newItem->id);
    strcpy(newItem->path, path);
    newItem->type = type;
    newItem->size = 0;
    return newItem;
}

void add_item(IndexDB *db, Item *item) {
    // Increase memory to store blocks if it's full
    while (db->length >= db->max) {
        if (db -> max == 0) {
            db -> items = malloc(sizeof(Item)*1);
            db -> max = 1;
        }
        else {
            db->max = db->max * 2;
            db->items = realloc(db->items, sizeof(Item) * db->max);
        }
    }
    // Add block and increase index
    db->items[db->length] = item;
    (db->length)++;
}

Int search_item_index(IndexDB *db, uuid_t itemId) {
    for (UInt i = 0; i < db->length; i++) {
        if(uuid_compare(db->items[i]->id, itemId) == 0) {
            return (Int)i;
        }
    }
    return -1;
}

void remove_item(IndexDB *db, uuid_t itemId) {
    // find item index
    Int index = search_item_index(db, itemId);
    if (index == -1) {
        // item was not found, nothing to remove
        return;
    }
    
    // free and shift array to the left
    free(db->items[index]);
    for (Int i = index; i < (Int)db->length - 1; i++) {
        db->items[i] = db->items[i+1];
    }
    
    (db->length)--;
}

Item* search_item(IndexDB *db, uuid_t itemId) {
    Int index = search_item_index(db, itemId);
    if (index == -1) {
        // block was not found
        return NULL;
    }
    return db->items[index];
}

Item* search_item_path(IndexDB *db, const String path) {
    for (UInt i = 0; i < db->length; i++) {
        if(strcmp(path, db->items[i]->path) == 0) {
            return db->items[i];
        }
    }
    return NULL;
}


ItemArray get_dir_items(IndexDB *db, const String path) {
    ItemArray result;
    result.length = 0;
    result.items = malloc(sizeof(Item) * db->length);
    
    for (UInt i = 0; i < db->length; i++) {
        Item *item = db->items[i];
        if (!isPrefix(path, item->path)) {
            continue;
        }
        // make sure its direct child
        String suffix = item->path + strlen(path) + 1;
        Int slashIndex = firstIndexOf(suffix, '/');
        
        // Check that there are no '/' characters in the path beyond suffix
        // With the exception of the last character
        if(strlen(suffix) > 0 && (slashIndex == -1 || slashIndex == (Int)strlen(suffix) - 1)) {
            result.items[result.length++] = item;
        }
    }
    
    // compact array
    result.items = realloc(result.items, sizeof(Item) * result.length);
    return  result;
}

ItemArray get_dir_descendants(IndexDB *db, const String path) {
    ItemArray result;
    result.length = 0;
    result.items = malloc(sizeof(Item) * db->length);
    
    for (UInt i = 0; i < db->length; i++) {
        Item *item = db->items[i];
        if (isPrefix(path, item->path)) {
            result.items[result.length++] = item;
        }
    }
    
    // compact array
    result.items = realloc(result.items, sizeof(Item) * result.length);
    return result;
}
