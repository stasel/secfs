//
//  Created by Stasel
//

#include <stdlib.h>
#include <string.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include "encryption.h"
#include "../utilities/utilities.h"

AESEncryptResult aes_encrypt(ByteArray bytes, ByteArray key, ByteArray iv) {
    
    AESEncryptResult result;
    
    if (key.length != KEY_LENGTH) {
        result.error = "Invalid key length";
        return result;
    }
    
    if (iv.length != IV_LENGTH) {
        result.error = "Invalid IV length";
        return result;
    }
    
    if (bytes.length == 0) {
        result.error = "Nothing to encrypt";
        return result;
    }
    
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        result.error = "Error initializing new context";
        return result;
    }
    
    Int initResult = EVP_EncryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, key.bytes, iv.bytes);
    if (!initResult) {
        result.error = "Error initializing encryption";
        return result;
    }
    
    Int cipherTextLength;
    Byte *cipherText = malloc((ULong)MAX(bytes.length*2, 32));
    
    Int encryptionResult = EVP_EncryptUpdate(ctx, cipherText, &cipherTextLength, bytes.bytes, (Int)bytes.length);
    if (!encryptionResult) {
        result.error = "Encryption error";
        return result;
    }
    
    Int cipherTextFinalizeLength;
    Int finalizeResult = EVP_EncryptFinal_ex(ctx, cipherText + cipherTextLength, &cipherTextFinalizeLength);
    if (!finalizeResult) {
        result.error = "Finalize error";
        return result;
    }
    
    EVP_CIPHER_CTX_free(ctx);
    
    UInt finalCipherLength = (UInt)cipherTextLength + (UInt)cipherTextFinalizeLength;
    cipherText = realloc(cipherText, (UInt)finalCipherLength);
    
    result.cipher.bytes = cipherText;
    result.cipher.length = finalCipherLength;
    result.error = NULL;
    
    debugPrint("Encrypted %d bytes of plaintext to %d bytes of cipher", bytes.length, result.cipher.length);
    
    return result;
}

AESDecryptResult aes_decrypt(ByteArray cipher, ByteArray key, ByteArray iv) {
    AESDecryptResult result;
    
    if (key.length != KEY_LENGTH) {
        result.error = "Invalid key length";
        return result;
    }
    
    if (iv.length != IV_LENGTH) {
        result.error = "Invalid IV length";
        return result;
    }
    
    if (cipher.length == 0) {
        result.error = "Nothing to decrypt";
        return result;
    }
    
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        result.error = "Error initializing new context";
        return result;
    }
    
    Int initResult = EVP_DecryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, key.bytes, iv.bytes);
    if (!initResult) {
        result.error = "Error initializing decryption";
        return result;
    }
    
    Int plainTextLength;
    Byte *plainText = malloc((UInt)cipher.length);
    
    Int decryptionResult = EVP_DecryptUpdate(ctx, plainText, &plainTextLength, cipher.bytes, (Int)cipher.length);
    if (!decryptionResult) {
        result.error = "Decryption error";
        return result;
    }
    
    Int plainTextFinalizeLength;
    Int finalizeResult = EVP_DecryptFinal(ctx, plainText + plainTextLength, &plainTextFinalizeLength);
    if (!finalizeResult) {
        result.error = "Finalize error";
        return result;
    }
    
    EVP_CIPHER_CTX_free(ctx);

    UInt finalPlainTextLength = (UInt)plainTextLength + (UInt)plainTextFinalizeLength;
    plainText = realloc(plainText, (UInt)finalPlainTextLength);

    result.plainText.bytes = plainText;
    result.plainText.length = finalPlainTextLength;
    result.error = NULL;
    
    debugPrint("Decrypted %d bytes of cipher to %d bytes of plaintext",cipher.length,result.plainText.length);
    
    return result;
}

SHA256Result sha_256(ByteArray data) {
    SHA256Result result;
    
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) {
        result.error = "Error initializing new context";
        return result;
    }
    
    Int initResult = EVP_DigestInit_ex(ctx, EVP_sha256(), NULL);
    if (!initResult) {
        result.error = "Error initializing digest";
        return result;
    }
        
    Int decryptionResult = EVP_DigestUpdate(ctx, data.bytes, data.length);
    if (!decryptionResult) {
        result.error = "Digest error";
        return result;
    }
    
    Byte *digest = OPENSSL_malloc((UInt)EVP_MD_size(EVP_sha256()));
    UInt length;
    Int finalizeResult = EVP_DigestFinal_ex(ctx, digest, &length);
    if (!finalizeResult) {
        result.error = "Finalize error";
        return result;
    }
    EVP_MD_CTX_free(ctx);

    result.digest.length = length;
    result.digest.bytes = digest;
    result.error = NULL;
    return result;
}

ByteArray get_random_bytes(UInt size) {
    ByteArray random;
    random.length = size;
    random.bytes = malloc(size);
    RAND_bytes(random.bytes, (Int)size);
    return random;
}

ByteArray generate_key(ByteArray userPassword, ByteArray salt) {
    // Concatenate password and salt
    ByteArray saltedPassword;
    saltedPassword.length = userPassword.length + salt.length;
    saltedPassword.bytes = malloc(saltedPassword.length);
    memcpy(saltedPassword.bytes, userPassword.bytes, userPassword.length);
    memcpy(&saltedPassword.bytes[userPassword.length], salt.bytes, salt.length);

    // calcualte sha256 hash and use it as the key
    ByteArray key = sha_256(saltedPassword).digest;
    free(saltedPassword.bytes);
    return key;
}

