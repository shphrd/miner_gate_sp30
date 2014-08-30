/*
 * Copyright 2014 Zvi (Zvisha) Shteingart - Spondoolies-tech.com
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 3 of the License, or (at your option)
 * any later version.  See COPYING for more details.
 *
 * Note that changing this SW will void your miners guaranty
 */


#include "ac2dc_const.h"
#include "ac2dc.h"
#include "dc2dc.h"
#include "i2c.h"
#include "asic.h"
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h> 

extern MINER_BOX vm;
extern pthread_mutex_t i2c_mutex;
static int mgmt_addr[5] = {0, AC2DC_MURATA_NEW_I2C_MGMT_DEVICE, AC2DC_MURATA_OLD_I2C_MGMT_DEVICE, AC2DC_EMERSON_1200_I2C_MGMT_DEVICE, 0};
static int eeprom_addr[5] = {0, AC2DC_MURATA_NEW_I2C_EEPROM_DEVICE, AC2DC_MURATA_OLD_I2C_EEPROM_DEVICE, AC2DC_EMERSON_1200_I2C_EEPROM_DEVICE, 0};
static int revive_code[5] = {0, 0x0, 0x0, 0x40,0};



static char psu_names[5][20] = {"NONE","NEW-MURATA","OLD-MURATA","EMERSON1200", "GENERIC"};

char* psu_get_name(int type) {
  passert(type < 5);
  return psu_names[type];
}


// Return Watts
/*
static int ac2dc_get_power(AC2DC *ac2dc, int psu_id) {
  int err = 0;
  static int warned = 0;
  int r;

#ifdef DUMMY_I2C_EMERSON_FIX
  emerson_workaround();  
#endif  

  if (err) {
    psyslog("RESET I2C BUS?\n");  
    /*
    system("echo 111 > /sys/class/gpio/export");
    system("echo out > /sys/class/gpio/gpio111/direction");
    system("echo 0 > /sys/class/gpio/gpio111/value");
    system("echo 1 > /sys/class/gpio/gpio111/value");
    usleep(1000000);
    system("echo 111 > /sys/class/gpio/unexport");
    passert(0);
    * /
  }
  if (err) {
    if ((warned++) < 10)
      psyslog( "FAILED TO INIT AC2DC\n" );
    if ((warned) == 9)
      psyslog( "FAILED TO INIT AC2DC, giving up :(\n" );
    return 100;
  }

  return power;
}
*/


bool ac2dc_check_connected(int top_or_bottom) {

  int err;
  bool ret = false;
  i2c_write(PRIMARY_I2C_SWITCH, 
   ((top_or_bottom == PSU_TOP)? PRIMARY_I2C_SWITCH_AC2DC_TOP_PIN : PRIMARY_I2C_SWITCH_AC2DC_BOTTOM_PIN) | PRIMARY_I2C_SWITCH_DEAULT, &err);  
  i2c_read_word(AC2DC_MURATA_OLD_I2C_MGMT_DEVICE, AC2DC_I2C_READ_TEMP_WORD, &err);
  if (!err) {
    ret= true;
  }

  
  i2c_read_word(AC2DC_EMERSON_1200_I2C_MGMT_DEVICE, AC2DC_I2C_READ_TEMP_WORD, &err);
    if (!err) {
     ret= true;
    }

  i2c_read_word(AC2DC_MURATA_NEW_I2C_MGMT_DEVICE, AC2DC_I2C_READ_TEMP_WORD, &err);
    if (!err) {
      ret= true;
    }
  return ret;

  i2c_write(PRIMARY_I2C_SWITCH, PRIMARY_I2C_SWITCH_DEAULT, &err);  
}


