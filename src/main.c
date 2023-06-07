//
//  Created by Stasel
//

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include "utilities/utilities.h"
#include "filesystem/filesystem.h"
#include "security/passwordinput.h"

void show_help(void) {
    printf("Usage: secfs <secure folder> <mount point>\n\n");
}

int main(int argc, String argv[]) {
    
    if (argc < 3) {
        show_help();
        return 1;
    }
    
    String dataPath = argv[1];
    String mountPath = argv[2];
    
    // Append trailing '/' to dataPath if missing
    if (dataPath[strlen(dataPath) - 1] != '/') {
        dataPath = malloc(strlen(dataPath) + 1);
        strcpy(dataPath, argv[1]);
        strcat(dataPath, "/");
    }

    debugPrint("Data path: %s, mount path: %s", dataPath, mountPath);
    
    // check for existing secfs.
    Secfs *secfs;
    if (!is_existing_secfs(dataPath)) {
        // New secure folder
        printf("Couldn't find secfs in '%s'\nWould you like to initialize secfs in that directory? [y/n] ", dataPath);
        if (!boolPrompt()) {
            exit(EXIT_SUCCESS);
        }
        
        ByteArray iv = get_random_bytes(IV_LENGTH);
        ByteArray key = setup_password(iv);

        LoadSecfsResult initResult = init_secfs(dataPath, key, iv);
        if (initResult.error) {
            fatalError("Could not initialize secure folder: %s", initResult.error);
        }
        secfs = initResult.secfs;
        printf("\n\nSecfs folder has been successfully initialized at %s\n",dataPath);
        printf("!!! If you lose your password, you will lose access to your data. Please keep your password safe\n\n");
    }
    else {
        // Existing secure folder
        LoadIVResult ivResult = load_iv(dataPath);
        if (ivResult.error) {
            fatalError("Could not load secure folder: %s",ivResult.error);
        }
        ByteArray iv = ivResult.iv;
        ByteArray key;
        while (true) {
            key = enter_password(iv);
            if(verify_key(key, iv, dataPath)) {
                break;
            }
            printf("\nIncorrect password. Please try again\n");
        }
        
        LoadSecfsResult loadResult = load_secfs(dataPath, key);
        if (loadResult.error) {
            fatalError("Could not load secure folder: %s",loadResult.error);
        }
        secfs = loadResult.secfs;
    }

    printf("\n\n======================= Secfs is now running =======================\n");
    printf("Mount path (Working directory):\t\t%s\n",mountPath);
    printf("Secure data path (Encrypted storage):\t%s\n",dataPath);
    printf("====================================================================\n");
    fs_start(secfs, mountPath);
    
    return 0;
}

