/* 2011-06-10: File added and changed by Sony Corporation */
/*
 * cmv59dx.c - cmv59dx camera driver
 *
 * Copyright (C) 2010, 2011 Sony Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <media/cmv59dx.h>
#include <linux/crc16.h>
#include <mach/gpio.h>
#include <../gpio-names.h>
#include <linux/time.h>
#include <media/tegra_camera.h>

#define GPIO_PIN_5M_28V         TEGRA_GPIO_PL0
#define GPIO_PIN_5M_18V         TEGRA_GPIO_PL1
#define GPIO_PIN_5M_12V         TEGRA_GPIO_PL2
#define GPIO_PIN_5M_RST         TEGRA_GPIO_PL3
#define GPIO_PIN_5M_PWRDWN      TEGRA_GPIO_PL4
#define GPIO_PIN_5M_28V_VCM     TEGRA_GPIO_PL5
#define GPIO_PIN_VGA_EN         TEGRA_GPIO_PL6
#define GPIO_PIN_VGA_RST        TEGRA_GPIO_PL7

struct cmv59dx_reg {
    u16 addr;
    u16 val;
};

struct cmv59dx_info {
    struct i2c_client *i2c_client;
    struct cmv59dx_platform_data *pdata;
};

#define POLL_BIT        0
#define POLL_VALUE      0
#define POLL_DELAY      0
#define POLL_TIMEOUT    0

#define BITFIELD_BIT    0
#define BITFIELD_VALUE  0

#define POLL_FIELD_VALUE        0
#define POLL_FIELD_DELAY        0
#define POLL_FIELD_TIMEOUT      0

#define SEQ_CMD             1
#define AE_STATUS_READY     2

#define CMV59DX_TABLE_WAIT_MS 0
#define CMV59DX_TABLE_END 1
#define CMV59DX_MAX_RETRIES 3

#define SEQUENCE_END (0xFFFF)
#define SEQUENCE_WAIT_MS (0xFFFE)
#define CMV59DX_POLL_REG (0xFFFD)
#define CMV59DX_BITFIELD (0xFFFC)
#define CMV59DX_POLL_FIELD (0xFFFB)

#define CMV59DX_MCU_ADDR      0x98C
#define CMV59DX_MCU_DATA      0x990


static struct cmv59dx_reg InitSequence[] =
{
//1.Initialize camera
    {0x001A, 0x0003},

    {SEQUENCE_WAIT_MS, 1},//1

    {0x001A, 0x0000},

    {SEQUENCE_WAIT_MS, 1},//1

    {0x0018, 0x4028}, 
    {0x001A, 0x0200},
    {0x001E, 0x0777},//0x0777
    {0x0016, 0x42DF},

    {CMV59DX_POLL_REG, 0x301A},//add poll_reg
    {POLL_BIT, 0x0004},
    {POLL_VALUE, 0},
    {POLL_DELAY, 10},
    {POLL_TIMEOUT, 100},

    {CMV59DX_MCU_ADDR,0x02F0},
    {CMV59DX_MCU_DATA,0x0000},
    {CMV59DX_MCU_ADDR,0x02F2},
    {CMV59DX_MCU_DATA,0x0210},
    {CMV59DX_MCU_ADDR,0x02F4},
    {CMV59DX_MCU_DATA,0x001A},
    {CMV59DX_MCU_ADDR,0x2145},
    {CMV59DX_MCU_DATA,0x02F4},
    {CMV59DX_MCU_ADDR,0xA134},
    {CMV59DX_MCU_DATA,0x0001},

    {CMV59DX_MCU_ADDR,0x1078},
    {CMV59DX_MCU_DATA,0x0000},

//2.Image set ExtClk=18MHz,op_pix=28MHz
    {CMV59DX_BITFIELD,0x14},
    {BITFIELD_BIT,1},
    {BITFIELD_VALUE,1},

    {CMV59DX_BITFIELD,0x14},
    {BITFIELD_BIT,2},
    {BITFIELD_VALUE,0},

    {0x0010, 0x0225},
    {0x0012, 0x0000},
    {0x0014, 0x244B},
    {SEQUENCE_WAIT_MS, 1},//1
    {0x0014, 0x304B},

    {CMV59DX_POLL_REG, 0x0014},//add poll_reg
    {POLL_BIT, 0x8000},
    {POLL_VALUE, 0},
    {POLL_DELAY, 50},
    {POLL_TIMEOUT, 20},

    {CMV59DX_BITFIELD,0x14},
    {BITFIELD_BIT,1},
    {BITFIELD_VALUE,0},

    {CMV59DX_MCU_ADDR,0x2703},
    {CMV59DX_MCU_DATA,0x0280},
    {CMV59DX_MCU_ADDR,0x2705},
    {CMV59DX_MCU_DATA,0x01E0},
    {CMV59DX_MCU_ADDR,0x2707},
    {CMV59DX_MCU_DATA,0x0280},
    {CMV59DX_MCU_ADDR,0x2709},
    {CMV59DX_MCU_DATA,0x01E0},
    {CMV59DX_MCU_ADDR,0x270D},
    {CMV59DX_MCU_DATA,0x0000},
    {CMV59DX_MCU_ADDR,0x270F},
    {CMV59DX_MCU_DATA,0x0000},
    {CMV59DX_MCU_ADDR,0x2711},
    {CMV59DX_MCU_DATA,0x01E7},
    {CMV59DX_MCU_ADDR,0x2713},
    {CMV59DX_MCU_DATA,0x0287},
    {CMV59DX_MCU_ADDR,0x2715},
    {CMV59DX_MCU_DATA,0x0001},
    {CMV59DX_MCU_ADDR,0x2717},
#ifdef CONFIG_MACH_NBX03
    {CMV59DX_MCU_DATA,0x0026},
#else
    {CMV59DX_MCU_DATA,0x0025},
#endif
    {CMV59DX_MCU_ADDR,0x2719},
    {CMV59DX_MCU_DATA,0x001A},
    {CMV59DX_MCU_ADDR,0x271B},
    {CMV59DX_MCU_DATA,0x006B},
    {CMV59DX_MCU_ADDR,0x271D},
    {CMV59DX_MCU_DATA,0x006B},
    {CMV59DX_MCU_ADDR,0x271F},
    {CMV59DX_MCU_DATA,0x044A},
    {CMV59DX_MCU_ADDR,0x2721},
    {CMV59DX_MCU_DATA,0x034A},
    {CMV59DX_MCU_ADDR,0x2723},
    {CMV59DX_MCU_DATA,0x0000},
    {CMV59DX_MCU_ADDR,0x2725},
    {CMV59DX_MCU_DATA,0x0000},
    {CMV59DX_MCU_ADDR,0x2727},
    {CMV59DX_MCU_DATA,0x01E7},
    {CMV59DX_MCU_ADDR,0x2729},
    {CMV59DX_MCU_DATA,0x0287},
    {CMV59DX_MCU_ADDR,0x272B},
    {CMV59DX_MCU_DATA,0x0001},
    {CMV59DX_MCU_ADDR,0x272D},
#ifdef CONFIG_MACH_NBX03
    {CMV59DX_MCU_DATA,0x0026},
#else
    {CMV59DX_MCU_DATA,0x0025},
#endif
    {CMV59DX_MCU_ADDR,0x272F},
    {CMV59DX_MCU_DATA,0x001A},
    {CMV59DX_MCU_ADDR,0x2731},
    {CMV59DX_MCU_DATA,0x006B},
    {CMV59DX_MCU_ADDR,0x2733},
    {CMV59DX_MCU_DATA,0x006B},
    {CMV59DX_MCU_ADDR,0x2735},
    {CMV59DX_MCU_DATA,0x044A},
    {CMV59DX_MCU_ADDR,0x2737},
    {CMV59DX_MCU_DATA,0x034A},
    {CMV59DX_MCU_ADDR,0x2739},
    {CMV59DX_MCU_DATA,0x0000},
    {CMV59DX_MCU_ADDR,0x273B},
    {CMV59DX_MCU_DATA,0x027F},
    {CMV59DX_MCU_ADDR,0x273D},
    {CMV59DX_MCU_DATA,0x0000},
    {CMV59DX_MCU_ADDR,0x273F},
    {CMV59DX_MCU_DATA,0x01DF},
    {CMV59DX_MCU_ADDR,0x2747},
    {CMV59DX_MCU_DATA,0x0000},
    {CMV59DX_MCU_ADDR,0x2749},
    {CMV59DX_MCU_DATA,0x027F},
    {CMV59DX_MCU_ADDR,0x274B},
    {CMV59DX_MCU_DATA,0x0000},
    {CMV59DX_MCU_ADDR,0x274D},
    {CMV59DX_MCU_DATA,0x01DF},
    {CMV59DX_MCU_ADDR,0x222D},
    {CMV59DX_MCU_DATA,0x0089},
    {CMV59DX_MCU_ADDR,0xA408},
    {CMV59DX_MCU_DATA,0x0021},
    {CMV59DX_MCU_ADDR,0xA409},
    {CMV59DX_MCU_DATA,0x0023},
    {CMV59DX_MCU_ADDR,0xA40A},
    {CMV59DX_MCU_DATA,0x0028},
    {CMV59DX_MCU_ADDR,0xA40B},
    {CMV59DX_MCU_DATA,0x002A},
    {CMV59DX_MCU_ADDR,0x2411},
    {CMV59DX_MCU_DATA,0x0089},
    {CMV59DX_MCU_ADDR,0x2413},
    {CMV59DX_MCU_DATA,0x00A5},
    {CMV59DX_MCU_ADDR,0x2415},
    {CMV59DX_MCU_DATA,0x0089},
    {CMV59DX_MCU_ADDR,0x2417},
    {CMV59DX_MCU_DATA,0x00A5},
    {CMV59DX_MCU_ADDR,0xA404},
    {CMV59DX_MCU_DATA,0x0010},
    {CMV59DX_MCU_ADDR,0xA40D},
    {CMV59DX_MCU_DATA,0x0002},
    {CMV59DX_MCU_ADDR,0xA40E},
    {CMV59DX_MCU_DATA,0x0003},
    {CMV59DX_MCU_ADDR,0xA410},
    {CMV59DX_MCU_DATA,0x000A},
/*
//for 15fps
    {CMV59DX_MCU_ADDR, 0xA20C},
    {CMV59DX_MCU_DATA, 0x0008},
*/
//for 30fps
    {CMV59DX_MCU_ADDR, 0x271F},
    {CMV59DX_MCU_DATA, 0x0225},
    {CMV59DX_MCU_ADDR, 0x2721},
    {CMV59DX_MCU_DATA, 0x034A},
    {CMV59DX_MCU_ADDR, 0x2735},
    {CMV59DX_MCU_DATA, 0x0225},
    {CMV59DX_MCU_ADDR, 0x2737},
    {CMV59DX_MCU_DATA, 0x034A},
    {CMV59DX_MCU_ADDR, 0xA20C},
    {CMV59DX_MCU_DATA, 0x0004},