void ac2dc_init_one(AC2DC* ac2dc, int top_or_bottom) {
  
 int err;
 
 i2c_write(PRIMARY_I2C_SWITCH, 
   ((top_or_bottom == PSU_TOP)? PRIMARY_I2C_SWITCH_AC2DC_TOP_PIN : PRIMARY_I2C_SWITCH_AC2DC_BOTTOM_PIN) | PRIMARY_I2C_SWITCH_DEAULT, &err);  
 
     int res = i2c_read_word(AC2DC_MURATA_OLD_I2C_MGMT_DEVICE, AC2DC_I2C_READ_TEMP_WORD, &err);
      if (!err) {
#ifdef MINERGATE
        psyslog("OLD MURATA AC2DC LOCATED\n");
#endif
        ac2dc->ac2dc_type = AC2DC_TYPE_MURATA_OLD;
      } else {
        i2c_read_word(AC2DC_EMERSON_1200_I2C_MGMT_DEVICE, AC2DC_I2C_READ_TEMP_WORD, &err);
        if (!err) {
#ifdef MINERGATE
          psyslog("EMERSON 1200 AC2DC LOCATED\n");
#endif
          ac2dc->ac2dc_type = AC2DC_TYPE_EMERSON_1_2;
        } else {
          // NOT EMERSON 1200
          i2c_read_word(AC2DC_MURATA_NEW_I2C_MGMT_DEVICE, AC2DC_I2C_READ_TEMP_WORD, &err);
          if (!err) {
#ifdef MINERGATE
           psyslog("NEW MURATA AC2DC LOCATED\n");
#endif
           ac2dc->ac2dc_type = AC2DC_TYPE_MURATA_NEW;
          } else {
            // NOT MURATA 1200
            psyslog("UNKNOWN AC2DC - DISABLE AC2DC\n");
            ac2dc->ac2dc_type = AC2DC_TYPE_NONE;
            ac2dc->voltage = 0;
          }
        }
      }
    i2c_write(PRIMARY_I2C_SWITCH, 
      ((top_or_bottom == PSU_TOP)? PRIMARY_I2C_SWITCH_AC2DC_TOP_PIN : PRIMARY_I2C_SWITCH_AC2DC_BOTTOM_PIN) | PRIMARY_I2C_SWITCH_DEAULT, &err);  
    ac2dc->voltage = i2c_read_word(mgmt_addr[ac2dc->ac2dc_type], AC2DC_I2C_READ_VIN_WORD, &err);
#ifdef DUMMY_I2C_EMERSON_FIX
    emerson_workaround();  
#endif    
    ac2dc->voltage = i2c_getint(ac2dc->voltage );
  
#ifdef MINERGATE
    psyslog("INPUT VOLTAGE=%d\n", ac2dc->voltage);
#endif
    // Fix Emerson BUG
    i2c_write(PRIMARY_I2C_SWITCH, PRIMARY_I2C_SWITCH_DEAULT, &err);  
    i2c_write(PRIMARY_I2C_SWITCH, PRIMARY_I2C_SWITCH_DEAULT, &err);  

}



void ac2dc_init() {
  psyslog("DISCOVERING AC2DC BOTTOM\n");
  if(vm.ac2dc[PSU_TOP].ac2dc_no_i2c) {
    vm.ac2dc[PSU_TOP].ac2dc_type = AC2DC_TYPE_GENERIC;
  } else {
    vm.ac2dc[PSU_TOP].ac2dc_type = AC2DC_TYPE_NONE;
    psyslog("DISCOVERING AC2DC TOP\n");  
    ac2dc_init_one(&vm.ac2dc[PSU_TOP], PSU_TOP);
  }


  if(vm.ac2dc[PSU_BOTTOM].ac2dc_no_i2c) {
    vm.ac2dc[PSU_BOTTOM].ac2dc_type = AC2DC_TYPE_GENERIC;
  } else {
    vm.ac2dc[PSU_BOTTOM].ac2dc_type = AC2DC_TYPE_NONE;
    ac2dc_init_one(&vm.ac2dc[PSU_BOTTOM], PSU_BOTTOM);
  }
  
  i2c_write(PRIMARY_I2C_SWITCH, PRIMARY_I2C_SWITCH_DEAULT);
  // Disable boards if no DC2DC
  vm.board_present[BOARD_BOTTOM] = (vm.ac2dc[PSU_BOTTOM].ac2dc_type != AC2DC_TYPE_NONE); 
  vm.board_present[BOARD_TOP] = (vm.ac2dc[PSU_TOP].ac2dc_type != AC2DC_TYPE_NONE); 


  
}

