//
//  Created by Stasel
//

#ifndef passwordinput_h
#define passwordinput_h

#include "../utilities/utilities.h"

ByteArray setup_password(ByteArray salt);
ByteArray enter_password(ByteArray salt);

#endif /* passwordinput_h */
