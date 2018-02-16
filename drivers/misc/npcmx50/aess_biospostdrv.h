/*
 * $RCSfile$
 * $Revision$
 * $Date$
 * $Author$
 *
 * BIOS Post Code driver.
 *
 * (C) 2006-2016 Avocent Corp.
 *
 * This file is subject to the terms and conditions of the GNU
 * General Public License. This program is distributed in the hope
 * that it will be useful, but WITHOUT ANY WARRANTY; without even
 * the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 */

#ifndef AESSBIOSPOSTDRV_H
#define AESSBIOSPOSTDRV_H

//#include <aess_drivers/aess_type.h>
#include "aess_biospostdrv_platform.h"


#ifdef AESSBIOSPOSTDRV_C

/* implement debug message with debugfs */
//#include <linux/aess_debugfs.h>


/* BIOSPOST Interrupt */
#define BIOS_KCS_HIB_INT 				KCS_HIB_INT


/* BIOSPOST Register Basse Address */
#define LPC_REG_BASE_ADDR          		NPCMX50_KCS_BASE_ADDR


/* BIOS POST Codes FIFO Registers */
#define BIOS_POST_REG_BASE_ADDR         LPC_REG_BASE_ADDR
#define BIOS_PS_REG_FIFO_LADDR_2        (void __iomem *) (BIOS_POST_REG_BASE_ADDR + 0x42)
#define BIOS_PS_REG_FIFO_MADDR_2        (void __iomem *) (BIOS_POST_REG_BASE_ADDR + 0x44)
#define BIOS_PS_REG_FIFO_ENABLE         (void __iomem *) (BIOS_POST_REG_BASE_ADDR + 0x46)
#define BIOS_PS_REG_FIFO_STATUS         (void __iomem *) (BIOS_POST_REG_BASE_ADDR + 0x48)
#define BIOS_PS_REG_FIFO_DATA           (void __iomem *) (BIOS_POST_REG_BASE_ADDR + 0x4A)
#define BIOS_PS_REG_FIFO_MISC_STATUS    (void __iomem *) (BIOS_POST_REG_BASE_ADDR + 0x4C)
#define BIOS_PS_REG_FIFO_LADDR_1        (void __iomem *) (BIOS_POST_REG_BASE_ADDR + 0x50)
#define BIOS_PS_REG_FIFO_MADDR_1        (void __iomem *) (BIOS_POST_REG_BASE_ADDR + 0x52)


/* BIOS interface package and structure definition */
#define BIOSPOST_KFIFO_SIZE     		0x400
#define BIOSPOST_KFIFO_SIZE_MINUS_1		(BIOSPOST_KFIFO_SIZE-1)

#define IOADDRESS1_INIT     			0x01
#define IOADDRESS2_INIT     			0x02
#define IOADDRESS_NOINIT    			0x0
#define NO_FILE_ID          			0x0
#define FILE_ID1            			0x1
#define FILE_ID2            			0x2
#define IO_NUMBER           			0x2

/* MICS */
#define SHIFT_0_BITS            		0x00
#define SHIFT_1_BITS            		0x01


/*BIOS regiser data*/
#define FIFO_READY_INT_ENABLE   		0x08
#define FIFO_READY_INT_DISABLE  		0xF7
#define FIFO_IOADDR0            		0x0
#define FIFO_IOADDR1            		0x01
#define FIFO_IOADDR_ENABLE      		0x80
#define FIFO_IOADDR_DISABLE     		0x80
#define FIFO_DATA_VALID         		0x80
#define FIFO_OVERFLOW           		0x20
#define FIFO_ADDR_DECODE        		0x01
#define FIFO_MATCH_ADDR1        		0x00
#define FIFO_MATCH_ADDR2        		0x01

#define DUPLICATE_POST_CODE_COUNT 		10
#define POST_CODE_INTS_NO_ACTION  		0
#define POST_CODE_INTS_DISABLE			1


/******************************************************************************
*   STRUCT      :   sBIOSFIFOtruct
******************************************************************************/
/**
 *  @brief   Structure to BIOS FIFO driver related buffer,
 *           used by linux driver.
 *
 *****************************************************************************/
typedef struct{

  /* data is input index*/
	UINT16 u16In;

  /* data is output index*/
	UINT16 u16Out;

	/* data is output index*/
	UINT16 u16Size;

	/* data is output index*/
	UINT16 Reserved;

	/* data buffer*/
	UINT8 au8Buffer[BIOSPOST_KFIFO_SIZE];

} sBIOSFIFOStruct;


/******************************************************************************
*   STRUCT      :   sBIOStruct
******************************************************************************/
/**
 *  @brief   Structure to BIOS driver related data parameter,
 *           used by linux driver.
 *
 *****************************************************************************/
typedef struct
{
	/** File ID*/
	UINT8 u8FileID;

	/** Record whether IO Address1 initialized*/
	UINT8  u8IOAddrInitFlag;

	/** BIOS Post Codes FIFO address 2 LSB*/
	UINT8 u8BIOSPostAddressLSB;

	/** BIOS Post Codes FIFO address 2 MSB*/
	UINT8 u8BIOSPostAddressMSB;

	/*FIFO for I/O Address1*/
	sBIOSFIFOStruct sIOAddrBuf;
} sBIOSPostStruct;

/* Declear BIOS Post Codes data structure */
static sBIOSPostStruct S_aBIOSPostData[IO_NUMBER];


/******************************************************************************
*   STRUCT      :   Function Prototype
******************************************************************************/
/**
 *  @brief   Prototype for each private function.
 *
 *****************************************************************************/


#endif   /* AESSBIOSPOSTDRV_C */

#endif   /* AESSBIOSPOSTDRV_H */
