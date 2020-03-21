#include <stdint.h>
#include <string.h>
#include "mem_utils.h"
#include "mem_utils_conf.h"

static uint8_t  mu_buff[MU_MAX_RW_SIZE];

void Crc16Ini();
uint16_t Crc16(uint8_t * pcBlock,uint16_t len);

//Addres of magic in cells
#define archive_stamp(unit,addr)  addr+unit->size-8
//Index of magic in last page 
#define archive_stamp_index(unit) unit->storage->rw_size-8

static uint32_t check_arch_magic(uint8_t* buf){
  //Check control summ
  if(buf[5]!=((buf[0]+buf[1]+buf[2]+buf[3]+buf[4])&0xFF)){
    return 0;
  }
  //Check acrchive version
  if(buf[4]!=MU_ARCH_VER){
    return 0;
  }
  //Parse timestamp
  uint32_t ret=buf[3]; ret<<=8;
           ret+=buf[2]; ret<<=8;
           ret+=buf[1]; ret<<=8;
           ret+=buf[0];
  return ret;
}

static void set_arch_magic(uint8_t* buf,uint32_t stump){
  //Build timestamp
  buf[0]=(uint8_t)stump; stump>>=8;
  buf[1]=(uint8_t)stump; stump>>=8;
  buf[2]=(uint8_t)stump; stump>>=8;
  buf[3]=(uint8_t)stump; 
  //Add archive version
  buf[4]=MU_ARCH_VER;
  //Calculate controll summ
  buf[5]=buf[0]+buf[1]+buf[2]+buf[3]+buf[4];
}

static void arch_ini(mu_unit_t* unit){
  if(unit->init_flag==0){
    //First timestamp
    unit->stamp=1;
    //End of area of data cells in memory    
    unit->end=unit->begin+(unit->size*unit->copy_qty);
    //Start of cell addres
    uint32_t read_addres=unit->begin;
    //End of cell addres
    uint32_t read_stop_addres=unit->begin+unit->size;
    //Min max timestamps for seek oldes and newest cell 
    uint32_t min_stamp=0xFFFFFFFF;
    uint32_t max_stamp=0;
    //Temprory timestamp
    uint32_t stamp=0;
    //Count of used cells
    unit->copy_cnt=0;
    //Test memory area
    unit->oldest=read_addres;
    unit->newest=unit->end-unit->size;
    while(read_addres<unit->end){
      //Read magic 
      unit->storage->read(archive_stamp(unit,read_addres),mu_buff,6);
      stamp=check_arch_magic(mu_buff);
      if(stamp){ //Magic is ok
        //CRC check
        Crc16Ini();
        uint16_t crc_calc=1;
        for(uint32_t addres=read_addres;addres<read_stop_addres;
            addres+=unit->storage->rw_size){
          unit->storage->read(read_addres,mu_buff,unit->storage->rw_size);
          if(addres==(read_stop_addres-unit->storage->rw_size)){
            crc_calc=Crc16(mu_buff,unit->storage->rw_size-2);
          }else{
            crc_calc=Crc16(mu_buff,unit->storage->rw_size);
          }
        }
        uint16_t read_crc=mu_buff[unit->storage->rw_size-1];   read_crc<<=8;
        read_crc+=mu_buff[unit->storage->rw_size-2];
        if(crc_calc==read_crc){ //CRC valid      
          unit->copy_cnt++;     
          if(stamp>max_stamp){
            stamp=max_stamp;
            unit->newest=read_addres;
            unit->stamp=min_stamp; //Found newest timestamp
          }
          if(stamp<min_stamp){
            stamp=min_stamp;
            unit->oldest=read_addres;
          }
        }
      }
      read_stop_addres+=unit->size;
      read_addres+=unit->size;
    }
    unit->init_flag=1;
  }
}