//3.Lens Correction V13-FOV66-70%
    {0x3658, 0x01F0},
    {0x365A, 0x59CC},
    {0x365C, 0x5232},
    {0x365E, 0x006D},
    {0x3660, 0x8952},
    {0x3680, 0x3F8A},
    {0x3682, 0xD10C},
    {0x3684, 0x5C90},
    {0x3686, 0xD0B0},
    {0x3688, 0xA614},
    {0x36A8, 0x43B2},
    {0x36AA, 0x4291},
    {0x36AC, 0x34B6},
    {0x36AE, 0xF955},
    {0x36B0, 0xF379},
    {0x36D0, 0x23B2},
    {0x36D2, 0x7630},
    {0x36D4, 0xBA14},
    {0x36D6, 0xC814},
    {0x36D8, 0x0FB9},
    {0x36F8, 0x0E15},
    {0x36FA, 0xD055},
    {0x36FC, 0xCFBA},
    {0x36FE, 0x109A},
    {0x3700, 0x7FBD},
    {0x364E, 0x02F0},
    {0x3650, 0x3D8A},
    {0x3652, 0x2512},
    {0x3654, 0x6049},
    {0x3656, 0x3B2F},
    {0x3676, 0x8F06},
    {0x3678, 0xFCEC},
    {0x367A, 0x3710},
    {0x367C, 0xB62D},
    {0x367E, 0xC834},
    {0x369E, 0x1072},
    {0x36A0, 0x18F0},
    {0x36A2, 0x3156},
    {0x36A4, 0xCCF4},
    {0x36A6, 0xE8F9},
    {0x36C6, 0x23F2},
    {0x36C8, 0xE40F},
    {0x36CA, 0xA814},
    {0x36CC, 0x0E52},
    {0x36CE, 0x5078},
    {0x36EE, 0x1A75},
    {0x36F0, 0xB254},
    {0x36F2, 0xC23A},
    {0x36F4, 0x7CD8},
    {0x36F6, 0x5C1D},
    {0x3662, 0x0230},
    {0x3664, 0x8D67},
    {0x3666, 0x1F32},
    {0x3668, 0xACEC},
    {0x366A, 0x03AE},
    {0x368A, 0x9ECC},
    {0x368C, 0x9B2D},
    {0x368E, 0x0631},
    {0x3690, 0x224F},
    {0x3692, 0xBA54},
    {0x36B2, 0x0392},
    {0x36B4, 0x3E50},
    {0x36B6, 0x27B6},
    {0x36B8, 0xFC54},
    {0x36BA, 0xD9F9},
    {0x36DA, 0x4112},
    {0x36DC, 0x29F1},
    {0x36DE, 0x96D4},
    {0x36E0, 0xF934},
    {0x36E2, 0x2B18},
    {0x3702, 0x2615},
    {0x3704, 0xE9D4},
    {0x3706, 0xB4DA},
    {0x3708, 0x3899},
    {0x370A, 0x52BD},
    {0x366C, 0x01B0},
    {0x366E, 0x69EA},
    {0x3670, 0x2432},
    {0x3672, 0x30EE},
    {0x3674, 0x056F},
    {0x3694, 0xB52A},
    {0x3696, 0xC12D},
    {0x3698, 0x4650},
    {0x369A, 0x640F},
    {0x369C, 0xBAB4},
    {0x36BC, 0x0CF2},
    {0x36BE, 0x0351},
    {0x36C0, 0x36F6},
    {0x36C2, 0xA3F5},
    {0x36C4, 0xF2B9},
    {0x36E4, 0x2B12},
    {0x36E6, 0x102F},
    {0x36E8, 0xE0F4},
    {0x36EA, 0x88F5},
    {0x36EC, 0x56B8},
    {0x370C, 0x1EF5},
    {0x370E, 0x9075},
    {0x3710, 0xCC9A},
    {0x3712, 0x4D59},
    {0x3714, 0x6BBD},
    {0x3644, 0x0144},
    {0x3642, 0x00F4},

    {CMV59DX_BITFIELD,0x3210},
    {BITFIELD_BIT,0x0008},
    {BITFIELD_VALUE,1},
//4.Auto Exposure
    {CMV59DX_MCU_ADDR, 0xA202},
    {CMV59DX_MCU_DATA, 0x0000},
    {CMV59DX_MCU_ADDR, 0xA203},
    {CMV59DX_MCU_DATA, 0x00FF},
    {CMV59DX_MCU_ADDR, 0xA208},
    {CMV59DX_MCU_DATA, 0x0000},
    {CMV59DX_MCU_ADDR, 0xA24C},
    {CMV59DX_MCU_DATA, 0x0020},
    {CMV59DX_MCU_ADDR, 0xA24F},
    {CMV59DX_MCU_DATA, 0x0051},
    {CMV59DX_MCU_ADDR, 0xA109},
    {CMV59DX_MCU_DATA, 0x0020},
    {CMV59DX_MCU_ADDR, 0xA10A},
    {CMV59DX_MCU_DATA, 0x0002},
    {CMV59DX_MCU_ADDR, 0xA20D},
    {CMV59DX_MCU_DATA, 0x0030},
    {CMV59DX_MCU_ADDR, 0xA24A},
    {CMV59DX_MCU_DATA, 0x0028},
    {CMV59DX_MCU_ADDR, 0xA24B},
    {CMV59DX_MCU_DATA, 0x0096},
