//
//  Created by Stasel
//

#include <string.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "../utilities/utilities.h"
#include "passwordinput.h"
#include "encryption.h"

#define MAX_PASSWORD_LENGTH 50

void setStdinEcho(Bool enable) {
    struct termios tty;
    tcgetattr(STDIN_FILENO, &tty);
    if( !enable )
        tty.c_lflag &= ~(tcflag_t)ECHO;
    else
        tty.c_lflag |= ECHO;

    (void) tcsetattr(STDIN_FILENO, TCSANOW, &tty);
}

void getPassword(char* out) {
    setStdinEcho(false);
    char *result = fgets(out, MAX_PASSWORD_LENGTH, stdin);
    debugPrint("%s",result);
    setStdinEcho(true);
}

// Create new password flow
ByteArray setup_password(ByteArray salt) {
    char password[MAX_PASSWORD_LENGTH];
    char confirmPassword[MAX_PASSWORD_LENGTH];
    
    while(true) {
        printf("Please create a new ecryption password: ");
        getPassword(password);
        
        if (strlen(password) < 9) {
            printf("\nPassword is too short, please use at least 8 characters\n\n");
            continue;
        }

        printf("\nPlease confirm your password: ");
        getPassword(confirmPassword);

        if (strcmp(password, confirmPassword) != 0) {
            printf("\nPasswords do not match\n\n");
            continue;
        }
        break;
    }
    
    ByteArray passwordBytes = { (Byte*)password, (UInt)strlen(password) };
    return generate_key(passwordBytes, salt);
}

// Enter existing password flow
ByteArray enter_password(ByteArray salt) {
    char password[50];
    
    while(true) {
        printf("\nPlease enter the ecryption password: ");
        getPassword(password);
        
        if (strlen(password) == 1) {
            continue;
        }
        break;
    }
    
    ByteArray passwordBytes = { (Byte*)password, (UInt)strlen(password) };
    return generate_key(passwordBytes, salt);
}
