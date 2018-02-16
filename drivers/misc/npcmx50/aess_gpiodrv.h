/*
 * $RCSfile$
 * $Revision$
 * $Date$
 * $Author$
 *
 * VSC 452 On chip GPIO driver.
 *  
 * Copyright (C) 2006 Avocent Corp.
 *
 * This file is subject to the terms and conditions of the GNU 
 * General Public License. This program is distributed in the hope 
 * that it will be useful, but WITHOUT ANY WARRANTY; without even 
 * the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
 * PURPOSE.  See the GNU General Public License for more details.
 */
#include <asm/ioctl.h>

#if !defined (AESSGPIODRV_H)
#define AESSGPIODRV_H


/* Global include area */
/* GPIO-config command number */
#define INPUT_INT_OFF            0x0
#define INPUT_INT_RISING         0x1
#define INPUT_INT_FAILING        0x2
#define INPUT_INT_HIGH           0x3
#define INPUT_INT_LOW            0x4
#define INPUT_INT_BOTH_EDGE      0x5
#define INPUT_INT_BOTH_LEVEL      0x6

/* GPIO command type */
#define GPIO_CONFIG    0 
#define GPIO_WRITE     1
#define GPIO_READ      2


/* GPIO-input command number */
#define NORMAL_INPUT                    0x0
#define INT_STATUS_INPUT                0x1
#define INT_ENABLE_INPUT                0x2
#define BLINK_INPUT                		0x3
#define OUTPUT_INPUT                	0x4
#define DIRECT_INPUT                	0x5
#define OPEN_DRAIN_INPUT                0x6

/* GPIO-output command number */
#define NORMAL_OUTPUT            0x0
#define OPEN_DRAIN_OUTPUT        0x1
#define POWER_OUTPUT             0x2
#define BLINK_OUTPUT             0x3
#define DEBOUNCE_OUTPUT          0x4
#define CLR_INT_OUTPUT           0x5
#define EVENT_TYPE_OUTPUT        0x6
#define EVENT_POLARITY_OUTPUT    0x7
#define EVENT_ENABLE_OUTPUT      0x8
#define GPIO_SELECTED_OUTPUT     0x9

/* Command value and mask */
#define SET_GPIO_OUTPUT_LOW          0x00000000
#define SET_GPIO_OUTPUT_HIGH         0x00000001



#ifdef AESSGPIODRV_C

#define MAX_CONFIG_COMMAND      7
#define MAX_WRITE_COMMAND       10  


/* ioctl definitions */
#define AESS_GPIODRV_IOC_MAGIC    0xB5
#define AESS_GPIODRV_R           _IOWR(AESS_GPIODRV_IOC_MAGIC, 0, int)
#define AESS_GPIODRV_W           _IOWR(AESS_GPIODRV_IOC_MAGIC, 1, int)
#define AESS_GPIODRV_CFG         _IOWR(AESS_GPIODRV_IOC_MAGIC, 2, int)



/* For temporary use, because no type.h now */
#define STATUS_OK     0
#define STATUS_FAIL   1

/* init flag */
#define AESSGPIO_NOT_INIT 0
#define AESSGPIO_INIT_OK  1


/* Command value and mask */

#define GPIO_DATA_MASK           0x00000001
#define GPIO_DIRECTION_MASK      0x00000002

#define GPIO_PORT_MASK           0x000000E0
#define GPIO_PIN_MASK            0x0000001F
#define GPIO_PORT_SHIFT          0x5

#define GPIO_BLINK_DATA_MASK           0x7

  

#ifdef CONFIG_MACH_NPCM750
#define MAX_MULTI_SEL                       3
#define MAX_GPIO_PIN                        32
#define MAX_GPIO_PORT                       8
#define GPIO_PIN_MAX                        256
#define Z2_VERSION_NUM                      0x04A92750        // NPCM750 Z2
#define GPIO_PORT_BASE_ADDR(port)           NPCMX50_GPIO_BASE_ADDR(port)
#else
#ifdef CONFIG_MACH_NPCM650
#define MAX_MULTI_SEL                       1
#define MAX_GPIO_PIN                        18
#define MAX_GPIO_PORT                       13
#define GPIO_PIN_MAX                        208
#define Z2_VERSION_NUM                      0x04926650        // NPCM650 Z2 not exist (only Z1)
#define GPIO_PORT_BASE_ADDR_MODULE0         NPCMX50_GPIO_BASE_ADDR(0)
#define GPIO_PORT_BASE_ADDR_MODULE1         NPCMX50_GPIO_BASE_ADDR(1)
#endif
#endif