//AE Dynamic Range Tracking
    {CMV59DX_MCU_ADDR,0xA11D},
    {CMV59DX_MCU_DATA,0x0001},
    {CMV59DX_MCU_ADDR,0xA129},
    {CMV59DX_MCU_DATA,0x0001},
//Refresh
    {CMV59DX_MCU_ADDR,0xA103},
    {CMV59DX_MCU_DATA,0x0006},

    {CMV59DX_POLL_FIELD,SEQ_CMD},
    {POLL_FIELD_VALUE,0},
    {POLL_FIELD_DELAY,10},
    {POLL_FIELD_TIMEOUT,50},

    {CMV59DX_MCU_ADDR,0xA103},
    {CMV59DX_MCU_DATA,0x0005},

    {CMV59DX_POLL_FIELD,SEQ_CMD},
    {POLL_FIELD_VALUE,0},
    {POLL_FIELD_DELAY,10},
    {POLL_FIELD_TIMEOUT,50},

//AE Convergence
    {CMV59DX_MCU_ADDR,0xA207},
    {CMV59DX_MCU_DATA,0x0004},

    {CMV59DX_POLL_FIELD,AE_STATUS_READY},
    {POLL_FIELD_VALUE,1},
    {POLL_FIELD_DELAY,100},
    {POLL_FIELD_TIMEOUT,1000},

    {CMV59DX_MCU_ADDR,0xA207},
    {CMV59DX_MCU_DATA,0x000C},

//5.Auto White Balance
    {CMV59DX_MCU_ADDR,0xA35D},
    {CMV59DX_MCU_DATA,0x007B},
    {CMV59DX_MCU_ADDR,0xA35E},
    {CMV59DX_MCU_DATA,0x0085},
    {CMV59DX_MCU_ADDR,0xA35F},
    {CMV59DX_MCU_DATA,0x007E},
    {CMV59DX_MCU_ADDR,0xA360},
    {CMV59DX_MCU_DATA,0x0082},
    {CMV59DX_MCU_ADDR,0x2361},
    {CMV59DX_MCU_DATA,0x0040},
    {CMV59DX_MCU_ADDR,0xA302},
    {CMV59DX_MCU_DATA,0x0000},
    {CMV59DX_MCU_ADDR,0xA303},
    {CMV59DX_MCU_DATA,0x00FF},
    {0x323E,0xC22C},
    {0x3244,0x0335},
    {0x3240,0x6214},

//6.Noise Reduction
    {0x31E0,0x0001},

//7.Gamma Correction
    {0x322A,0x0004},
    {0x35B0,0x049D},
    {CMV59DX_MCU_ADDR,0xAB04},
    {CMV59DX_MCU_DATA,0x0020},
    {CMV59DX_MCU_ADDR,0xAB37},
    {CMV59DX_MCU_DATA,0x0003},
    {CMV59DX_MCU_ADDR,0x2B28},
    {CMV59DX_MCU_DATA,0x1388},
    {CMV59DX_MCU_ADDR,0x2B2A},
    {CMV59DX_MCU_DATA,0x4E20},
    {CMV59DX_MCU_ADDR,0x2B38},
    {CMV59DX_MCU_DATA,0x0100},
    {CMV59DX_MCU_ADDR,0x2B3A},
    {CMV59DX_MCU_DATA,0x20CC},
    {CMV59DX_MCU_ADDR,0xAB22},
    {CMV59DX_MCU_DATA,0x0002},
    {CMV59DX_MCU_ADDR,0xAB26},
    {CMV59DX_MCU_DATA,0x0001},
    {CMV59DX_MCU_ADDR,0xAB1F},
    {CMV59DX_MCU_DATA,0x00C7},
    {CMV59DX_MCU_ADDR,0xAB31},
    {CMV59DX_MCU_DATA,0x001E},
    {CMV59DX_MCU_ADDR,0xAB36},
    {CMV59DX_MCU_DATA,0x0016},
 
//8.Mode :Plain
//8.1 Digital Gain:Normal
    {0x3032, 0x0100},
    {0x3034, 0x0100},
    {0x3036, 0x0100},
    {0x3038, 0x0100},
//8.2 AE target :0
    {CMV59DX_MCU_ADDR,0xA24F},
    {CMV59DX_MCU_DATA,0x0051},
//AE Dynamic Range Tracking
    {CMV59DX_MCU_ADDR,0xA11D},
    {CMV59DX_MCU_DATA,0x0001},
    {CMV59DX_MCU_ADDR,0xA129},
    {CMV59DX_MCU_DATA,0x0001},
//Refresh
    {CMV59DX_MCU_ADDR,0xA103},
    {CMV59DX_MCU_DATA,0x0006},

    {CMV59DX_POLL_FIELD,SEQ_CMD},
    {POLL_FIELD_VALUE,0},
    {POLL_FIELD_DELAY,10},
    {POLL_FIELD_TIMEOUT,50},

    {CMV59DX_MCU_ADDR,0xA103},
    {CMV59DX_MCU_DATA,0x0005},

    {CMV59DX_POLL_FIELD,SEQ_CMD},
    {POLL_FIELD_VALUE,0},
    {POLL_FIELD_DELAY,10},
    {POLL_FIELD_TIMEOUT,50},

//AE Convergence
    {CMV59DX_MCU_ADDR,0xA207},
    {CMV59DX_MCU_DATA,0x0004},

    {CMV59DX_POLL_FIELD,AE_STATUS_READY},
    {POLL_FIELD_VALUE,1},
    {POLL_FIELD_DELAY,100},
    {POLL_FIELD_TIMEOUT,1000},

    {CMV59DX_MCU_ADDR,0xA207},
    {CMV59DX_MCU_DATA,0x000C},
