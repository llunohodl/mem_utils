#ifndef MEM_UTILS_H_
#define MEM_UTILS_H_

#include <stdint.h>
#include <string.h>


#ifndef NULL
#define NULL ((void*)0)
#endif


#define NOR_ARCH_VER    0

//Storage specifier (EEPROM / FLASH depended params)
typedef struct{
  //All adresses and sizes in bytes
  uint32_t start_addres;        //Start addres in memory                  
  uint32_t  end_addres;         //End addres in memory
  uint16_t rw_size;             //minimum read/write uinit size 
  uint16_t er_size;             //minimum erase uinit size 0 for EEPROM
  //Erase function return 0 if succeed
  int8_t (*erase)(uint32_t Addr);
  //Write function return 0 if succeed
  int8_t (*write)(uint32_t Addr,uint8_t* data, uint16_t len);
  //Read  function return 0 if succeed
  int8_t (*read)(uint32_t Addr,uint8_t* data, uint16_t len);
}mu_storage_t;

//Memory uint (set of cells with data)
typedef struct{
  //All adresses and sizes in bytes
  mu_storage_t* storage;       //Used storage
  uint32_t begin;              //Start addres in memory
  uint32_t size;               //Size of data 
  uint32_t end;                //End addres in memory 
  uint16_t copy_qty;           //Number of data copies
  uint16_t copy_cnt;           //Count of avaleble copies
  uint32_t newest;             //Newest element addres
  uint32_t oldest;             //Oldest element addres
  uint32_t stamp;              //Current (newest) timestamp          
  uint8_t init_flag;
}mu_unit_t;

//Macro for init EEPROM storage params
#define mu_eeprom_storage(name,Start,Size,Page_sz,Read,Write)\
mu_storage_t name={.start_addres=Start,.end_addres=Start+Size-1,\
                   .rw_size=Page_sz,.er_size=0,\
                   .write=Write,.read=Read,.erase=NULL};\
uint32_t const name##_start_addr=Start;\
uint32_t const name##_page_sz=Page_sz;\
uint32_t const name##_block_sz=0;

//Macro for init NOR flash storage params
#define mu_nor_storage(name,Start,Size,Page_sz,Block_sz,Read,Write,Erase)\
mu_storage_t name={.start_addres=Start,.end_addres=Start+Size-1,\
                   .rw_size=Page_sz,.er_size=Block_sz,\
                   .write=Write,.read=Read,.erase=Erase};

//Macro to calculate page-sized cell size 
#define mu_allign_sz(size,page_sz) \
((((size+8)/page_sz)+((size+8)%page_sz ? 1 : 0))*page_sz)

//
//Macro for add new archive area in storage
#define mu_archive(name,Storage,Begin,Size,Copy_qty,Prev_name)\
mu_unit_t name = {.storage=&Storage,.size=Size,      \
                  .begin=Begin,.end=Begin+Size*Copy_qty,     \
                  .copy_qty=Copy_qty,.init_flag=0};                             



/* Read data from archive (FIFO mode)
 *  uinit: archive 
 *  Buffer,len: Out data pointer and buffer lenght
 *  r_stamp: timestamp of element
 */ 
int8_t mu_read_arch_data(mu_unit_t* unit,uint8_t* Buffer, uint16_t len, uint32_t* r_stamp);
/* Delete oldest dtata in archive
 *  uinit: archive 
 */ 
void mu_delete_arch_data(mu_unit_t* unit);

/* Read data from archive (FILO mode)
 *  uinit: archive 
 *  Buffer,len: Out data pointer and buffer lenght
 *  r_stamp: timestamp of element
 */ 
int8_t mu_read_last_data(mu_unit_t* unit,uint8_t* Buffer, uint16_t len, uint32_t* r_stamp);

/* Write data to the archive, the oldest data will be overwritten with new data, 
 * if there is not enough free space
 *  uinit: archive 
 *  Buffer,len: Out data pointer and buffer lenght
 *  stamp: timestamp of element (0 if not need specify custom)
 */ 
void mu_write_arch_data(mu_unit_t* unit,uint8_t* Buffer, uint16_t len, uint32_t stamp);


#endif //MEM_UTILS_H_