#ifndef MINERGATE
int ac2dc_get_vpd(ac2dc_vpd_info_t *pVpd, int psu_id, AC2DC *ac2dc) {

  if (ac2dc->ac2dc_type == AC2DC_TYPE_NONE) {
     return 0;
  }


	int rc = 0;
	int err = 0;
  int pnr_offset = 0x34;
  int pnr_size = 15;
  int model_offset = 0x57;
  int model_size = 4;
  int serial_offset = 0x5b;
  int serial_size = 10;
  int revision_offset = 0x61;
  int revision_size = 2;

  if (ac2dc->ac2dc_type == AC2DC_TYPE_MURATA_NEW) // MURATA
  {
	  pnr_offset = 0x1D;
	  pnr_size = 21;
	  model_offset = 0x16;
	  model_size = 6;
	  serial_offset = 0x34;
	  serial_size = 12;
	  revision_offset = 0x3a;
	  revision_size = 2;
  }else if (ac2dc->ac2dc_type == AC2DC_TYPE_EMERSON_1_2) // EMRSN1200
  {
	  /*
	   *
5 (0x2c , 0x30) Vendor ID
12 (0x32, 0x3d) Product Name
12 (0x3F , 0x4A ) Product NR
2  (0x4c ,0x4d) REV
13 (0x4f , 0x5B)SNR
	   *
	   */
	  pnr_offset = 0x3F;
	  pnr_size = 12;
	  model_offset = 0x3F;
	  model_size = 12;
	  serial_offset = 0x4f;
	  serial_size = 13;
	  revision_offset = 0x4c;
	  revision_size = 2;

  }

  if (NULL == pVpd) {
    psyslog("call ac2dc_get_vpd performed without allocating sturcture first\n");
    return 1;
  }
  //psyslog("%s:%d\n",__FILE__, __LINE__);
  

  pthread_mutex_lock(&i2c_mutex);

  i2c_write(PRIMARY_I2C_SWITCH, ((psu_id == PSU_TOP)?PRIMARY_I2C_SWITCH_AC2DC_TOP_PIN:PRIMARY_I2C_SWITCH_AC2DC_BOTTOM_PIN) | PRIMARY_I2C_SWITCH_DEAULT , &err);

  if (err) {
    fprintf(stderr, "Failed writing to I2C address 0x%X (err %d)",
            PRIMARY_I2C_SWITCH, err);
     pthread_mutex_unlock(&i2c_mutex);
    return err;
  }

  for (int i = 0; i < pnr_size; i++) {
    pVpd->pnr[i] = ac2dc_get_eeprom_quick(pnr_offset + i, &err);
    if (err)
      goto ac2dc_get_eeprom_quick_err;
  }

  for (int i = 0; i < model_size; i++) {
    pVpd->model[i] = ac2dc_get_eeprom_quick(model_offset + i, &err);
    if (err)
      goto ac2dc_get_eeprom_quick_err;
  }

  for (int i = 0; i < revision_size; i++) {
    pVpd->revision[i] = ac2dc_get_eeprom_quick(revision_offset + i, &err);
    if (err)
      goto ac2dc_get_eeprom_quick_err;
  }


  for (int i = 0; i < serial_size; i++) {
    pVpd->serial[i] = ac2dc_get_eeprom_quick(serial_offset + i, &err);
    if (err)
      goto ac2dc_get_eeprom_quick_err;
  }


ac2dc_get_eeprom_quick_err:

  if (err) {
    fprintf(stderr, RED            "Failed reading AC2DC eeprom (err %d)\n" RESET, err);
    rc =  err;
  }

  i2c_write(PRIMARY_I2C_SWITCH, PRIMARY_I2C_SWITCH_DEAULT);
  pthread_mutex_unlock(&i2c_mutex);

  return rc;
}
#endif

// this function assumes as a precondition
// that the i2c bridge is already pointing to the correct device
// needed to read ac2dc eeprom
// no side effect either
// use this funtion when performing serial multiple reads
#ifndef MINERGATE
unsigned char ac2dc_get_eeprom_quick(int offset, AC2DC *ac2dc, int *pError) {

  if (ac2dc->ac2dc_type == AC2DC_TYPE_NONE) {
     return 0;
  }

  //pthread_mutex_lock(&i2c_mutex); no lock here !!

  unsigned char b =
      (unsigned char)i2c_read_byte(eeprom_addr[ac2dc->ac2dc_type], offset, pError);

  return b;
}
#endif