#define NPCMX50_GLOBAL_CTRL_REG             NPCMX50_GCR_BASE_ADDR
#define GLOBAL_REG_PDID_REG          		(NPCMX50_GLOBAL_CTRL_REG+0x0)
#define GLOBAL_REG_PIN_SELECT1_ADDR  		(NPCMX50_GLOBAL_CTRL_REG + 0xc)
#define GLOBAL_REG_PIN_SELECT2_ADDR  		(NPCMX50_GLOBAL_CTRL_REG + 0x10)
#ifdef CONFIG_MACH_NPCM750
#define GLOBAL_REG_PIN_SELECT3_ADDR  		(NPCMX50_GLOBAL_CTRL_REG + 0x64)
#define GLOBAL_REG_PIN_FLOCKR1_ADDR  		(NPCMX50_GLOBAL_CTRL_REG + 0x74)
#define GLOBAL_REG_PIN_SELECT4_ADDR  		(NPCMX50_GLOBAL_CTRL_REG + 0xB0)
#define GLOBAL_REG_PIN_I2CSEGSEL_ADDR  		(NPCMX50_GLOBAL_CTRL_REG + 0xE0)
#else
#ifdef CONFIG_MACH_NPCM650
#define GLOBAL_REG_GPIOP1PE_ADDR            (NPCMX50_GLOBAL_CTRL_REG + 0x1c)
#define GLOBAL_REG_GPIOP2PE_ADDR            (NPCMX50_GLOBAL_CTRL_REG + 0x20)
#define GLOBAL_REG_GPIOP3PE_ADDR            (NPCMX50_GLOBAL_CTRL_REG + 0x24)
#define GLOBAL_REG_GPIOP4PE_ADDR            (NPCMX50_GLOBAL_CTRL_REG + 0x28)
#define GLOBAL_REG_GPIOP5PE_ADDR            (NPCMX50_GLOBAL_CTRL_REG + 0x2c)
#define GLOBAL_REG_GPIOP6PE_ADDR            (NPCMX50_GLOBAL_CTRL_REG + 0x30)
#define GLOBAL_REG_GPIOP7PE_ADDR            (NPCMX50_GLOBAL_CTRL_REG + 0x34)
#define GLOBAL_REG_GPIOP8PE_ADDR            (NPCMX50_GLOBAL_CTRL_REG + 0x80)
#define GLOBAL_REG_GPIOP9PE_ADDR            (NPCMX50_GLOBAL_CTRL_REG + 0x84)
#define GLOBAL_REG_GPIOP10PE_ADDR           (NPCMX50_GLOBAL_CTRL_REG + 0x88)
#define GLOBAL_REG_GPIOP11PE_ADDR           (NPCMX50_GLOBAL_CTRL_REG + 0x8C)
#define GLOBAL_REG_GPIOP12PE_ADDR           (NPCMX50_GLOBAL_CTRL_REG + 0x90)
#endif
#endif

