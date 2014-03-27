/* 2011-06-10: File added by Sony Corporation */
/*
 * Copyright (c) 2010 Sony Corporation.
 * All rights reserved.
 */

typedef enum
{
    WPC_MODULE_WIFI=0,
    WPC_MODULE_BLUETOOTH=1,
    WPC_MODULE_GPS=2,
    WPC_MODULE_WWAN=3,
    WPC_MODULE_WWAN_RF=4,
    WPC_MODULE_NONE=-1,
} WPCModuleType;

int wireless_power_control(int module, int on_off);
int wireless_power_status(int module, int* p_on_off);

