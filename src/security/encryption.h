//
//  Created by Stasel
//

#ifndef encryption_h
#define encryption_h

#include "../utilities/utilities.h"

#define KEY_LENGTH 32
#define IV_LENGTH 16

typedef struct {
    ByteArray cipher;
    String error;
} AESEncryptResult;

typedef struct {
    ByteArray plainText;
    String error;
} AESDecryptResult;

typedef struct {
    ByteArray digest;
    String error;
} SHA256Result;


AESEncryptResult aes_encrypt(ByteArray bytes, ByteArray key, ByteArray iv);
AESDecryptResult aes_decrypt(ByteArray bytes, ByteArray key, ByteArray iv);
SHA256Result sha_256(ByteArray data);
ByteArray get_random_bytes(UInt size);
ByteArray generate_key(ByteArray userPassword, ByteArray salt);

#endif /* encryption_h */