#ifdef CONFIG_MACH_NPCM750
#define GPIO_REG_GPNTLOCK1_ADDR(n)          (NPCMX50_GPIO_BASE_ADDR(n) + 0x0000)
#define GPIO_REG_GPNDIN_ADDR(n)             (NPCMX50_GPIO_BASE_ADDR(n) + 0x0004)
#define GPIO_REG_GPNPOL_ADDR(n)             (NPCMX50_GPIO_BASE_ADDR(n) + 0x0008)
#define GPIO_REG_GPNDOUT_ADDR(n)            (NPCMX50_GPIO_BASE_ADDR(n) + 0x000C)
#define GPIO_REG_GPNOE_ADDR(n)              (NPCMX50_GPIO_BASE_ADDR(n) + 0x0010)
#define GPIO_REG_GPNOTYP_ADDR(n)            (NPCMX50_GPIO_BASE_ADDR(n) + 0x0014)
#define GPIO_REG_GPNMP_ADDR(n)              (NPCMX50_GPIO_BASE_ADDR(n) + 0x0018)
#define GPIO_REG_GPNPU_ADDR(n)              (NPCMX50_GPIO_BASE_ADDR(n) + 0x001C)
#define GPIO_REG_GPNPD_ADDR(n)              (NPCMX50_GPIO_BASE_ADDR(n) + 0x0020)
#define GPIO_REG_GPNDBNC_ADDR(n)            (NPCMX50_GPIO_BASE_ADDR(n) + 0x0024)
#define GPIO_REG_GPNEVTYP_ADDR(n)           (NPCMX50_GPIO_BASE_ADDR(n) + 0x0028)
#define GPIO_REG_GPNEVBE_ADDR(n)            (NPCMX50_GPIO_BASE_ADDR(n) + 0x002C)
#define GPIO_REG_GPNOBL0_ADDR(n)            (NPCMX50_GPIO_BASE_ADDR(n) + 0x0030)
#define GPIO_REG_GPNOBL1_ADDR(n)            (NPCMX50_GPIO_BASE_ADDR(n) + 0x0034)
#define GPIO_REG_GPNOBL2_ADDR(n)            (NPCMX50_GPIO_BASE_ADDR(n) + 0x0038)
#define GPIO_REG_GPNOBL3_ADDR(n)            (NPCMX50_GPIO_BASE_ADDR(n) + 0x003C)
#define GPIO_REG_GPNEVEN_ADDR(n)            (NPCMX50_GPIO_BASE_ADDR(n) + 0x0040)
#define GPIO_REG_GPNEVENS_ADDR(n)           (NPCMX50_GPIO_BASE_ADDR(n) + 0x0044)
#define GPIO_REG_GPNEVENC_ADDR(n)           (NPCMX50_GPIO_BASE_ADDR(n) + 0x0048)
#define GPIO_REG_GPNEVST_ADDR(n)            (NPCMX50_GPIO_BASE_ADDR(n) + 0x004C)
#define GPIO_REG_GPNSPLCK_ADDR(n)           (NPCMX50_GPIO_BASE_ADDR(n) + 0x0050)
#define GPIO_REG_GPNMPLCK_ADDR(n)           (NPCMX50_GPIO_BASE_ADDR(n) + 0x0054)
#define GPIO_REG_GPNIEM_ADDR(n)             (NPCMX50_GPIO_BASE_ADDR(n) + 0x0058)
#define GPIO_REG_GPNOSRC_ADDR(n)            (NPCMX50_GPIO_BASE_ADDR(n) + 0x005C)
#define GPIO_REG_GPNODSC_ADDR(n)            (NPCMX50_GPIO_BASE_ADDR(n) + 0x0060)
#define GPIO_REG_GPNVER_ADDR(n)             (NPCMX50_GPIO_BASE_ADDR(n) + 0x0064)
#define GPIO_REG_GPNDOS_ADDR(n)             (NPCMX50_GPIO_BASE_ADDR(n) + 0x0068)
#define GPIO_REG_GPNDOC_ADDR(n)             (NPCMX50_GPIO_BASE_ADDR(n) + 0x006C)
#define GPIO_REG_GPNOES_ADDR(n)             (NPCMX50_GPIO_BASE_ADDR(n) + 0x0070)
#define GPIO_REG_GPNOEC_ADDR(n)             (NPCMX50_GPIO_BASE_ADDR(n) + 0x0074)
#define GPIO_REG_GPNTLOCK2_ADDR(n)          (NPCMX50_GPIO_BASE_ADDR(n) + 0x007C)


// pin number for port
UINT8 gpio_pin_number[MAX_GPIO_PORT] = {32,32,32,32,32,32,32,32};
UINT8 gpio_port_map[GPIO_PIN_MAX] = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
    4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
    4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
    5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
    5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
    6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7 };

UINT8 gpio_pin_map[GPIO_PIN_MAX] = {
    0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,
    0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,
    0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,
    0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,
    0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,
    0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,
    0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,
    0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31 };

UINT8 gpio_num_map[MAX_GPIO_PORT][MAX_GPIO_PIN] = {
    // port 0
    {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31},
    // port 1
    {32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63},
    // port 2
    {64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95},
    // port 3
    {96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127},
    // port 4
    {128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159},
    // port 5
    {160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191},
    // port 6
    {192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223},
    // port 7
    {224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255} };


#else
#ifdef CONFIG_MACH_NPCM650