// no precondition as per i2c
// and thus sets switch first,
// and then sets it back
// side effect - it closes the i2c bridge when finishes.
#ifndef MINERGATE
int ac2dc_get_eeprom(int offset, int psu_id, AC2DC *ac2dc, int *pError) {
  // Stub for remo
  if (ac2dc->ac2dc_type == AC2DC_TYPE_NONE) {
    return 0;
  }
  //printf("%s:%d\n",__FILE__, __LINE__);
  pthread_mutex_lock(&i2c_mutex);
  int b;
  i2c_write(PRIMARY_I2C_SWITCH, ((psu_id == PSU_TOP)?PRIMARY_I2C_SWITCH_AC2DC_TOP_PIN:PRIMARY_I2C_SWITCH_AC2DC_BOTTOM_PIN) | PRIMARY_I2C_SWITCH_DEAULT, pError);
  if (pError && *pError) {
     pthread_mutex_unlock(&i2c_mutex);
    return *pError;
  }

  b = i2c_read_byte(eeprom_addr[ac2dc->ac2dc_type], offset, pError);
  i2c_write(PRIMARY_I2C_SWITCH, PRIMARY_I2C_SWITCH_DEAULT);
   pthread_mutex_unlock(&i2c_mutex);
  return b;
}
#endif





#ifdef MINERGATE
int update_work_mode(int decrease_top, int decrease_bottom, bool to_alternative);
void exit_nicely(int seconds_sleep_before_exit, const char* p);
static pthread_t ac2dc_thread;

void test_fix_ac2dc_limits() {
  int err;
  if (vm.ac2dc[PSU_TOP].ac2dc_type != AC2DC_TYPE_NONE) {
    i2c_write(PRIMARY_I2C_SWITCH, PRIMARY_I2C_SWITCH_AC2DC_TOP_PIN | PRIMARY_I2C_SWITCH_DEAULT);      
    AC2DC* ac2dc = &vm.ac2dc[PSU_TOP];
    int p = i2c_read_byte(mgmt_addr[ac2dc->ac2dc_type], AC2DC_I2C_READ_STATUS_IOUT, &err);
    if (!err) {
      int q = 0x1f;
      if (ac2dc->ac2dc_type == AC2DC_TYPE_EMERSON_1_2) {
        q = i2c_read_byte(mgmt_addr[ac2dc->ac2dc_type], AC2DC_I2C_READ_FAULTS, &err);     
      }
      psyslog("TOP PSU STAT:%x %x\n", p, q);
      if ((p & (AC2DC_I2C_READ_STATUS_IOUT_OP_ERR | AC2DC_I2C_READ_STATUS_IOUT_OC_ERR)) ||
              ((ac2dc->ac2dc_type == AC2DC_TYPE_EMERSON_1_2) && (q != 0x1f))) {            
        psyslog("AC2DC OVERCURRENT TOP:%x\n",p);
        i2c_write_byte(mgmt_addr[ac2dc->ac2dc_type],AC2DC_I2C_WRITE_PROTECT,0x0,&err);
        usleep(3000000);
        i2c_write_byte(mgmt_addr[ac2dc->ac2dc_type],AC2DC_I2C_ON_OFF,revive_code[ac2dc->ac2dc_type],&err);
        usleep(1000000);
        i2c_write_byte(mgmt_addr[ac2dc->ac2dc_type],AC2DC_I2C_ON_OFF,0x80,&err);

         if ((p & AC2DC_I2C_READ_STATUS_IOUT_OC_ERR) &&
            (vm.ac2dc[PSU_TOP].ac2dc_power_limit > 1270)) {
          mg_event_x("update TOP work mode %d", vm.ac2dc[PSU_TOP].ac2dc_power_limit);
          update_work_mode(5, 0, false);
        }     
        mg_event_x("AC2DC top fail on %d", vm.ac2dc[PSU_TOP].ac2dc_power_limit);
        vm.i2c_busy_with_bug = 0;    
        i2c_write(PRIMARY_I2C_SWITCH, PRIMARY_I2C_SWITCH_DEAULT);
        exit_nicely(4,"AC2DC fail 1");
      }
    }else {
      mg_event("top i2c miss");
    }
  }
  i2c_write(PRIMARY_I2C_SWITCH, PRIMARY_I2C_SWITCH_DEAULT);

  if (vm.ac2dc[PSU_BOTTOM].ac2dc_type != AC2DC_TYPE_NONE) {
    i2c_write(PRIMARY_I2C_SWITCH, PRIMARY_I2C_SWITCH_AC2DC_BOTTOM_PIN | PRIMARY_I2C_SWITCH_DEAULT);      
    AC2DC* ac2dc = &vm.ac2dc[PSU_BOTTOM];
    int p = i2c_read_byte(mgmt_addr[ac2dc->ac2dc_type], AC2DC_I2C_READ_STATUS_IOUT, &err);
    if (!err) {
      int q = 0x1f;
      if (ac2dc->ac2dc_type == AC2DC_TYPE_EMERSON_1_2) {
        q = i2c_read_byte(mgmt_addr[ac2dc->ac2dc_type], AC2DC_I2C_READ_FAULTS, &err);    
      }
      psyslog("BOT PSU STAT:%x %x\n", p, q);
      if ((p & (AC2DC_I2C_READ_STATUS_IOUT_OP_ERR | AC2DC_I2C_READ_STATUS_IOUT_OC_ERR)) ||
           ((ac2dc->ac2dc_type == AC2DC_TYPE_EMERSON_1_2) && (q != 0x1f))) {
        psyslog("AC2DC OVERCURRENT BOTTOM:%x\n",p);
        i2c_write_byte(mgmt_addr[ac2dc->ac2dc_type],AC2DC_I2C_WRITE_PROTECT,0x0,&err);
        usleep(3000000);
        i2c_write_byte(mgmt_addr[ac2dc->ac2dc_type],AC2DC_I2C_ON_OFF,revive_code[ac2dc->ac2dc_type],&err);
        usleep(1000000);
        i2c_write_byte(mgmt_addr[ac2dc->ac2dc_type],AC2DC_I2C_ON_OFF,0x80,&err);
        if ((p & AC2DC_I2C_READ_STATUS_IOUT_OC_ERR) &&
            (vm.ac2dc[PSU_BOTTOM].ac2dc_power_limit > 1270)) {
          mg_event_x("update bottom work mode %d", vm.ac2dc[PSU_TOP].ac2dc_power_limit);
          update_work_mode(0, 5, false);
        }
        mg_event_x("AC2DC bottom fail on %d", vm.ac2dc[PSU_BOTTOM].ac2dc_power_limit);
        vm.i2c_busy_with_bug = 0;    
        i2c_write(PRIMARY_I2C_SWITCH, PRIMARY_I2C_SWITCH_DEAULT);
        exit_nicely(4,"AC2DC fail 2"); 
      }
    } else {
      mg_event("bottom i2c miss");
    }
  }
  i2c_write(PRIMARY_I2C_SWITCH, PRIMARY_I2C_SWITCH_DEAULT);  


}