//8.3 Gamma Table :Normal Contrast
    {CMV59DX_MCU_ADDR,0xAB3C},
    {CMV59DX_MCU_DATA,0x0000},
    {CMV59DX_MCU_ADDR,0xAB3D},
    {CMV59DX_MCU_DATA,0x000B},
    {CMV59DX_MCU_ADDR,0xAB3E},
    {CMV59DX_MCU_DATA,0x0020},
    {CMV59DX_MCU_ADDR,0xAB3F},
    {CMV59DX_MCU_DATA,0x003B},
    {CMV59DX_MCU_ADDR,0xAB40},
    {CMV59DX_MCU_DATA,0x005B},
    {CMV59DX_MCU_ADDR,0xAB41},
    {CMV59DX_MCU_DATA,0x0072},
    {CMV59DX_MCU_ADDR,0xAB42},
    {CMV59DX_MCU_DATA,0x0085},
    {CMV59DX_MCU_ADDR,0xAB43},
    {CMV59DX_MCU_DATA,0x0096},
    {CMV59DX_MCU_ADDR,0xAB44},
    {CMV59DX_MCU_DATA,0x00A4},
    {CMV59DX_MCU_ADDR,0xAB45},
    {CMV59DX_MCU_DATA,0x00B1},
    {CMV59DX_MCU_ADDR,0xAB46},
    {CMV59DX_MCU_DATA,0x00BC},
    {CMV59DX_MCU_ADDR,0xAB47},
    {CMV59DX_MCU_DATA,0x00C6},
    {CMV59DX_MCU_ADDR,0xAB48},
    {CMV59DX_MCU_DATA,0x00D0},
    {CMV59DX_MCU_ADDR,0xAB49},
    {CMV59DX_MCU_DATA,0x00D9},
    {CMV59DX_MCU_ADDR,0xAB4A},
    {CMV59DX_MCU_DATA,0x00E2},
    {CMV59DX_MCU_ADDR,0xAB4B},
    {CMV59DX_MCU_DATA,0x00EA},
    {CMV59DX_MCU_ADDR,0xAB4C},
    {CMV59DX_MCU_DATA,0x00F1},
    {CMV59DX_MCU_ADDR,0xAB4D},
    {CMV59DX_MCU_DATA,0x00F8},
    {CMV59DX_MCU_ADDR,0xAB4E},
    {CMV59DX_MCU_DATA,0x00FF},
    {CMV59DX_MCU_ADDR,0xAB4F},
    {CMV59DX_MCU_DATA,0x0000},
    {CMV59DX_MCU_ADDR,0xAB50},
    {CMV59DX_MCU_DATA,0x000B},
    {CMV59DX_MCU_ADDR,0xAB51},
    {CMV59DX_MCU_DATA,0x0023},
    {CMV59DX_MCU_ADDR,0xAB52},
    {CMV59DX_MCU_DATA,0x004D},
    {CMV59DX_MCU_ADDR,0xAB53},
    {CMV59DX_MCU_DATA,0x0071},
    {CMV59DX_MCU_ADDR,0xAB54},
    {CMV59DX_MCU_DATA,0x0088},
    {CMV59DX_MCU_ADDR,0xAB55},
    {CMV59DX_MCU_DATA,0x009A},
    {CMV59DX_MCU_ADDR,0xAB56},
    {CMV59DX_MCU_DATA,0x00A9},
    {CMV59DX_MCU_ADDR,0xAB57},
    {CMV59DX_MCU_DATA,0x00B5},
    {CMV59DX_MCU_ADDR,0xAB58},
    {CMV59DX_MCU_DATA,0x00C0},
    {CMV59DX_MCU_ADDR,0xAB59},
    {CMV59DX_MCU_DATA,0x00C9},
    {CMV59DX_MCU_ADDR,0xAB5A},
    {CMV59DX_MCU_DATA,0x00D2},
    {CMV59DX_MCU_ADDR,0xAB5B},
    {CMV59DX_MCU_DATA,0x00DA},
    {CMV59DX_MCU_ADDR,0xAB5C},
    {CMV59DX_MCU_DATA,0x00E1},
    {CMV59DX_MCU_ADDR,0xAB5D},
    {CMV59DX_MCU_DATA,0x00E8},
    {CMV59DX_MCU_ADDR,0xAB5E},
    {CMV59DX_MCU_DATA,0x00EE},
    {CMV59DX_MCU_ADDR,0xAB5F},
    {CMV59DX_MCU_DATA,0x00F4},
    {CMV59DX_MCU_ADDR,0xAB60},
    {CMV59DX_MCU_DATA,0x00FA},
    {CMV59DX_MCU_ADDR,0xAB61},
    {CMV59DX_MCU_DATA,0x00FF},
//8.4.Manual WB->Auto WB 
//CCM :Original
    {CMV59DX_MCU_ADDR,0x2306},
    {CMV59DX_MCU_DATA,0x038A},
    {CMV59DX_MCU_ADDR,0x2308},
    {CMV59DX_MCU_DATA,0xFDDD},
    {CMV59DX_MCU_ADDR,0x230A},
    {CMV59DX_MCU_DATA,0x003A},
    {CMV59DX_MCU_ADDR,0x230C},
    {CMV59DX_MCU_DATA,0xFF42},
    {CMV59DX_MCU_ADDR,0x230E},
    {CMV59DX_MCU_DATA,0x02D7},
    {CMV59DX_MCU_ADDR,0x2310},
    {CMV59DX_MCU_DATA,0xFF67},
    {CMV59DX_MCU_ADDR,0x2312},
    {CMV59DX_MCU_DATA,0xFF5D},
    {CMV59DX_MCU_ADDR,0x2314},
    {CMV59DX_MCU_DATA,0xFD98},
    {CMV59DX_MCU_ADDR,0x2316},
    {CMV59DX_MCU_DATA,0x0437},
    {CMV59DX_MCU_ADDR,0x2318},
    {CMV59DX_MCU_DATA,0x0015},
    {CMV59DX_MCU_ADDR,0x231A},
    {CMV59DX_MCU_DATA,0x003C},
    {CMV59DX_MCU_ADDR,0x231C},
    {CMV59DX_MCU_DATA,0xFF30},
    {CMV59DX_MCU_ADDR,0x231E},
    {CMV59DX_MCU_DATA,0x0092},
    {CMV59DX_MCU_ADDR,0x2320},
    {CMV59DX_MCU_DATA,0xFFD1},
    {CMV59DX_MCU_ADDR,0x2322},
    {CMV59DX_MCU_DATA,0x0041},
    {CMV59DX_MCU_ADDR,0x2324},
    {CMV59DX_MCU_DATA,0xFFD4},
    {CMV59DX_MCU_ADDR,0x2326},
    {CMV59DX_MCU_DATA,0xFFB8},
    {CMV59DX_MCU_ADDR,0x2328},
    {CMV59DX_MCU_DATA,0x0085},
    {CMV59DX_MCU_ADDR,0x232A},
    {CMV59DX_MCU_DATA,0x0199},
    {CMV59DX_MCU_ADDR,0x232C},
    {CMV59DX_MCU_DATA,0xFDE2},
    {CMV59DX_MCU_ADDR,0x232E},
    {CMV59DX_MCU_DATA,0x0010},
    {CMV59DX_MCU_ADDR,0x2330},
    {CMV59DX_MCU_DATA,0xFFE9},
    {CMV59DX_MCU_ADDR,0xAB20},
    {CMV59DX_MCU_DATA,0x0068},
    {CMV59DX_MCU_ADDR,0xAB24},
    {CMV59DX_MCU_DATA,0x0028},
    {CMV59DX_MCU_ADDR,0xA365},
    {CMV59DX_MCU_DATA,0x0020},
    {CMV59DX_MCU_ADDR,0xA366},
    {CMV59DX_MCU_DATA,0x00F0},
    {CMV59DX_MCU_ADDR,0xA367},
    {CMV59DX_MCU_DATA,0x00A0},
    {CMV59DX_MCU_ADDR,0xA368},
    {CMV59DX_MCU_DATA,0x0060},
    {CMV59DX_MCU_ADDR,0xA363},
    {CMV59DX_MCU_DATA,0x00D5},
    {CMV59DX_MCU_ADDR,0xA364},
    {CMV59DX_MCU_DATA,0x00ED},
    {CMV59DX_MCU_ADDR,0xA34A},
    {CMV59DX_MCU_DATA,0x0076},
    {CMV59DX_MCU_ADDR,0xA34B},
    {CMV59DX_MCU_DATA,0x00D9},
    {CMV59DX_MCU_ADDR,0xA34C},
    {CMV59DX_MCU_DATA,0x0060},
    {CMV59DX_MCU_ADDR,0xA34D},
    {CMV59DX_MCU_DATA,0x00C7},
    {CMV59DX_MCU_ADDR,0xA369},
    {CMV59DX_MCU_DATA,0x0082},
    {CMV59DX_MCU_ADDR,0xA36A},
    {CMV59DX_MCU_DATA,0x0082},
    {CMV59DX_MCU_ADDR,0xA36B},
    {CMV59DX_MCU_DATA,0x0082},
//
    {CMV59DX_MCU_ADDR,0xA115},
    {CMV59DX_MCU_DATA,0x0072},
    {CMV59DX_MCU_ADDR,0xA11F},
    {CMV59DX_MCU_DATA,0x0001},
    {CMV59DX_MCU_ADDR,0xA103},
    {CMV59DX_MCU_DATA,0x0005},

    {CMV59DX_POLL_FIELD,SEQ_CMD},
    {POLL_FIELD_VALUE,0},
    {POLL_FIELD_DELAY,10},
    {POLL_FIELD_TIMEOUT,50},

//8.5 Sharpness :0
    {0x326C,0x1000},
    {CMV59DX_MCU_ADDR,0xAB22},
    {CMV59DX_MCU_DATA,0x0002},