#define GPIO_REG_EVENT_TYPE_ADDR            (GPIO_PORT_BASE_ADDR_MODULE0 + 0)
#define GPIO_REG_EVENT_POLARITY_ADDR        (GPIO_PORT_BASE_ADDR_MODULE0 + 0x4)
#define GPIO_REG_EVENT_DEBOUNCE_ADDR        (GPIO_PORT_BASE_ADDR_MODULE0 + 0x8)
#define GPIO_REG_EVENT_ENABLE_ADDR          (GPIO_PORT_BASE_ADDR_MODULE0 + 0xc)
#define GPIO_REG_EVENT_STATUS_ADDR          (GPIO_PORT_BASE_ADDR_MODULE0 + 0x10)

#define GPIO_REG_PORT0_CFG0_ADDR            (GPIO_PORT_BASE_ADDR_MODULE0 + 0x14)
#define GPIO_REG_PORT0_CFG1_ADDR            (GPIO_PORT_BASE_ADDR_MODULE0 + 0x18)
#define GPIO_REG_PORT0_DATAOUT_ADDR         (GPIO_PORT_BASE_ADDR_MODULE0 + 0x1c)
#define GPIO_REG_PORT0_DATAIN_ADDR          (GPIO_PORT_BASE_ADDR_MODULE0 + 0x20)

#define GPIO_REG_PORT1_CFG0_ADDR            (GPIO_PORT_BASE_ADDR_MODULE0 + 0x24)
#define GPIO_REG_PORT1_CFG1_ADDR            (GPIO_PORT_BASE_ADDR_MODULE0 + 0x28)
#define GPIO_REG_PORT1_CFG2_ADDR            (GPIO_PORT_BASE_ADDR_MODULE0 + 0x2c)
#define GPIO_REG_PORT1_BLINK_CTR_ADDR       (GPIO_PORT_BASE_ADDR_MODULE0 + 0x30)
#define GPIO_REG_PORT1_DATAOUT_ADDR         (GPIO_PORT_BASE_ADDR_MODULE0 + 0x34)
#define GPIO_REG_PORT1_DATAIN_ADDR          (GPIO_PORT_BASE_ADDR_MODULE0 + 0x38)

#define GPIO_REG_PORT2_CFG0_ADDR            (GPIO_PORT_BASE_ADDR_MODULE0 + 0x3c)
#define GPIO_REG_PORT2_CFG1_ADDR            (GPIO_PORT_BASE_ADDR_MODULE0 + 0x40)
#define GPIO_REG_PORT2_CFG2_ADDR            (GPIO_PORT_BASE_ADDR_MODULE0 + 0x44)
#define GPIO_REG_PORT2_DATAOUT_ADDR         (GPIO_PORT_BASE_ADDR_MODULE0 + 0x48)
#define GPIO_REG_PORT2_DATAIN_ADDR          (GPIO_PORT_BASE_ADDR_MODULE0 + 0x4c)

#define GPIO_REG_PORT3_CFG0_ADDR            (GPIO_PORT_BASE_ADDR_MODULE0 + 0x50)
#define GPIO_REG_PORT3_CFG1_ADDR            (GPIO_PORT_BASE_ADDR_MODULE0 + 0x54)
#define GPIO_REG_PORT3_CFG2_ADDR            (GPIO_PORT_BASE_ADDR_MODULE0 + 0x58)
#define GPIO_REG_PORT3_DATAOUT_ADDR         (GPIO_PORT_BASE_ADDR_MODULE0 + 0x5c)
#define GPIO_REG_PORT3_DATAIN_ADDR          (GPIO_PORT_BASE_ADDR_MODULE0 + 0x60)

#define GPIO_REG_PORT4_CFG0_ADDR            (GPIO_PORT_BASE_ADDR_MODULE0 + 0x64)
#define GPIO_REG_PORT4_CFG1_ADDR            (GPIO_PORT_BASE_ADDR_MODULE0 + 0x68)
#define GPIO_REG_PORT4_CFG2_ADDR            (GPIO_PORT_BASE_ADDR_MODULE0 + 0x6c)
#define GPIO_REG_PORT4_DATAOUT_ADDR         (GPIO_PORT_BASE_ADDR_MODULE0 + 0x70)
#define GPIO_REG_PORT4_DATAIN_ADDR          (GPIO_PORT_BASE_ADDR_MODULE0 + 0x74)

