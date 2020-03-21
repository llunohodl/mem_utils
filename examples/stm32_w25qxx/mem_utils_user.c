#include "mem_utils.h"
#include "w25qxx.h"

int8_t NOR_Write(uint32_t Addr,uint8_t* data, uint16_t len){
  W25qxx_WritePage(data,Addr>>8,0,len);
  return 0;
}

int8_t NOR_Read(uint32_t Addr,uint8_t* data, uint16_t len){
  W25qxx_ReadBytes(data,Addr,len);
  return 0;
}

int8_t NOR_Erase(uint32_t Addr){
  W25qxx_EraseSector(Addr>>12);
  return 0;
}

mu_nor_storage(NOR,1024*1024*3,1024*1024,256,4096,NOR_Read,NOR_Write,NOR_Erase);
mu_archive(Params,NOR,1024*1024*3,mu_allign_sz(680-8,256),16,0);  //32 cells in two blocks

static uint32_t temp[120];
static uint32_t Stamp=1584778798;
static uint32_t rStamp=0;
static uint32_t Arch_err_cnt=0;

#define MK_DATA()\
for(int i=0;i<120;i++){\
  temp[i]=Stamp+i*3571+i*1031;\
}

#define TST_DATA()\
for(int i=0;i<120;i++){\
  if(temp[i]!=rStamp+i*3571+i*1031){\
    Arch_err_cnt++; break;\
  }\
}

void NOR_test(){
  Arch_err_cnt=0;
  Stamp++;
  MK_DATA();
  mu_write_arch_data(&Params,(uint8_t*)temp,120*sizeof(uint32_t),Stamp);
   
  Stamp++;
  MK_DATA();
  mu_write_arch_data(&Params,(uint8_t*)temp,120*sizeof(uint32_t),0);
     
  for(int i=0;i<35;i++){
    Stamp++;
    MK_DATA();
    mu_write_arch_data(&Params,(uint8_t*)temp,120*sizeof(uint32_t),0);
    
    if(mu_read_arch_data(&Params,(uint8_t*)temp,120*sizeof(uint32_t),&rStamp)==0){
      TST_DATA();
    }else{
      Arch_err_cnt++;
    }
    mu_delete_arch_data(&Params);
  }
}
