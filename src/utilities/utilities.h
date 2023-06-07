//
//  Created by Stasel
//

#ifndef utilities_h
#define utilities_h

#include <stdlib.h>
#include <inttypes.h>

#define DEBUG 0
#define SUCCESS 0
#define ERROR -1
#define PATH_MAX_LENGTH 512

// Macros
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))
#define ALLOC(type) malloc(sizeof(type))

// Types
typedef uint8_t Byte;
typedef int8_t Short;
typedef int32_t Int;
typedef uint32_t UInt;
typedef int64_t Long;
typedef uint64_t ULong;
typedef char* String;
typedef enum { false = 0, true = 1 } Bool;
typedef String Error;

typedef struct {
    Byte* bytes;
    UInt length;
} ByteArray;

typedef struct {
    String error;
    ULong size;
} FileSizeResult;

typedef struct {
    String error;
    ByteArray contents;
} ReadFileResult;

typedef struct {
    String error;
} WriteFileResult;


// Helper methods
void debugPrint(const String message, ...);
void fatalError(const String message, ...);
Bool isPrefix(const String prefix, const String string);
Int firstIndexOf(const String string, char c);
Bool boolPrompt(void);
ByteArray initByteArray(UInt size);

// Filesystem helpers
FileSizeResult fileSize(const String path);
ReadFileResult readFile(const String path);
WriteFileResult writeFile(const String path, ByteArray data);
Bool isFileExists(const String path);

#endif /* utilities_h */