#define GPIO_REG_PORT5_CFG0_ADDR            (GPIO_PORT_BASE_ADDR_MODULE0 + 0x78)
#define GPIO_REG_PORT5_CFG1_ADDR            (GPIO_PORT_BASE_ADDR_MODULE0 + 0x7c)
#define GPIO_REG_PORT5_CFG2_ADDR            (GPIO_PORT_BASE_ADDR_MODULE0 + 0x80)
#define GPIO_REG_PORT5_DATAOUT_ADDR         (GPIO_PORT_BASE_ADDR_MODULE0 + 0x84)
#define GPIO_REG_PORT5_DATAIN_ADDR          (GPIO_PORT_BASE_ADDR_MODULE0 + 0x88)
 
#define GPIO_REG_PORT6_DATAIN_ADDR          (GPIO_PORT_BASE_ADDR_MODULE0 + 0x8c)

#define GPIO_REG_PORT7_CFG0_ADDR            (GPIO_PORT_BASE_ADDR_MODULE0 + 0x90)
#define GPIO_REG_PORT7_CFG1_ADDR            (GPIO_PORT_BASE_ADDR_MODULE0 + 0x94)
#define GPIO_REG_PORT7_CFG2_ADDR            (GPIO_PORT_BASE_ADDR_MODULE0 + 0x98)
#define GPIO_REG_PORT7_DATAOUT_ADDR         (GPIO_PORT_BASE_ADDR_MODULE0 + 0x9c)
#define GPIO_REG_PORT7_DATAIN_ADDR          (GPIO_PORT_BASE_ADDR_MODULE0 + 0xa0)

#define GPIO_REG_PORT8_CFG0_ADDR            (GPIO_PORT_BASE_ADDR_MODULE1 + 0x14)
#define GPIO_REG_PORT8_CFG1_ADDR            (GPIO_PORT_BASE_ADDR_MODULE1 + 0x18)
#define GPIO_REG_PORT8_DATAOUT_ADDR         (GPIO_PORT_BASE_ADDR_MODULE1 + 0x1c)
#define GPIO_REG_PORT8_DATAIN_ADDR          (GPIO_PORT_BASE_ADDR_MODULE1 + 0x20)

#define GPIO_REG_PORT9_CFG0_ADDR            (GPIO_PORT_BASE_ADDR_MODULE1 + 0x24)
#define GPIO_REG_PORT9_CFG1_ADDR            (GPIO_PORT_BASE_ADDR_MODULE1 + 0x28)
#define GPIO_REG_PORT9_CFG2_ADDR            (GPIO_PORT_BASE_ADDR_MODULE1 + 0x2c)
#define GPIO_REG_PORT9_BLINK_CTR_ADDR       (GPIO_PORT_BASE_ADDR_MODULE1 + 0x30)
#define GPIO_REG_PORT9_DATAOUT_ADDR         (GPIO_PORT_BASE_ADDR_MODULE1 + 0x34)
#define GPIO_REG_PORT9_DATAIN_ADDR          (GPIO_PORT_BASE_ADDR_MODULE1 + 0x38)

#define GPIO_REG_PORT10_CFG0_ADDR            (GPIO_PORT_BASE_ADDR_MODULE1 + 0x3c)
#define GPIO_REG_PORT10_CFG1_ADDR            (GPIO_PORT_BASE_ADDR_MODULE1 + 0x40)
#define GPIO_REG_PORT10_CFG2_ADDR            (GPIO_PORT_BASE_ADDR_MODULE1 + 0x44)
#define GPIO_REG_PORT10_DATAOUT_ADDR         (GPIO_PORT_BASE_ADDR_MODULE1 + 0x48)
#define GPIO_REG_PORT10_DATAIN_ADDR          (GPIO_PORT_BASE_ADDR_MODULE1 + 0x4c)

#define GPIO_REG_PORT11_CFG0_ADDR            (GPIO_PORT_BASE_ADDR_MODULE1 + 0x50)
#define GPIO_REG_PORT11_CFG1_ADDR            (GPIO_PORT_BASE_ADDR_MODULE1 + 0x54)
#define GPIO_REG_PORT11_CFG2_ADDR            (GPIO_PORT_BASE_ADDR_MODULE1 + 0x58)
#define GPIO_REG_PORT11_DATAOUT_ADDR         (GPIO_PORT_BASE_ADDR_MODULE1 + 0x5c)
#define GPIO_REG_PORT11_DATAIN_ADDR          (GPIO_PORT_BASE_ADDR_MODULE1 + 0x60)

