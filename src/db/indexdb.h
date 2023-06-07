//
//  Created by Stasel
//

#ifndef indexdb_h
#define indexdb_h

#include <uuid/uuid.h>
#include "../utilities/utilities.h"

typedef enum { ItemTypeFile = 0, ItemTypeDir = 1 } ItemType;

typedef struct {
    uuid_t id;
    ItemType type;
    char path[512];
    ULong size;
} Item;

typedef struct {
    Item **items;
    UInt length;
    UInt max;
}IndexDB;

typedef struct {
    String error;
    IndexDB *indexDB;
} LoadIndexDBResult;

typedef struct {
    UInt length;
    Item **items;
} ItemArray;


IndexDB* init_indexDB(void);
LoadIndexDBResult load_indexDB (const String path, ByteArray key, ByteArray iv);
Error archive_indexDB(const String path, IndexDB *db, ByteArray key, ByteArray iv);

Item* create_item(ItemType type, String path);
void add_item(IndexDB *db, Item *item);
void remove_item(IndexDB *db, uuid_t itemId);
Item* search_item(IndexDB *db, uuid_t itemId);
Item* search_item_path(IndexDB *db, const String path);

ItemArray get_dir_items(IndexDB *db, const String path);
ItemArray get_dir_descendants(IndexDB *db, const String path);


#endif /* indexdb */