void update_single_psu(AC2DC *ac2dc, int i2c_switch, int top_or_bot) {
   int p;
   int err;
   struct timeval tv;
   start_stopper(&tv);
//   i2c_write(PRIMARY_I2C_SWITCH, i2c_switch);  
   i2c_write(PRIMARY_I2C_SWITCH, i2c_switch);
   
   p = i2c_read_word(mgmt_addr[ac2dc->ac2dc_type], AC2DC_I2C_READ_PIN_WORD, &err);
   if (!err) {
     ac2dc->ac2dc_in_power = i2c_getint(p); 
     DBG( DBG_SCALING ,"PowerIn: %d\n", ac2dc->ac2dc_in_power);
   } else {
     //ac2dc->ac2dc_in_power = 0;
     DBG( DBG_SCALING ,"PowerIn: Error\n", ac2dc->ac2dc_in_power);
   }
   
//   i2c_write(PRIMARY_I2C_SWITCH, i2c_switch);  
//   i2c_write(PRIMARY_I2C_SWITCH, i2c_switch);      
   p = i2c_read_word(mgmt_addr[ac2dc->ac2dc_type], AC2DC_I2C_READ_POUT_WORD, &err);
   if (!err) {
     int pp = i2c_read_byte(mgmt_addr[ac2dc->ac2dc_type], AC2DC_I2C_READ_STATUS_IOUT, &err);
     DBG( DBG_SCALING ,"PSU STAT:%x\n", pp);
     ac2dc->ac2dc_power_last_last = ac2dc->ac2dc_power_last;
     ac2dc->ac2dc_power_last = ac2dc->ac2dc_power_now;      
     ac2dc->ac2dc_power_now = i2c_getint(p); 
     ac2dc->ac2dc_power = MAX(ac2dc->ac2dc_power_now, ac2dc->ac2dc_power_last);
     ac2dc->ac2dc_power = MAX(ac2dc->ac2dc_power, ac2dc->ac2dc_power_last_last);     
     DBG( DBG_SCALING ,"PowerOut: R:%d PM:%d [PL:%d PLL:%d PLLL:%d]\n", 
                 p,
                 ac2dc->ac2dc_power,
                 ac2dc->ac2dc_power_now,
                 ac2dc->ac2dc_power_last,
                 ac2dc->ac2dc_power_last_last);
   } else {
     DBG( DBG_SCALING ,"PowerOut: Error\n", ac2dc->ac2dc_power);
   }
//   i2c_write(PRIMARY_I2C_SWITCH, i2c_switch);  
//   i2c_write(PRIMARY_I2C_SWITCH, i2c_switch);      
   i2c_write(PRIMARY_I2C_SWITCH, PRIMARY_I2C_SWITCH_DEAULT);  
   
   int usecs = end_stopper(&tv,NULL);
   if (vm.bad_ac2dc[top_or_bot] == 0 && (usecs > 500000)) {
     vm.i2c_busy_with_bug = 0;    
     vm.bad_ac2dc[top_or_bot] = 1;
     mg_event_x("Bad PSU FW %s", (top_or_bot == PSU_TOP)?"TOP":"BOTTOM");  
     exit_nicely(0,"Bad PSU Firmware");
   }
}