#define GPIO_REG_PORT12_CFG0_ADDR            (GPIO_PORT_BASE_ADDR_MODULE1 + 0x64)
#define GPIO_REG_PORT12_CFG1_ADDR            (GPIO_PORT_BASE_ADDR_MODULE1 + 0x68)
#define GPIO_REG_PORT12_CFG2_ADDR            (GPIO_PORT_BASE_ADDR_MODULE1 + 0x6c)
#define GPIO_REG_PORT12_DATAOUT_ADDR         (GPIO_PORT_BASE_ADDR_MODULE1 + 0x70)
#define GPIO_REG_PORT12_DATAIN_ADDR          (GPIO_PORT_BASE_ADDR_MODULE1 + 0x74)

// pin number for port
UINT8 gpio_pin_number[MAX_GPIO_PORT] = {16,16,16,16,16,15,18,14,16,16,16,16,16};
UINT8 gpio_port_map[GPIO_PIN_MAX] = {
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,
    2,2,2,2,2,2,2,2,
    2,2,2,2,2,2,2,2,
    3,3,3,3,3,3,3,3,
    3,3,3,3,3,3,3,3,
    4,4,4,4,4,4,4,4,
    4,4,4,4,4,4,4,4,
    5,5,5,5,5,5,5,5,
    5,5,5,5,5,5,5,13,
    6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,
    6,6,7,7,7,7,7,7,
    7,7,7,7,7,7,7,7,
    8,8,8,8,8,8,8,8,
    8,8,8,8,8,8,8,8,
    9,9,9,9,9,9,9,9,
    9,9,9,9,9,9,9,9,
    10,10,10,10,10,10,10,10,
    10,10,10,10,10,10,10,10,
    11,11,11,11,11,11,11,11,
    11,11,11,11,11,11,11,11,
    12,12,12,12,12,12,12,12,
    12,12,12,12,12,12,12,12};

UINT8 gpio_pin_map[GPIO_PIN_MAX] = {
    0,1,2,3,4,5,6,7,
    8,9,10,11,12,13,14,15,
    0,1,2,3,4,5,6,7,
    8,9,10,11,12,13,14,15,
    0,1,2,3,4,5,6,7,
    8,9,10,11,12,13,14,15,
    0,1,2,3,4,5,6,7,
    8,9,10,11,12,13,14,15,
    0,1,2,3,4,5,6,7,
    8,9,10,11,12,13,14,15,
    0,1,2,3,4,5,6,7,
    8,9,10,11,12,13,14,15,
    0,1,2,3,4,5,6,7,
    8,9,10,11,12,13,14,15,
    16,17,0,1,2,3,4,5,
    6,7,8,9,10,11,12,13,
    0,1,2,3,4,5,6,7,
    8,9,10,11,12,13,14,15,
    0,1,2,3,4,5,6,7,
    8,9,10,11,12,13,14,15,
    0,1,2,3,4,5,6,7,
    8,9,10,11,12,13,14,15,
    0,1,2,3,4,5,6,7,
    8,9,10,11,12,13,14,15,
    0,1,2,3,4,5,6,7,
    8,9,10,11,12,13,14,15};

UINT8 gpio_num_map[MAX_GPIO_PORT][MAX_GPIO_PIN] = {
    // port 0
    {0,1,2,3,4,5,6,7,
    8,9,10,11,12,13,14,15},
    // port 1
    {16,17,18,19,20,21,22,23,
    24,25,26,27,28,29,30,31},
    // port 2
    {32,33,34,35,36,37,38,39,
    40,41,42,43,44,45,46,47},
    // port 3
    {48,49,50,51,52,53,54,55,
    56,57,58,59,60,61,62,63},
    // port 4
    {64,65,66,67,68,69,70,71,
    72,73,74,75,76,77,78,79},
    // port 5
    {80,81,82,83,84,85,86,87,
     88,89,90,91,92,93,94,95},
    // port 6
    {96,97,98,99,100,101,102,103,
     104,105,106,107,108,109,110,111,112,113},
    // port 7
    {114,115,116,117,118,119,120,121,
     122,123,124,125,126,127},
    // port 8
    {128,129,130,131,132,133,134,135,
    136,137,138,139,140,141,142,143},
    // port 9
    {144,145,146,147,148,149,150,151,
    152,153,154,155,156,157,158,159},
    // port 10
    {160,161,162,163,164,165,166,167,
    168,169,170,171,172,173,174,175},
    // port 11
    {176,177,178,179,180,181,182,183,
    184,185,186,187,188,189,190,191},
    // port 12
    {192,193,194,195,196,197,198,199,
    200,201,202,203,204,205,206,207}
    };