//8.6 Refresh
//Refresh
    {CMV59DX_MCU_ADDR,0xA103},
    {CMV59DX_MCU_DATA,0x0006},

    {CMV59DX_POLL_FIELD,SEQ_CMD},
    {POLL_FIELD_VALUE,0},
    {POLL_FIELD_DELAY,10},
    {POLL_FIELD_TIMEOUT,50},

    {CMV59DX_MCU_ADDR,0xA103},
    {CMV59DX_MCU_DATA,0x0005},

    {CMV59DX_POLL_FIELD,SEQ_CMD},
    {POLL_FIELD_VALUE,0},
    {POLL_FIELD_DELAY,10},
    {POLL_FIELD_TIMEOUT,50},
//9
/*
 *bit 2 of registers 0x3400
 *0 :Asserted to transmit a continuous MIPI output clock
 *1 :Clock is only active while data is being transmitted.
 */ 
    {0x3400, 0x7A20},//change from 0x7A2C to 0x7A20
    {0x001A, 0x0000},
    {SEQUENCE_END, 0x0000}
};

static struct cmv59dx_reg SetModeSequence_640x480[] = 
{
    {CMV59DX_MCU_ADDR, 0x2739},   // MODE_CROP_X0_A
    {CMV59DX_MCU_DATA, 0x0000},   // 0
    {CMV59DX_MCU_ADDR, 0x273B},   // MODE_CROP_X1_A
    {CMV59DX_MCU_DATA, 0x027F},   // 639
    {CMV59DX_MCU_ADDR, 0x273D},   // MODE_CROP_Y0_A
    {CMV59DX_MCU_DATA, 0x0000},   // 0
    {CMV59DX_MCU_ADDR, 0x273F},   // MODE_CROP_Y1_A
    {CMV59DX_MCU_DATA, 0x01DF},   // 479
    {CMV59DX_MCU_ADDR, 0x2703},   // MODE_OUTPUT_WIDTH_A
    {CMV59DX_MCU_DATA, 0x0280},   // 640
    {CMV59DX_MCU_ADDR, 0x2705},   // MODE_OUTPUT_HEIGHT_A
    {CMV59DX_MCU_DATA, 0x01E0},   // 480

    {CMV59DX_MCU_ADDR,0xA103},
    {CMV59DX_MCU_DATA,0x0006},

    {CMV59DX_POLL_FIELD,SEQ_CMD},
    {POLL_FIELD_VALUE,0},
    {POLL_FIELD_DELAY,10},
    {POLL_FIELD_TIMEOUT,50},

    {CMV59DX_MCU_ADDR,0xA103},
    {CMV59DX_MCU_DATA,0x0005},

    {CMV59DX_POLL_FIELD,SEQ_CMD},
    {POLL_FIELD_VALUE,0},
    {POLL_FIELD_DELAY,10},
    {POLL_FIELD_TIMEOUT,50},

    {SEQUENCE_END,   0x0000}
};

static struct cmv59dx_reg SetModeSequence_352x288[] = 
{
    {CMV59DX_MCU_ADDR, 0x2739},   // MODE_CROP_X0_A
    {CMV59DX_MCU_DATA, 0x001C},   // 26
    {CMV59DX_MCU_ADDR, 0x273B},   // MODE_CROP_X1_A
    {CMV59DX_MCU_DATA, 0x0262},   // 613 (= 26 + 588 - 1)
    {CMV59DX_MCU_ADDR, 0x273D},   // MODE_CROP_Y0_A
    {CMV59DX_MCU_DATA, 0x0001},   // 0
    {CMV59DX_MCU_ADDR, 0x273F},   // MODE_CROP_Y1_A
    {CMV59DX_MCU_DATA, 0x01DD},   // 479
    {CMV59DX_MCU_ADDR, 0x2703},   // MODE_OUTPUT_WIDTH_A
    {CMV59DX_MCU_DATA, 0x0160},   // 352
    {CMV59DX_MCU_ADDR, 0x2705},   // MODE_OUTPUT_HEIGHT_A
    {CMV59DX_MCU_DATA, 0x0120},   // 288

    {CMV59DX_MCU_ADDR,0xA103},
    {CMV59DX_MCU_DATA,0x0006},

    {CMV59DX_POLL_FIELD,SEQ_CMD},
    {POLL_FIELD_VALUE,0},
    {POLL_FIELD_DELAY,10},
    {POLL_FIELD_TIMEOUT,50},

    {CMV59DX_MCU_ADDR,0xA103},
    {CMV59DX_MCU_DATA,0x0005},

    {CMV59DX_POLL_FIELD,SEQ_CMD},
    {POLL_FIELD_VALUE,0},
    {POLL_FIELD_DELAY,10},
    {POLL_FIELD_TIMEOUT,50},

    {SEQUENCE_END,   0x0000}
};

static struct cmv59dx_reg SetModeSequence_176x144[] = 
{
    {CMV59DX_MCU_ADDR, 0x2739},   // MODE_CROP_X0_A
    {CMV59DX_MCU_DATA, 0x001A},   // 26
    {CMV59DX_MCU_ADDR, 0x273B},   // MODE_CROP_X1_A
    {CMV59DX_MCU_DATA, 0x0266},   // 613 (= 26 + 588 - 1)
    {CMV59DX_MCU_ADDR, 0x273D},   // MODE_CROP_Y0_A
    {CMV59DX_MCU_DATA, 0x0000},   // 0
    {CMV59DX_MCU_ADDR, 0x273F},   // MODE_CROP_Y1_A
    {CMV59DX_MCU_DATA, 0x01DF},   // 479
    {CMV59DX_MCU_ADDR, 0x2703},   // MODE_OUTPUT_WIDTH_A
    {CMV59DX_MCU_DATA, 0x00B0},   // 176
    {CMV59DX_MCU_ADDR, 0x2705},   // MODE_OUTPUT_HEIGHT_A
    {CMV59DX_MCU_DATA, 0x0090},   // 144

    {CMV59DX_MCU_ADDR,0xA103},
    {CMV59DX_MCU_DATA,0x0006},

    {CMV59DX_POLL_FIELD,SEQ_CMD},
    {POLL_FIELD_VALUE,0},
    {POLL_FIELD_DELAY,10},
    {POLL_FIELD_TIMEOUT,50},

    {CMV59DX_MCU_ADDR,0xA103},
    {CMV59DX_MCU_DATA,0x0005},

    {CMV59DX_POLL_FIELD,SEQ_CMD},
    {POLL_FIELD_VALUE,0},
    {POLL_FIELD_DELAY,10},
    {POLL_FIELD_TIMEOUT,50},

    {SEQUENCE_END,   0x0000}
};

static struct cmv59dx_reg SetModeSequence_320x240[] = 
{
    {CMV59DX_MCU_ADDR, 0x2739},   // MODE_CROP_X0_A
    {CMV59DX_MCU_DATA, 0x0000},   // 26
    {CMV59DX_MCU_ADDR, 0x273B},   // MODE_CROP_X1_A
    {CMV59DX_MCU_DATA, 0x027F},   // 613 (= 26 + 588 - 1)
    {CMV59DX_MCU_ADDR, 0x273D},   // MODE_CROP_Y0_A
    {CMV59DX_MCU_DATA, 0x0000},   // 0
    {CMV59DX_MCU_ADDR, 0x273F},   // MODE_CROP_Y1_A
    {CMV59DX_MCU_DATA, 0x01DF},   // 479
    {CMV59DX_MCU_ADDR, 0x2703},   // MODE_OUTPUT_WIDTH_A
    {CMV59DX_MCU_DATA, 0x0140},   // 176
    {CMV59DX_MCU_ADDR, 0x2705},   // MODE_OUTPUT_HEIGHT_A
    {CMV59DX_MCU_DATA, 0x00F0},   // 144

    {CMV59DX_MCU_ADDR,0xA103},
    {CMV59DX_MCU_DATA,0x0006},

    {CMV59DX_POLL_FIELD,SEQ_CMD},
    {POLL_FIELD_VALUE,0},
    {POLL_FIELD_DELAY,10},
    {POLL_FIELD_TIMEOUT,50},

