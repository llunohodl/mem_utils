#ifndef MEM_UTILS_CONF_H_
#define MEM_UTILS_CONF_H_

#include <stdint.h>

/** Select CRC function
  * 0 - slow calculation but less ROM usagege
  * 1 - fast CRC calculation with table (table in ROM 512 bytes) 
*/
#define MU_CRC_FAST 0 

//Maximal read/write size 
#define MU_MAX_RW_SIZE 256 
#define MU_ERASE_SIZE_POWER_OF_2 1
 
#define MU_ARCH_VER  1

#endif //MEM_UTILS_CONF_H_