// port address mapping
// data-in reg mapping
UINT32 gpio_port_datain_addr[MAX_GPIO_PORT] = 
{
    (UINT32) GPIO_REG_PORT0_DATAIN_ADDR,
    (UINT32) GPIO_REG_PORT1_DATAIN_ADDR,
    (UINT32) GPIO_REG_PORT2_DATAIN_ADDR,
    (UINT32) GPIO_REG_PORT3_DATAIN_ADDR,
    (UINT32) GPIO_REG_PORT4_DATAIN_ADDR,
    (UINT32) GPIO_REG_PORT5_DATAIN_ADDR,
    (UINT32) GPIO_REG_PORT6_DATAIN_ADDR,
    (UINT32) GPIO_REG_PORT7_DATAIN_ADDR,
    (UINT32) GPIO_REG_PORT8_DATAIN_ADDR,
    (UINT32) GPIO_REG_PORT9_DATAIN_ADDR,
    (UINT32) GPIO_REG_PORT10_DATAIN_ADDR,
    (UINT32) GPIO_REG_PORT11_DATAIN_ADDR,
    (UINT32) GPIO_REG_PORT12_DATAIN_ADDR
};

// data-out reg mapping
UINT32 gpio_port_dataout_addr[MAX_GPIO_PORT] = 
{
    (UINT32) GPIO_REG_PORT0_DATAOUT_ADDR,
    (UINT32) GPIO_REG_PORT1_DATAOUT_ADDR,
    (UINT32) GPIO_REG_PORT2_DATAOUT_ADDR,
    (UINT32) GPIO_REG_PORT3_DATAOUT_ADDR,
    (UINT32) GPIO_REG_PORT4_DATAOUT_ADDR,
    (UINT32) GPIO_REG_PORT5_DATAOUT_ADDR,
    0,
    (UINT32) GPIO_REG_PORT7_DATAOUT_ADDR,
    (UINT32) GPIO_REG_PORT8_DATAOUT_ADDR,
    (UINT32) GPIO_REG_PORT9_DATAOUT_ADDR,
    (UINT32) GPIO_REG_PORT10_DATAOUT_ADDR,
    (UINT32) GPIO_REG_PORT11_DATAOUT_ADDR,
    (UINT32) GPIO_REG_PORT12_DATAOUT_ADDR
};

// data direction reg mapping
UINT32 gpio_port_datamode_addr[MAX_GPIO_PORT] = 
{
    (UINT32) GPIO_REG_PORT0_CFG0_ADDR,
    (UINT32) GPIO_REG_PORT1_CFG0_ADDR,
    (UINT32) GPIO_REG_PORT2_CFG0_ADDR,
    (UINT32) GPIO_REG_PORT3_CFG0_ADDR,
    (UINT32) GPIO_REG_PORT4_CFG0_ADDR,
    (UINT32) GPIO_REG_PORT5_CFG0_ADDR,
    0, // port 6 is read only
    (UINT32) GPIO_REG_PORT7_CFG0_ADDR,
    (UINT32) GPIO_REG_PORT8_CFG0_ADDR,
    (UINT32) GPIO_REG_PORT9_CFG0_ADDR,
    (UINT32) GPIO_REG_PORT10_CFG0_ADDR,
    (UINT32) GPIO_REG_PORT11_CFG0_ADDR,
    (UINT32) GPIO_REG_PORT12_CFG0_ADDR
};

// open drain reg mapping
UINT32 gpio_port_opendrain_addr[MAX_GPIO_PORT] = 
{
    (UINT32) GPIO_REG_PORT0_CFG1_ADDR,
    (UINT32) GPIO_REG_PORT1_CFG1_ADDR,
    (UINT32) GPIO_REG_PORT2_CFG1_ADDR,
    (UINT32) GPIO_REG_PORT3_CFG1_ADDR,
    (UINT32) GPIO_REG_PORT4_CFG1_ADDR,
    (UINT32) GPIO_REG_PORT5_CFG1_ADDR,
    0, // port 6 is read only
    (UINT32) GPIO_REG_PORT7_CFG1_ADDR,
    (UINT32) GPIO_REG_PORT8_CFG1_ADDR,
    (UINT32) GPIO_REG_PORT9_CFG1_ADDR,
    (UINT32) GPIO_REG_PORT10_CFG1_ADDR,
    (UINT32) GPIO_REG_PORT11_CFG1_ADDR,
    (UINT32) GPIO_REG_PORT12_CFG1_ADDR
};