    {CMV59DX_MCU_ADDR,0xA103},
    {CMV59DX_MCU_DATA,0x0005},

    {CMV59DX_POLL_FIELD,SEQ_CMD},
    {POLL_FIELD_VALUE,0},
    {POLL_FIELD_DELAY,10},
    {POLL_FIELD_TIMEOUT,50},

    {SEQUENCE_END,   0x0000}
};

static struct cmv59dx_reg SetModeSequence_160x120[] = 
{
    {CMV59DX_MCU_ADDR, 0x2739},   // MODE_CROP_X0_A
    {CMV59DX_MCU_DATA, 0x0000},   // 26
    {CMV59DX_MCU_ADDR, 0x273B},   // MODE_CROP_X1_A
    {CMV59DX_MCU_DATA, 0x027F},   // 613 (= 26 + 588 - 1)
    {CMV59DX_MCU_ADDR, 0x273D},   // MODE_CROP_Y0_A
    {CMV59DX_MCU_DATA, 0x0000},   // 0
    {CMV59DX_MCU_ADDR, 0x273F},   // MODE_CROP_Y1_A
    {CMV59DX_MCU_DATA, 0x01DF},   // 479
    {CMV59DX_MCU_ADDR, 0x2703},   // MODE_OUTPUT_WIDTH_A
    {CMV59DX_MCU_DATA, 0x00A0},   // 176
    {CMV59DX_MCU_ADDR, 0x2705},   // MODE_OUTPUT_HEIGHT_A
    {CMV59DX_MCU_DATA, 0x0078},   // 144

    {CMV59DX_MCU_ADDR,0xA103},
    {CMV59DX_MCU_DATA,0x0006},

    {CMV59DX_POLL_FIELD,SEQ_CMD},
    {POLL_FIELD_VALUE,0},
    {POLL_FIELD_DELAY,10},
    {POLL_FIELD_TIMEOUT,50},

    {CMV59DX_MCU_ADDR,0xA103},
    {CMV59DX_MCU_DATA,0x0005},

    {CMV59DX_POLL_FIELD,SEQ_CMD},
    {POLL_FIELD_VALUE,0},
    {POLL_FIELD_DELAY,10},
    {POLL_FIELD_TIMEOUT,50},

    {SEQUENCE_END,   0x0000}
};
/*
static struct cmv59dx_reg SetModeSequence_fps_auto[] = {
    {CMV59DX_MCU_ADDR, 0x271F},
    {CMV59DX_MCU_DATA, 0x0225},
    {CMV59DX_MCU_ADDR, 0x2721},
    {CMV59DX_MCU_DATA, 0x034A},
    {CMV59DX_MCU_ADDR, 0x2735},
    {CMV59DX_MCU_DATA, 0x0225},
    {CMV59DX_MCU_ADDR, 0x2737},
    {CMV59DX_MCU_DATA, 0x034A},
    {CMV59DX_MCU_ADDR, 0xA20C},
    {CMV59DX_MCU_DATA, 0x000C},
    {SEQUENCE_END,   0x0000}
};
*/
static struct cmv59dx_reg SetModeSequence_fps_30[] = {
    {CMV59DX_MCU_ADDR, 0x271F},
    {CMV59DX_MCU_DATA, 0x0225},
    {CMV59DX_MCU_ADDR, 0x2721},
    {CMV59DX_MCU_DATA, 0x034A},
    {CMV59DX_MCU_ADDR, 0x2735},
    {CMV59DX_MCU_DATA, 0x0225},
    {CMV59DX_MCU_ADDR, 0x2737},
    {CMV59DX_MCU_DATA, 0x034A},
    {CMV59DX_MCU_ADDR, 0xA20C},
    {CMV59DX_MCU_DATA, 0x0004},

    {CMV59DX_POLL_FIELD,SEQ_CMD},
    {POLL_FIELD_VALUE,0},
    {POLL_FIELD_DELAY,10},
    {POLL_FIELD_TIMEOUT,50},

    {CMV59DX_MCU_ADDR,0xA103},
    {CMV59DX_MCU_DATA,0x0005},

    {CMV59DX_POLL_FIELD,SEQ_CMD},
    {POLL_FIELD_VALUE,0},
    {POLL_FIELD_DELAY,10},
    {POLL_FIELD_TIMEOUT,50},

    {SEQUENCE_END,   0x0000}
};
/*
static struct cmv59dx_reg SetModeSequence_fps_25[] = {
    {CMV59DX_MCU_ADDR, 0x271F},
    {CMV59DX_MCU_DATA, 0x0293},
    {CMV59DX_MCU_ADDR, 0x2721},
    {CMV59DX_MCU_DATA, 0x034A},
    {CMV59DX_MCU_ADDR, 0x2735},
    {CMV59DX_MCU_DATA, 0x0293},
    {CMV59DX_MCU_ADDR, 0x2737},
    {CMV59DX_MCU_DATA, 0x034A},
    {CMV59DX_MCU_ADDR, 0xA20C},
    {CMV59DX_MCU_DATA, 0x0004},
    {SEQUENCE_END,   0x0000}
};

static struct cmv59dx_reg SetModeSequence_fps_20[] = {
    {CMV59DX_MCU_ADDR, 0x271F},
    {CMV59DX_MCU_DATA, 0x0337},
    {CMV59DX_MCU_ADDR, 0x2721},
    {CMV59DX_MCU_DATA, 0x034A},
    {CMV59DX_MCU_ADDR, 0x2735},
    {CMV59DX_MCU_DATA, 0x0337},
    {CMV59DX_MCU_ADDR, 0x2737},
    {CMV59DX_MCU_DATA, 0x034A},
    {CMV59DX_MCU_ADDR, 0xA20C},
    {CMV59DX_MCU_DATA, 0x0006},
    {SEQUENCE_END,   0x0000}
};
*/
static struct cmv59dx_reg SetModeSequence_fps_15[] = {
    {CMV59DX_MCU_ADDR, 0x271F},
    {CMV59DX_MCU_DATA, 0x044A},
    {CMV59DX_MCU_ADDR, 0x2721},
    {CMV59DX_MCU_DATA, 0x034A},
    {CMV59DX_MCU_ADDR, 0x2735},
    {CMV59DX_MCU_DATA, 0x044A},
    {CMV59DX_MCU_ADDR, 0x2737},
    {CMV59DX_MCU_DATA, 0x034A},
    {CMV59DX_MCU_ADDR, 0xA20C},
    {CMV59DX_MCU_DATA, 0x0008},

    {CMV59DX_POLL_FIELD,SEQ_CMD},
    {POLL_FIELD_VALUE,0},
    {POLL_FIELD_DELAY,10},
    {POLL_FIELD_TIMEOUT,50},

    {CMV59DX_MCU_ADDR,0xA103},
    {CMV59DX_MCU_DATA,0x0005},

    {CMV59DX_POLL_FIELD,SEQ_CMD},
    {POLL_FIELD_VALUE,0},
    {POLL_FIELD_DELAY,10},
    {POLL_FIELD_TIMEOUT,50},

