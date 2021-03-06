/*
 * Copyright (C) 2011 Sony Corporation
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2, as
 * published by the Free Software Foundation.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
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