void *update_ac2dc_power_measurments_thread(void *ptr) {
  //struct timeval tv;
  //start_stopper(&tv);
  pthread_mutex_lock(&i2c_mutex);  
  //do_stupid_i2c_workaround();
  int err;  
  //static int    psu_id;
  AC2DC *ac2dc;
  int p;
  ac2dc = &vm.ac2dc[PSU_TOP];
  if (!vm.ac2dc[PSU_TOP].ac2dc_no_i2c) {
    if (ac2dc->ac2dc_type != AC2DC_TYPE_NONE) {
      update_single_psu(ac2dc, PRIMARY_I2C_SWITCH_AC2DC_TOP_PIN | PRIMARY_I2C_SWITCH_DEAULT, PSU_TOP);
    } else {
      if (ac2dc_check_connected(PSU_TOP)) {
        vm.i2c_busy_with_bug = 0;
        exit_nicely(10,"AC2DC connected top");
      }
    }
  }


  
  if (!vm.ac2dc[PSU_BOTTOM].ac2dc_no_i2c) {
    ac2dc = &vm.ac2dc[PSU_BOTTOM];
    if (ac2dc->ac2dc_type != AC2DC_TYPE_NONE) {
      update_single_psu(ac2dc, PRIMARY_I2C_SWITCH_AC2DC_BOTTOM_PIN | PRIMARY_I2C_SWITCH_DEAULT, PSU_BOTTOM);
    } else {
      if (ac2dc_check_connected(PSU_BOTTOM)) {
        vm.i2c_busy_with_bug = 0;
        exit_nicely(10, "AC2DC connected bottom");
      }
    }
  }
  
  //int usecs = end_stopper(&tv,"ac2dc update");
  
  pthread_mutex_unlock(&i2c_mutex);
  vm.i2c_busy_with_bug = 0;
  //pthread_exit(NULL);
  return NULL;
}

int update_ac2dc_power_measurments() {
  DBG(DBG_SCALING,"update_ac2dc_power_measurments start\n");
  update_ac2dc_power_measurments_thread(NULL);
  DBG(DBG_SCALING,"update_ac2dc_power_measurments stop\n");  
  /*
  struct sched_param param;
  param.sched_priority = sched_get_priority_max(SCHED_RR);
  
  pthread_attr_t tattr;
  pthread_attr_init(&tattr);
  pthread_attr_setschedpolicy(&tattr,SCHED_RR); 
  pthread_attr_getschedparam(&tattr, &param);
  pthread_attr_setdetachstate(&tattr,PTHREAD_CREATE_DETACHED);
 
  passert(vm.i2c_busy_with_bug == 0)
  vm.i2c_busy_with_bug = 1;
  int ret = pthread_create(&ac2dc_thread,  &tattr, update_ac2dc_power_measurments_thread, (void *)NULL);
  pthread_attr_destroy(&tattr); 
  */
}






#endif