    {SEQUENCE_END,   0x0000}
};
/*
static struct cmv59dx_reg SetModeSequence_fps_10[] = {
    {CMV59DX_MCU_ADDR, 0x271F},
    {CMV59DX_MCU_DATA, 0x066F},
    {CMV59DX_MCU_ADDR, 0x2721},
    {CMV59DX_MCU_DATA, 0x034A},
    {CMV59DX_MCU_ADDR, 0x2735},
    {CMV59DX_MCU_DATA, 0x066F},
    {CMV59DX_MCU_ADDR, 0x2737},
    {CMV59DX_MCU_DATA, 0x034A},
    {CMV59DX_MCU_ADDR, 0xA20C},
    {CMV59DX_MCU_DATA, 0x000C},
    {SEQUENCE_END,   0x0000}
};

static struct cmv59dx_reg SetModeSequence_fps_5[] = {
    {CMV59DX_MCU_ADDR, 0x271F},
    {CMV59DX_MCU_DATA, 0x0CDF},
    {CMV59DX_MCU_ADDR, 0x2721},
    {CMV59DX_MCU_DATA, 0x034A},
    {CMV59DX_MCU_ADDR, 0x2735},
    {CMV59DX_MCU_DATA, 0x0CDF},
    {CMV59DX_MCU_ADDR, 0x2737},
    {CMV59DX_MCU_DATA, 0x034A},
    {CMV59DX_MCU_ADDR, 0xA20C},
    {CMV59DX_MCU_DATA, 0x0018},
    {SEQUENCE_END,   0x0000}
};

static struct cmv59dx_reg SetModeSequence_fps_3[] = {
    {CMV59DX_MCU_ADDR, 0x271F},
    {CMV59DX_MCU_DATA, 0x1422},
    {CMV59DX_MCU_ADDR, 0x2735},
    {CMV59DX_MCU_DATA, 0x1422},
    {CMV59DX_MCU_ADDR, 0xA20C},
    {CMV59DX_MCU_DATA, 0x0020},
    {SEQUENCE_END,   0x0000}
};

static struct cmv59dx_reg SetModeSequence_fps_2[] = {
    {CMV59DX_MCU_ADDR, 0x271F},
    {CMV59DX_MCU_DATA, 0x1E34},
    {CMV59DX_MCU_ADDR, 0x2735},
    {CMV59DX_MCU_DATA, 0x1E34},
    {CMV59DX_MCU_ADDR, 0xA20C},
    {CMV59DX_MCU_DATA, 0x0032},
    {SEQUENCE_END,   0x0000}
};

static struct cmv59dx_reg SetModeSequence_fps_1[] = {
    {CMV59DX_MCU_ADDR, 0x271F},
    {CMV59DX_MCU_DATA, 0x3C68},
    {CMV59DX_MCU_ADDR, 0x2735},
    {CMV59DX_MCU_DATA, 0x3C68},
    {CMV59DX_MCU_ADDR, 0xA20C},
    {CMV59DX_MCU_DATA, 0x0064},
    {SEQUENCE_END,   0x0000}
};
*/
enum {
    CMV59DX_MODE_640x480,
    CMV59DX_MODE_352x288,
    CMV59DX_MODE_176x144,
    CMV59DX_MODE_320x240,
    CMV59DX_MODE_160x120,
};

static struct cmv59dx_reg *mode_table[] = {
    [CMV59DX_MODE_640x480] = SetModeSequence_640x480,
    [CMV59DX_MODE_352x288] = SetModeSequence_352x288,
    [CMV59DX_MODE_176x144] = SetModeSequence_176x144,
    [CMV59DX_MODE_320x240] = SetModeSequence_320x240,
    [CMV59DX_MODE_160x120] = SetModeSequence_160x120,
};

static int cmv59dx_read_reg(struct i2c_client *client, u16 addr, u16 *val)
{
    int err;
    struct i2c_msg msg[2];
    unsigned char data[4];

    if (!client->adapter)
        return -ENODEV;

    msg[0].addr = client->addr;
    msg[0].flags = 0;
    msg[0].len = 2;
    msg[0].buf = data;

    /* high byte goes out first */
    data[0] = (u8) (addr >> 8);;
    data[1] = (u8) (addr & 0xff);

    msg[1].addr = client->addr;
    msg[1].flags = I2C_M_RD;
    msg[1].len = 2;
    msg[1].buf = data + 2;

    err = i2c_transfer(client->adapter, msg, 2);

    if (err != 2)
        return -EINVAL;

    *val=data[2]<<8 | data[3];
    return 0;
}

static int cmv59dx_write_reg(struct i2c_client *client, u16 addr, u16 val)
{
    int err;
    struct i2c_msg msg;
    unsigned char data[4];
    int retry = 0;

    if (!client->adapter)
        return -ENODEV;

    data[0] = (u8) (addr >> 8);;
    data[1] = (u8) (addr & 0xff);
    data[2] = (u8) (val >> 8);
    data[3] = (u8) (val & 0xff);

    msg.addr = client->addr;
    msg.flags = 0;
    msg.len = 4;
    msg.buf = data;

    do {
        err = i2c_transfer(client->adapter, &msg, 1);
        if (err == 1)
            return 0;
        retry++;
        pr_err("cmv59dx: i2c transfer failed, retrying %x %x\n",
                addr, val);
        msleep(3);
    } while (retry <= CMV59DX_MAX_RETRIES);

    return err;
}

static int cmv59dx_write_table(struct i2c_client *client,
                const struct cmv59dx_reg table[],
                const struct cmv59dx_reg override_list[],
                int num_override_regs)
{   
    int err;
    const struct cmv59dx_reg *next;
    int i;
    u16 val = 0;

    for (next = table; next->addr != SEQUENCE_END; next++) {
        if (next->addr == SEQUENCE_WAIT_MS) {
            msleep(next->val);
            continue;
        }

        if (next->addr == CMV59DX_POLL_REG){
            u16 poll_addr;
            u16 poll_bit;
            u16 poll_value;
            u16 poll_delay;
            u16 poll_timeout;
            poll_addr = next->val;
            next++;
            poll_bit = next->val;
            next++;
            poll_value = next->val;
            next++;
            poll_delay = next->val;
            next++;
            poll_timeout =next->val;
            for(i=0;i<poll_timeout;i++){
                err = cmv59dx_read_reg(client, poll_addr, &val);
                if(err){
                    printk("%s error, addr=%#x\n",__func__,poll_addr);
                    return err;
                }
                printk("POLL_REG addr=%#x,val=%#x\n",poll_addr,val&poll_bit);
                if((val & poll_bit) == (poll_bit * poll_value)){
                    msleep(poll_delay);
                }else{
                    break;
                }
            }
            if(i==poll_timeout){
                printk("POLL_REG timeout addr=%#x\n",poll_addr);
            }
            continue;
        }

        if(next->addr == CMV59DX_BITFIELD){
            u16 bitfield_addr;
            u16 bitfield_bit;
            u16 bitfield_value;
            bitfield_addr = next->val;
            next++;
            bitfield_bit = next->val;
            next++;
            bitfield_value = next->val;
            err = cmv59dx_read_reg(client, bitfield_addr, &val);
            if(err){
                printk("%s error, addr=%#x\n",__func__,bitfield_addr);
                return err;
            }
            if(bitfield_value==1){
                val = val | bitfield_bit;
            }else{
                val = val & (~bitfield_bit);
            }
            err = cmv59dx_write_reg(client, bitfield_addr, val);
            if(err){
                printk("%s error, addr=%#x\n",__func__,bitfield_addr);
                return err;
            }
            continue;
        }

        if(next->addr == CMV59DX_POLL_FIELD){
            u16 poll_field_cmd;
            u16 poll_field_value;
            u16 poll_field_delay;
            u16 poll_field_timeout;
            poll_field_cmd = next->val;
            next++;
            poll_field_value = next->val;
            next++;
            poll_field_delay = next->val;
            next++;
            poll_field_timeout =next->val;
            if(poll_field_cmd == SEQ_CMD){
                for(i=0;i<poll_field_timeout;i++){
                    err = cmv59dx_write_reg(client, CMV59DX_MCU_ADDR, 0xA103);
                    if(err){
                        printk("%s error\n",__func__);
                        return err;
                    }
                    err = cmv59dx_read_reg(client, CMV59DX_MCU_DATA, &val);
                    if(err){
                        printk("%s error\n",__func__);
                        return err;
                    }
                    printk("POLL_FIELD SEQ_CMD val=%#x\n",val);
                    if(val != poll_field_value){
                        msleep(poll_field_delay);
                    }else{
                        break;
                    }
                }
                if(i==poll_field_timeout){
                    printk("POLL_FIELD SEQ_CMD timeout\n");
                }

                continue;
            }

            if(poll_field_cmd == AE_STATUS_READY){
                for(i=0;i<poll_field_timeout;i++){
                    err = cmv59dx_write_reg(client, CMV59DX_MCU_ADDR, 0xA217);
                    if(err){
                        printk("%s error\n",__func__);
                        return err;
                    }
                    err = cmv59dx_read_reg(client, CMV59DX_MCU_DATA, &val);
                    if(err){
                        printk("%s error\n",__func__);
                        return err;
                    }
                    printk("POLL_FIELD AE_STATUS_READY val=%#x\n",val&0x04);
                    if((val & 0x04) != poll_field_value*0x04){
                        msleep(poll_field_delay);
                    }else{
                        break;
                    }
                }
                if(i==poll_field_timeout){
                    printk("POLL_FIELD AE_STATUS_READY timeout\n");
                }

                continue;
            }
        }

        val = next->val;

        /* When an override list is passed in, replace the reg */
        /* value to write if the reg is in the list            */
        if (override_list) {
            for (i = 0; i < num_override_regs; i++) {
                if (next->addr == override_list[i].addr) {
                    val = override_list[i].val;
                    break;
                }
            }
        }

        err = cmv59dx_write_reg(client, next->addr, val);
        if (err)
            return err;
    }
    return 0;
}

