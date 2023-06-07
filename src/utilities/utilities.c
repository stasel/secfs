//
//  Created by Stasel
//

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include "utilities.h"

void debugPrint(String message, ...) {
    if(!DEBUG) {
        return;
    }

    va_list arglist;
    va_start( arglist, message );
    printf("[DEBUG] ");
    vprintf( message, arglist );
    printf("\n");
    va_end( arglist );
}

void fatalError(String message, ...)
{
    va_list arglist;
    va_start( arglist, message );
    fprintf(stderr, "[FATAL] ");
    vfprintf(stderr, message, arglist );
    fprintf(stderr, "\n");
    va_end( arglist );
    exit(EXIT_FAILURE);
}

Bool isPrefix(const String prefix, const String string) {
    return strncmp(prefix, string, strlen(prefix)) == 0;
}

Int firstIndexOf(const String string, char c) {
    String ptr = strchr(string, c);
    if(ptr) {
       return (Int)(ptr - string);
    }
    return  -1;;
}

Bool boolPrompt() {
    Int answer = getchar();
    getchar(); // To consume `\n'
    return answer == 'y' || answer == 'Y';
}

ByteArray initByteArray(UInt size) {
    ByteArray result;
    result.length = size;
    result.bytes = malloc(size);
    memset(result.bytes, 0, size);
    return result;
}

FileSizeResult fileSize(const String path) {
    FileSizeResult result;
    
    struct stat stats;
    if(stat(path, &stats) == ERROR) {
        result.error = strerror(errno);
        result.size = 0;
        return result;
    }

    result.error = NULL;
    result.size = (ULong)stats.st_size;
    return result;
}

ReadFileResult readFile(const String path) {
    ReadFileResult result;
    result.error = NULL;
    
    FileSizeResult fileSizeResult = fileSize(path);
    if (fileSizeResult.error) {
        result.error = fileSizeResult.error;
        return result;
    }
    ULong fileSize = fileSizeResult.size;
    
    FILE *handler = fopen(path, "rb");
    if (handler == NULL) {
        result.error = strerror(errno);
        return result;
    }
    
    ByteArray fileContents;
    fileContents.length = (UInt)fileSize;
    fileContents.bytes = malloc(fileSize);

    if(fileSize > 0 && fread(fileContents.bytes, fileSize, 1, handler) != 1) {
        result.error = strerror(errno);
        return result;
    }
    
    fclose(handler);
    result.contents = fileContents;
    result.error = NULL;
    return result;
}

WriteFileResult writeFile(const String path, ByteArray data) {
    
    WriteFileResult result;

    FILE *handler = fopen(path, "w");
    if (handler == NULL) {
        result.error = strerror(errno);
        return result;
    }
    
    fwrite(data.bytes, data.length, 1, handler);
    fclose(handler);
    result.error = NULL;
    return result;
}

Bool isFileExists(const String path) {
    return access(path, F_OK) != -1;
}