static int8_t mu_read_data(mu_unit_t* unit,uint8_t* Buffer, uint16_t len, uint32_t* r_stamp,uint32_t* read_addr){
  arch_ini(unit);
  if(unit->copy_cnt==0){
    return -2; //No data
  }
  //Seek cell
  uint16_t try_count=unit->copy_qty; 
  while(try_count--){
    //Check magic
    unit->storage->read(archive_stamp(unit,*read_addr),mu_buff,6);
    uint32_t stamp=check_arch_magic(mu_buff);
    if(stamp){ //Magic OK
      *r_stamp=stamp;
      uint32_t read_stop_addres=*read_addr+unit->size;
      //Read oldest data 
      uint16_t index=0;
      Crc16Ini();
      uint16_t crc_calc=1;
      for(uint32_t addres=*read_addr;addres<read_stop_addres;
          addres+=unit->storage->rw_size){
        if(len>unit->storage->rw_size){ //read start cell
          unit->storage->read(addres,&Buffer[index],unit->storage->rw_size);
          crc_calc=Crc16(&Buffer[index],unit->storage->rw_size);
          len-=unit->storage->rw_size;
        }else{ 
          unit->storage->read(addres,mu_buff,unit->storage->rw_size);
          crc_calc=Crc16(mu_buff,unit->storage->rw_size-2);
          if(len){ //read last page 
            memcpy(&Buffer[index],mu_buff,len);
          }
        }
        index+=unit->storage->rw_size;
      }
      uint16_t read_crc=mu_buff[unit->storage->rw_size-1];   read_crc<<=8;
      read_crc+=mu_buff[unit->storage->rw_size-2];
      if(crc_calc==read_crc){
        return 0;
      }
    }
    *read_addr=*read_addr+unit->size;
    if(*read_addr>=unit->end){
      *read_addr=unit->begin;
    }
  }
  return -1; //No data
}

int8_t mu_read_arch_data(mu_unit_t* unit,uint8_t* Buffer, uint16_t len, uint32_t* r_stamp){
  return mu_read_data(unit,Buffer,len,r_stamp,&unit->oldest);
}

int8_t mu_read_last_data(mu_unit_t* unit,uint8_t* Buffer, uint16_t len, uint32_t* r_stamp){
  return mu_read_data(unit,Buffer,len,r_stamp,&unit->newest);
}

void mu_delete_arch_data(mu_unit_t* unit){
  if((unit->copy_cnt==0)||(unit->init_flag==0)){
    return; //No data
  }
  //Write corrupted data in last page of oldest cell (with magic and CRC)
  //For EEPROM it write all bytes to 0xA5               0b11110000->0b10100101
  //For NOR it set to zero bits 6,4,3,1 in every byte   0b11110000->0b10100000
  memset(mu_buff,0xA5,unit->storage->rw_size);
  unit->storage->write(unit->oldest+unit->size-unit->storage->rw_size,
                       mu_buff,unit->storage->rw_size);
  unit->oldest+=unit->size;
  if(unit->oldest>=unit->end){
    unit->oldest=unit->begin;
  }
  unit->copy_cnt--;
}

static int8_t check_area_empty(mu_unit_t* unit,uint32_t addres){
   uint32_t read_stop_addres=addres+unit->size;
   for( ;addres<read_stop_addres;addres+=unit->storage->rw_size){
    unit->storage->read(addres,mu_buff,unit->storage->rw_size);
    for(uint16_t i=0;i<unit->storage->rw_size;i++){
      if(mu_buff[i]!=0xFF){
        return -1;
      }
    }
  }
  return 0;
}