static int cmv59dx_set_mode(struct cmv59dx_info *info, struct cmv59dx_mode *mode)
{
    int sensor_mode;
    int err;

    pr_info("%s: xres %u yres %u framelength %u coarsetime %u gain %u\n",
        __func__, mode->xres, mode->yres, mode->frame_length,
        mode->coarse_time, mode->gain);
    if (mode->xres == 640 && mode->yres == 480)
        sensor_mode = CMV59DX_MODE_640x480;
    else if (mode->xres == 352 && mode->yres == 288)
        sensor_mode = CMV59DX_MODE_352x288;
    else if (mode->xres == 176 && mode->yres == 144)
        sensor_mode = CMV59DX_MODE_176x144;
    else if (mode->xres == 320 && mode->yres == 240)
        sensor_mode = CMV59DX_MODE_320x240;
    else if (mode->xres == 160 && mode->yres == 120)
        sensor_mode = CMV59DX_MODE_160x120;
    else {
        pr_err("%s: invalid resolution supplied to set mode %d %d\n",
                __func__, mode->xres, mode->yres);
        return -EINVAL;
    }

    err = cmv59dx_write_table(info->i2c_client, mode_table[sensor_mode],  NULL, 0);
    return 0;
}

static int cmv59dx_init_dev(struct cmv59dx_info *info, u32 arg)
{
    int err = 0;
    err = cmv59dx_write_table(info->i2c_client, InitSequence,  NULL, 0);
    printk("%s\n",__func__);
    
    return err;
}

static int cmv59dx_set_fps(struct cmv59dx_info *info, u8 fps)
{
    int status = 0;
    printk("%s,   fps =%d\n",__func__,fps);
    switch(fps){
        case 30:
            status = cmv59dx_write_table(info->i2c_client, SetModeSequence_fps_30, NULL, 0);
            if (status){
                printk("%s -> %d error\n",__func__,fps);
                return status;
            }
            break;
        case 15:
            status = cmv59dx_write_table(info->i2c_client, SetModeSequence_fps_15, NULL, 0);
            if (status){
                printk("%s -> %d error\n",__func__,fps);
                return status;
            }
            break;
        default:
            break;
    }

    return 0;
}
static long cmv59dx_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct cmv59dx_info *info = file->private_data;

    printk("%s\n", __func__);

    switch (cmd) {
    case CMV59DX_IOCTL_SET_MODE:
    {
        struct cmv59dx_mode mode;
        if (copy_from_user(&mode,  (const void __user *)arg,
                    sizeof(struct cmv59dx_mode))) {
            pr_info("%s %d\n", __func__, __LINE__);
            return -EFAULT;
        }

        return cmv59dx_set_mode(info, &mode);
    }
    case CMV59DX_IOCTL_INIT_DEV:
        return cmv59dx_init_dev(info, (u32)arg);
    case CMV59DX_IOCTL_SET_FPS:
        return cmv59dx_set_fps(info, (u8)arg);
    default:
        return -EINVAL;
    }
    return 0;
}

static int nbx03_cmv59dx_power_on(void)
{

    gpio_set_value(GPIO_PIN_VGA_RST, 1);
    gpio_set_value(GPIO_PIN_VGA_EN, 1);

    gpio_set_value(GPIO_PIN_5M_12V, 1);
    mdelay(1);
    gpio_set_value(GPIO_PIN_5M_18V, 1);
    mdelay(1);
    gpio_set_value(GPIO_PIN_5M_28V, 1);
    gpio_set_value(GPIO_PIN_VGA_RST, 0);
    mdelay(1);

    gpio_set_value(GPIO_PIN_5M_28V_VCM, 1);

    msleep(100);
    tegra_camera_enable_mclk();

    mdelay(1);
    gpio_set_value(GPIO_PIN_VGA_RST, 1);

    return 0;
}

static int nbx03_cmv59dx_power_off(void)
{

    gpio_set_value(GPIO_PIN_VGA_RST, 0);
    tegra_camera_disable_mclk();

    gpio_set_value(GPIO_PIN_5M_28V_VCM, 0);

    gpio_set_value(GPIO_PIN_5M_28V, 0);
    gpio_set_value(GPIO_PIN_5M_18V, 0);
    gpio_set_value(GPIO_PIN_5M_12V, 0);

    gpio_set_value(GPIO_PIN_VGA_EN, 0);

    return 0;
}

static struct cmv59dx_info *info;

static int cmv59dx_open(struct inode *inode, struct file *file)
{
    pr_info("%s\n", __func__);
    file->private_data = info;
#ifdef CONFIG_MACH_NBX03
    nbx03_cmv59dx_power_on();
#endif
    return 0;
}

int cmv59dx_release(struct inode *inode, struct file *file)
{
    pr_info("%s\n", __func__);
#ifdef CONFIG_MACH_NBX03
    nbx03_cmv59dx_power_off();
#endif
    file->private_data = NULL;
    return 0;
}


static const struct file_operations cmv59dx_fileops = {
    .owner = THIS_MODULE,
    .open = cmv59dx_open,
    .unlocked_ioctl = cmv59dx_ioctl,
    .release = cmv59dx_release,
};

static struct miscdevice cmv59dx_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "cmv59dx",
    .fops = &cmv59dx_fileops,
};

static int cmv59dx_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int err;

    pr_info("cmv59dx: probing sensor.\n");

    info = kzalloc(sizeof(struct cmv59dx_info), GFP_KERNEL);
    if (!info) {
        pr_err("cmv59dx: Unable to allocate memory!\n");
        return -ENOMEM;
    }

    err = misc_register(&cmv59dx_device);
    if (err) {
        pr_err("cmv59dx: Unable to register misc device!\n");
        kfree(info);
        return err;
    }

    info->pdata = client->dev.platform_data;
    info->i2c_client = client;

    i2c_set_clientdata(client, info);
    return 0;
}

static int cmv59dx_remove(struct i2c_client *client)
{
    struct cmv59dx_info *info;
    info = i2c_get_clientdata(client);
    misc_deregister(&cmv59dx_device);
    kfree(info);
    return 0;
}

static const struct i2c_device_id cmv59dx_id[] = {
    { "cmv59dx", 0 },
    { },
};

MODULE_DEVICE_TABLE(i2c, cmv59dx_id);

static struct i2c_driver cmv59dx_i2c_driver = {
    .driver = {
        .name = "cmv59dx",
        .owner = THIS_MODULE,
    },
    .probe = cmv59dx_probe,
    .remove = cmv59dx_remove,
    .id_table = cmv59dx_id,
};

static int __init cmv59dx_init(void)
{
    pr_info("cmv59dx sensor driver loading\n");
    return i2c_add_driver(&cmv59dx_i2c_driver);
}

static void __exit cmv59dx_exit(void)
{
    i2c_del_driver(&cmv59dx_i2c_driver);
}

module_init(cmv59dx_init);
module_exit(cmv59dx_exit);