// power source reg mapping
UINT32 gpio_port_power_addr[MAX_GPIO_PORT] = 
{
    0,
    (UINT32) GPIO_REG_PORT1_CFG2_ADDR,
    (UINT32) GPIO_REG_PORT2_CFG2_ADDR,
    (UINT32) GPIO_REG_PORT3_CFG2_ADDR,
    (UINT32) GPIO_REG_PORT4_CFG2_ADDR,
    (UINT32) GPIO_REG_PORT5_CFG2_ADDR,
    0, // port 6 is read only
    (UINT32) GPIO_REG_PORT7_CFG2_ADDR,
    0,
    (UINT32) GPIO_REG_PORT9_CFG2_ADDR,
    (UINT32) GPIO_REG_PORT10_CFG2_ADDR,
    (UINT32) GPIO_REG_PORT11_CFG2_ADDR,
    (UINT32) GPIO_REG_PORT12_CFG2_ADDR
};

// power source reg mapping
UINT32 gpio_port_pxpe_addr[MAX_GPIO_PORT] = 
{
    0,
    (UINT32) GLOBAL_REG_GPIOP1PE_ADDR,
    (UINT32) GLOBAL_REG_GPIOP2PE_ADDR,
    (UINT32) GLOBAL_REG_GPIOP3PE_ADDR,
    0,
    (UINT32) GLOBAL_REG_GPIOP5PE_ADDR,
    (UINT32) GLOBAL_REG_GPIOP6PE_ADDR,
    (UINT32) GLOBAL_REG_GPIOP7PE_ADDR,
    0,
    (UINT32) GLOBAL_REG_GPIOP9PE_ADDR,
    (UINT32) GLOBAL_REG_GPIOP10PE_ADDR,
    (UINT32) GLOBAL_REG_GPIOP11PE_ADDR,
    0
};
#endif
#endif

/******************************************************************************
*   STRUCT      :   sGPIOData
******************************************************************************/
/**
 *  @brief   Structure to GPIO driver related data parameter.
 *
 *****************************************************************************/
typedef struct
{
    /* Set read/write command type */
    UINT8 u8CommandType;
    
    /* Set command number */
    UINT8 u8CommandNum; 
    
    /* Set GPIO port number */
    UINT8 u8PortNum;       
    
    /* Set GPIO Pin number */
    UINT8 u8PinNum;        
    
    /* Data buffer */
    void *pData;                

} sGPIOData;

/******************************************************************************
*   STRUCT      :   sGPIOSelect
******************************************************************************/
/**
 *  @brief   Structure to GPIO driver related data parameter.
 *
 *****************************************************************************/
typedef struct
{
    /* multiple function pin select register address */
    UINT32 u32RegAddr;
    
    /* gpio enable bit */
    UINT8 u8RegBit; 
    
    /* gpio enable value */
    UINT8 u8BitEnableValue; 
} sGPIOSelect;

/******************************************************************************
*   STRUCT      :   Function Prototype 
******************************************************************************/
/**
 *  @brief   Prototype for each private function.
 *
 *****************************************************************************/
static int aess_gpio_write(sGPIOData *psdata);
  
static int aess_gpio_read(sGPIOData *psdata);

static int aess_gpio_config(sGPIOData *psdata);

int aess_gpio_commander(
                         /*bit 7~4->port, 3-0->pin */
                         UINT8 u8PortPin,  
                         /* Reference to command list */
                         UINT8 u8CommandType,
                         /* Reference to command list */
                         UINT8 u8CommandNum,
                         /* Data buffer, right now is UINT8 */
                         void *pData
                       );

#else
/* For external API to use */
extern int aess_gpio_commander(
                                  /* bit 7~4->port, 3-0->pin */
                                  UINT8 u8PortPin, 
                                  /* Reference to command list */
                                  UINT8 u8CommandType,
                                  /* Reference to command list */
                                  UINT8 u8CommandNum,
                                  /* Data buffer, right now is UINT8 */
                                  void *pData
                                );
#endif   /* AESSGPIODRV_C */

#endif   /* AESSGPIODRV_H */