void mu_write_arch_data(mu_unit_t* unit,uint8_t* Buffer, uint16_t len, uint32_t stamp){
  arch_ini(unit);
  //Set timestamp
  if(stamp==0){ //No custom timestamp 
    unit->stamp++;
  }else{
    if(unit->stamp>=stamp){ //Wrong custom stamp
      unit->stamp++;
    }else{ //Correct custom stamp
      unit->stamp=stamp;
    }
  }
  //Seek cell
  uint16_t try_count=unit->copy_qty; 
  uint32_t write_addr=unit->newest;
  while(try_count--){
    //Calculate new write addres
    write_addr+=unit->size;
    if(write_addr>=unit->end){
      write_addr=unit->begin;
    }
    uint32_t write_addr_end=write_addr+unit->size;
    uint16_t index=0;
    uint16_t wr_end=0;
    Crc16Ini();
    uint16_t crc_calc=1;
    //Ini CRC
    for(uint32_t addres=write_addr;addres<write_addr_end;addres+=unit->storage->rw_size){
      //Check if erase neeaded
      if(unit->storage->er_size){ //Erase before write
        #if MU_ERASE_SIZE_POWER_OF_2 > 0
        if((addres&(unit->storage->er_size-1))==0)
        #else
        if((addres%unit->storage->er_size)==0)
        #endif
        {unit->storage->erase(addres);}
        if((addres==write_addr)&&(check_area_empty(unit,write_addr)!=0)){
          break;
        }
      }
      //Write data
      if(len>=unit->storage->rw_size){ //Full page
        unit->storage->write(addres,&Buffer[index],unit->storage->rw_size);
        crc_calc=Crc16(&Buffer[index],unit->storage->rw_size);
        len-=unit->storage->rw_size;
      }else{
        if(len){ //Last page with data 
          memcpy(mu_buff,&Buffer[index],len);
          memset(&mu_buff[len],0x5A,unit->storage->rw_size-len-2);
          len=0;
        }else{   //Page without data
          memset(mu_buff,0x5A,unit->storage->rw_size-2);
        }
        if(addres==(write_addr_end-unit->storage->rw_size)){ //Last page in cell
          //add magic and CRC   
          set_arch_magic(&mu_buff[unit->storage->rw_size-8],unit->stamp);
          crc_calc=Crc16(mu_buff,unit->storage->rw_size-2);
          mu_buff[unit->storage->rw_size-2]=crc_calc;  crc_calc>>=8;
          mu_buff[unit->storage->rw_size-1]=crc_calc;
          wr_end=1;
        }else{ //Other pages in cell
          crc_calc=Crc16(mu_buff,unit->storage->rw_size);
        }
        //Write page
        unit->storage->write(addres,mu_buff,unit->storage->rw_size);
      }
      index+=unit->storage->rw_size;
    }
    if(wr_end){
      //Save new wrire addres
      unit->newest=write_addr;
      unit->copy_cnt++;
      if(unit->copy_cnt>unit->copy_qty){
        unit->copy_cnt=unit->copy_qty;
      }
      return;
    }
  }
}




static uint16_t crc = 0xFFFF;
void Crc16Ini(){
  crc = 0xFFFF;
}
#if MU_CRC_FAST==0
/*
  Name  : CRC-16 CCITT
  Poly  : 0x1021    x^16 + x^12 + x^5 + 1
  Init  : 0xFFFF
  Revert: false
  XorOut: 0x0000
  Check : 0x29B1 ("123456789")
  MaxLen: 4095 bytes
*/

uint16_t Crc16(uint8_t * pcBlock,uint16_t len){
    while (len--){
        crc ^= *pcBlock++ << 8;
        for (uint8_t i = 0; i < 8; i++){
            crc = crc & 0x8000 ? (crc << 1) ^ 0x1021 : crc << 1;
        }
    }
    return crc;
}

#else

const unsigned short Crc16Table[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
    0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
    0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
    0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
    0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
    0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
    0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
    0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
    0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
    0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
    0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
    0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
    0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
    0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
    0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
    0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
    0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
    0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
    0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
    0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
    0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
    0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0
};

uint16_t Crc16(uint8_t * pcBlock,uint16_t len){
    while (len--){
        crc = (crc << 8) ^ Crc16Table[(crc >> 8) ^ *pcBlock++];
    }
    return crc;
}

#endif