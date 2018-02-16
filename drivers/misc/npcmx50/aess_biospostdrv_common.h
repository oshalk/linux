/*
*
* Copyright (C) 2009,2010 Avocent Corporation
*
* This file is subject to the terms and conditions of the GNU
* General Public License Version 2. This program is distributed in the hope
* that it will be useful, but WITHOUT ANY WARRANTY; without even
* the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
* PURPOSE. See the GNU General Public License Version 2 for more details.
*

 *----------------------------------------------------------------------------\n
 *  MODULES     Bios post code linux driver.\n
 *----------------------------------------------------------------------------\n
 *  @file   aess_biospostdrv_common.h
 *  @brief  This is a device driver for biospost code.
 *
 *  @internal
 *----------------------------------------------------------------------------*/

#ifndef AESSBIOSPOSTDRV_COMMON_H
#define AESSBIOSPOSTDRV_COMMON_H

/* IOCTL command */
#define AESS_BIOSPOSTDRV_IOC_MAGIC    0xCF          /**< I/O magic number of bios post driver. */
#define AESS_BIOSPOSTDRV_INIT         _IOWR(AESS_BIOSPOSTDRV_IOC_MAGIC, 0, int) /**< I/O control command: initial bios post device. */
#define AESS_BIOSPOSTDRV_READ         _IOWR(AESS_BIOSPOSTDRV_IOC_MAGIC, 1, int) /**< I/O control command: read bios post code. */
#define AESS_BIOSPOSTDRV_RESET        _IOWR(AESS_BIOSPOSTDRV_IOC_MAGIC, 2, int) /**< I/O control command: reset bios post buffer. */

/******************************************************************************
*   STRUCT      :    sBIOSPostInfo
******************************************************************************/
/**
 *  @brief    BIOS Post Parameters definition from User space
 *
 *****************************************************************************/
typedef struct
{
    /** Data buffer one byte len.*/
    UINT8 *pu8Data;

    /** Read data size Already.*/
    UINT16 u16MaxReadLen;

    /** Read data size Already.*/
    UINT16 u16CopyLen;

    /** BIOS Post Codes FIFO address 2 LSB.*/
    UINT8 u8BIOSPostAddressLSB;

    /** BIOS Post Codes FIFO address 2 MSB.*/
    UINT8 u8BIOSPostAddressMSB;

    /** I/O Address Enable Flag.*/
    UINT8 u8IOAddrEnFlag;

    /** Type of Read Operation.*/
    UINT8 u8ReadOption;

} sBIOSPostInfo;

#define BIOSPOST_DRIVER_NAME "aess_biospostdrv"  /**< Device name of biospost device. */

#define SNP_NOT_INITIALED   0               /**< Used to indicate the bios post code channel has not been initialed. */
#define SNP_INITIALED       1               /**< Used to indicate the bios post code channel has been initialed. */

#define BUF_READ_ALL	    0	/**< Read all data from buffer */
#define BUF_PEEK_RECENT	    1	/**< Only Peek the most recent data in buffer */
#define BUF_PEEK_ALL        2	/**< Only Peek all data in buffer */

#endif   /* AESSBIOSPOSTDRV_COMMON_H */
/* End of code */
