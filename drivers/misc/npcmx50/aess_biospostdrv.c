/*
 * $RCSfile$
 * $Revision$
 * $Date$
 * $Author$
 *
 * NPCMX50 On chip BIOSPOST driver.
 *
 * (C) 2006-2016 Avocent Corp.
 *
 * This file is subject to the terms and conditions of the GNU
 * General Public License. This program is distributed in the hope
 * that it will be useful, but WITHOUT ANY WARRANTY; without even
 * the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 */

#define AESSBIOSPOSTDRV_C


#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>

#ifndef LINUX_VERSION_CODE
#include <linux/version.h>
#endif

#include <linux/spinlock.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/platform_device.h>

#include <mach/hal.h>

#include "aess_biospostdrv.h"

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>

extern void *kcs_virt_addr;
#undef LPC_REG_BASE_ADDR
#define LPC_REG_BASE_ADDR	(kcs_virt_addr)

extern int kcs_irq;
#undef BIOS_KCS_HIB_INT
#define BIOS_KCS_HIB_INT	kcs_irq

static const struct of_device_id biospost_dt_npcm750_match[] = {
       { .compatible = "nuvoton,npcm750-kcs" },
       { /*sentinel*/ },
};
#endif

//#define BIOSPOST_DEBUG 1
#define NEWDATA_OVERWRITE_OLDDATA_WHEN_OVERFLOW		1
#define POST_CODE_INTS_DISABLE_WHEN_TO_MANY_DUP		1

/* Debugging Message */
#ifdef BIOSPOST_DEBUG
#define DEBUG_MSG(fmt, args...) printk( "BIOSPOST: " fmt, ## args)
#else
#define DEBUG_MSG(fmt, args...)
#endif
#define PERROR(fmt, args...)	printk("aess_biospostdrv %s(): " fmt, __func__, ##args)

static int majornum = 0;           /* default to dynamic major */
module_param(majornum, int, 0);
MODULE_PARM_DESC(majornum, "Major device number");

/* driver name will passed by insmod command */
static char *driver_name = BIOSPOST_DRIVER_NAME;

/* Linux device data structure */
dev_t biospost_dev; 		/**< BIOSPOST Device */
struct cdev *biospost_cdev; /**< BIOSPOST cdev */

/** Sysfs class structure. */
static struct class *sys_class;

static spinlock_t S_spinlock;
static UINT8 S_u8Open = 0;

static struct platform_driver aess_biospost_device_driver = {
		.driver = {
			.name	= BIOSPOST_DRIVER_NAME,
			.owner	= THIS_MODULE,
	}
};


/******************************************************************************
*   FUNCTION        :   set_value
******************************************************************************/
/**
 *  @brief      Set attribute of sysfs exported file.
 *
 *  @return     String length of buf.
 *
 *  @dependency None
 *
 *  @limitation None
 *
 *  @warning    None
 *
 *  @note       None
 *
 *  @internal   Function Type: Private function \n
 *
 *****************************************************************************/
static ssize_t set_value(
						  /** Pointer of struct device_driver. */
						  struct device_driver *drv,
						  /** The string buffer contains new attribute. */
						  const char *buf,
						  /** Count. */
						  size_t count
						)
{
	return 0;
}


/******************************************************************************
*   FUNCTION        :   show_value
******************************************************************************/
/**
 *  @brief      Show attribute of sysfs exported file.
 *
 *  @return     On success, the total number of characters written is returned.\n
 *		On failure, a negative number is returned.
 *
 *  @dependency None
 *
 *  @limitation None
 *
 *  @warning    None
 *
 *  @note       None
 *
 *  @internal   Function Type: Private function \n
 *
 *****************************************************************************/
static ssize_t show_value(
						   /** Pointer of struct device_driver. */
						   struct device_driver *drv,
						   /** The buffer will contains string for showing. */
						   char *buf
						 )
{
	return 0;
}


struct driver_attribute drv_attr_interruptflag = {
	.attr = {.name  = "Status" , .mode   = S_IWUSR | S_IRUGO },
			 /* .owner = THIS_MODULE, .mode   = S_IWUSR | S_IRUGO }, */
	.show   = show_value,
	.store  = set_value,
};    /**< Sysfs driver attribute structure for bios post driver. */


/******************************************************************************
*   FUNCTION        :   aess_biospost_open
******************************************************************************/
/**
 *  @brief      The function is invoked when application open driver node.
 *
 *  @return     0 if driver execute the command success.
 *              Others if execute the command fail.
 *
 *  @dependency None
 *
 *  @limitation None
 *
 *  @warning    None
 *
 *  @note       None
 *
 *  @internal   Function Type: Global function \n
 *
 *****************************************************************************/
static int aess_biospost_open(struct inode *inode, struct file *filp)
{
	/* Set the initial value for data structure */
	if (0 == S_u8Open)
	{
		S_u8Open = 1;
		memset(S_aBIOSPostData, 0, sizeof(sBIOSPostStruct)*IO_NUMBER);
	}

	return (0);
}


/******************************************************************************
*   FUNCTION        :   aess_biospost_close
******************************************************************************/
/**
 *  @brief      The function is invoked when application close the device node.
 *
 *  @return     0 if driver execute the command success.
 *              Others if execute the command fail.
 *
 *  @dependency None
 *
 *  @limitation None
 *
 *  @warning    None
 *
 *  @note       None
 *
 *  @internal   Function Type: Global function \n
 *
 *****************************************************************************/
static int aess_biospost_close(struct inode* inode, struct file *filp)
{
	UINT8 u8Tmp = 0;
	UINT8 *pu8Data = (UINT8 *)filp->private_data;
	UINT8 i = 0;

	DEBUG_MSG("aess_BIOSPOST_close exit!\n");

	/*Check File ID*/
	if (((&S_aBIOSPostData[FIFO_IOADDR0].u8FileID) != pu8Data)  &&
		((&S_aBIOSPostData[FIFO_IOADDR1].u8FileID) != pu8Data))
	{
		/*illegal File ID*/
		return -EACCES;
	}

	u8Tmp = ioread8(BIOS_PS_REG_FIFO_ENABLE);

	if (NULL != pu8Data)
	{
		for (i = 0 ; i < IO_NUMBER; i++)
		{
			DEBUG_MSG("S_aBIOSPostData[%d].u8FileID is %x!\n", i, S_aBIOSPostData[i].u8FileID);

			/* disable IO ADDR */
			if (S_aBIOSPostData[i].u8FileID == pu8Data[0])
			{
				DEBUG_MSG("Disable IO Addr%d!\n", i);
				u8Tmp &= (~(FIFO_IOADDR_DISABLE >> i));
				S_aBIOSPostData[i].u8IOAddrInitFlag = IOADDRESS_NOINIT;

				if ((&S_aBIOSPostData[i].u8FileID) != pu8Data)
				{
					/*AP uses the same file descriptor to access two I/O ports*
					*Reset u8FileID of an I/O port which was initialized last*
					*Reset u8FileID of the other I/O port which was first initialized *
					* when exiting for() loop*/
					S_aBIOSPostData[i].u8FileID = NO_FILE_ID;
				}
			}
		}

		/*Reset S_aBIOSPostData[].u8FileID, Then Set Private Data to NULL*/
		pu8Data[0] = NO_FILE_ID;
		pu8Data = NULL;
	}

	/* disable BISO Post Code interrupts*/
	if (IOADDRESS_NOINIT == S_aBIOSPostData[FIFO_IOADDR0].u8IOAddrInitFlag &&
		IOADDRESS_NOINIT == S_aBIOSPostData[FIFO_IOADDR1].u8IOAddrInitFlag)
	{
		DEBUG_MSG("Disable BIOS Post Code Interrupt!\n");
		u8Tmp &= FIFO_READY_INT_DISABLE;
		S_u8Open = 0;
	}

	iowrite8(u8Tmp, BIOS_PS_REG_FIFO_ENABLE);

	return 0;
}


/******************************************************************************
*   FUNCTION        :   aess_biospost_drv_init
******************************************************************************/
/**
 *  @brief      Command handler for AESS_ BIOSPOSTDRV_INIT, used to initialize the I/O address.
 *
 *  @return     0 if driver execute the command success.
 *              Others if execute the command fail.
 *
 *  @dependency None
 *
 *  @limitation None
 *
 *  @warning    None
 *
 *  @note       None
 *
 *  @internal   Function Type: Internal function \n
 *
 *****************************************************************************/
static int aess_biospost_drv_init(sBIOSPostInfo *psInfo, struct file *filp)
{
	UINT8 u8Tmp = 0;
	UINT8 *pu8Data = (UINT8 *)filp->private_data;
	UINT8 u8IOIndex = psInfo->u8IOAddrEnFlag & 0x1;

	switch(u8IOIndex)
	{
		case FIFO_IOADDR0:
		case FIFO_IOADDR1:
			DEBUG_MSG("Enable IO Addr%x!\n",u8IOIndex);
			if (IOADDRESS_NOINIT == S_aBIOSPostData[u8IOIndex].u8IOAddrInitFlag)
			{
				S_aBIOSPostData[u8IOIndex].u8IOAddrInitFlag = IOADDRESS1_INIT;
				if (FIFO_IOADDR0 == u8IOIndex)
				{
					/*Set BIOS POST Codes FIFO Address 1 LSB and MSB*/
					S_aBIOSPostData[u8IOIndex].u8BIOSPostAddressLSB = psInfo->u8BIOSPostAddressLSB;
					S_aBIOSPostData[u8IOIndex].u8BIOSPostAddressMSB = psInfo->u8BIOSPostAddressMSB;
					iowrite8(psInfo->u8BIOSPostAddressLSB,BIOS_PS_REG_FIFO_LADDR_1);
					iowrite8(psInfo->u8BIOSPostAddressMSB,BIOS_PS_REG_FIFO_MADDR_1);
				}
				else
				{
					/*Set BIOS POST Codes FIFO Address 2 LSB and MSB*/
					S_aBIOSPostData[u8IOIndex].u8BIOSPostAddressLSB = psInfo->u8BIOSPostAddressLSB;
					S_aBIOSPostData[u8IOIndex].u8BIOSPostAddressMSB = psInfo->u8BIOSPostAddressMSB;
					iowrite8(psInfo->u8BIOSPostAddressLSB,BIOS_PS_REG_FIFO_LADDR_2);
					iowrite8(psInfo->u8BIOSPostAddressMSB,BIOS_PS_REG_FIFO_MADDR_2);
				}

				if (((&S_aBIOSPostData[FIFO_IOADDR0].u8FileID) != filp->private_data) &&
					((&S_aBIOSPostData[FIFO_IOADDR1].u8FileID) != filp->private_data))
				{
					/*A new file ID*/
					S_aBIOSPostData[u8IOIndex].u8FileID = (FILE_ID1 << u8IOIndex);
					filp->private_data = &S_aBIOSPostData[u8IOIndex].u8FileID;
				}
				else
				{
					/*An application initialize two I/O address*/
					S_aBIOSPostData[u8IOIndex].u8FileID = pu8Data[0];
				}

				DEBUG_MSG("S_aBIOSPostData[0].u8FileID is %x!\n", S_aBIOSPostData[FIFO_IOADDR0].u8FileID);
			}
			else
			{
				/*Check File ID*/
				if (((&S_aBIOSPostData[FIFO_IOADDR0].u8FileID) != filp->private_data) &&
					((&S_aBIOSPostData[FIFO_IOADDR1].u8FileID) != filp->private_data))
				{
					/* The I/O address is initialized by other applicaton already.
					The current application has no permission to access this I/O
					address. The file ID is illeagal */

					return -EACCES;
				}
			}
			break;

		default:
			/* IO Address invalid */
			return -EINVAL;
			break;
	}

	/* Enable FIFO Ready Interrupt and FIFO Capture of I/O address */
	u8Tmp = ioread8(BIOS_PS_REG_FIFO_ENABLE)|FIFO_READY_INT_ENABLE;
	u8Tmp |= (FIFO_IOADDR_ENABLE >> psInfo->u8IOAddrEnFlag);
	iowrite8(u8Tmp, BIOS_PS_REG_FIFO_ENABLE);

	return 0;
}


/******************************************************************************
*   FUNCTION        :   aess_biospost_read_all
******************************************************************************/
/**
 *  @brief      Sub command handler for AESS_BIOSPOSTDRV_READ uses to read the buffer content to user space.
 *
 *  @return     0 if driver execute the command success.
 *              Others if execute the command fail.
 *
 *  @dependency None
 *
 *  @limitation None
 *
 *  @warning    None
 *
 *  @note       None
 *
 *  @internal   Function Type: Internal function \n
 *
 *****************************************************************************/
static int aess_biospost_read_all(sBIOSPostInfo *psInfo)
{
	UINT16 local_Index = 0;
	UINT16 local_Len = 0;
	UINT16 u16Len = 0;
	UINT8 u8IOIndex = 0;


	u8IOIndex = psInfo->u8IOAddrEnFlag;

	switch(u8IOIndex)
	{
		case FIFO_IOADDR0:
		case FIFO_IOADDR1:

			/*Read data from KFIFO buffer*/
			if (S_aBIOSPostData[u8IOIndex].sIOAddrBuf.u16Size > 0)
			{
				u16Len = min(S_aBIOSPostData[u8IOIndex].sIOAddrBuf.u16Size, psInfo->u16MaxReadLen);
                
				local_Index = S_aBIOSPostData[u8IOIndex].sIOAddrBuf.u16Out & BIOSPOST_KFIFO_SIZE_MINUS_1;
                local_Len = BIOSPOST_KFIFO_SIZE - local_Index;
                local_Len = min(u16Len, local_Len);
        		if (copy_to_user((void __user *)psInfo->pu8Data, S_aBIOSPostData[u8IOIndex].sIOAddrBuf.au8Buffer+local_Index, local_Len))
            	{
            		PERROR("copy_to_user error!!\n");
            	    return -EFAULT;
            	}
                if (local_Len < u16Len) // copy the rest (Iif exist) from begnnig of cyclic buffer
                {
            		if (copy_to_user((void __user *)psInfo->pu8Data+local_Len, S_aBIOSPostData[u8IOIndex].sIOAddrBuf.au8Buffer, u16Len-local_Len))
                	{
                		PERROR("copy_to_user error!!\n");
                	    return -EFAULT;
                	}
                }
                S_aBIOSPostData[u8IOIndex].sIOAddrBuf.u16Out += u16Len;
                S_aBIOSPostData[u8IOIndex].sIOAddrBuf.u16Out &= BIOSPOST_KFIFO_SIZE_MINUS_1;
				psInfo->u16CopyLen = u16Len;
				S_aBIOSPostData[u8IOIndex].sIOAddrBuf.u16Size -= u16Len;

			}else
			{
				psInfo->u16CopyLen = 0;
				if (0 == (ioread8(BIOS_PS_REG_FIFO_ENABLE) & FIFO_READY_INT_ENABLE))
				{
					// DF331275: iDRAC6 connectivity lost during RTC_Wake cycling
					// Return error status if interrupt is disabled so that app-level
					// can take action if necessary.
					return (-1);
				}
			}
			break;

		default:
			return (-1);
			break;
	}

	return (0);
}


/******************************************************************************
*   FUNCTION        :   aess_biospost_read
******************************************************************************/
/**
 *  @brief      Command handler for AESS_BIOSPOSTDRV_READ uses to pass the incoming data to user space.
 *              The following is the list of different read commands:
 *              1.	BUF_READ_ALL
 *                  Read buffer content to user space and renew read size and position.
 *              2.	BUF_PEEK_RECENT
 *                  Peek the most recent data from the buffer without changing the read size and position.
 *              3.	BUF_PEEK_ALL
 *                  Copy the buffer content to user space without changing the read size and position.
 *
 *  @return     0 if driver execute the command success.
 *              Others if execute the command fail.
 *
 *  @dependency None
 *
 *  @limitation None
 *
 *  @warning    None
 *
 *  @note       None
 *
 *  @internal   Function Type: Internal function \n
 *
 *****************************************************************************/
static int aess_biospost_read(
							  /** point to sBIOSPostInfo pass comes from ioctl function */
							  sBIOSPostInfo *psInfo,

							 /** file structure point */
							  struct file *filp
							  )
{
	int err_check;

	/** @scope */

	/*Check File ID*/
	if (((&S_aBIOSPostData[FIFO_IOADDR0].u8FileID) != filp->private_data) &&
		((&S_aBIOSPostData[FIFO_IOADDR1].u8FileID) != filp->private_data))
	{
		/*illegal */
		printk("illegal File Descriptioin\n");
		return -EACCES;
	}

	/** Check which type of read operation should be performed and do the corresponding operation */
	switch(psInfo->u8ReadOption)
	{
		case BUF_READ_ALL:
		case BUF_PEEK_RECENT:
		case BUF_PEEK_ALL:
			err_check = aess_biospost_read_all(psInfo);
			break;

		default:
			//DRV_ERROR("aess_biospost_read()::ReadOption(Unknown)::return( -EINVAL)\n" );
			err_check = -EINVAL;
			break;
	}

	return err_check;
}


/******************************************************************************
*   FUNCTION        :   aess_biospost_reset
******************************************************************************/
/**
 *  @brief      Command handler for AESS_BIOSPOSTDRV_RESET uses to clean biospost FIFO.
 *
 *  @return     0 if driver execute the command success.
 *              Others if execute the command fail.
 *
 *  @dependency None
 *
 *  @limitation None
 *
 *  @warning    None
 *
 *  @note       None
 *
 *  @internal   Function Type: Global function \n
 *
 *****************************************************************************/
static int aess_biospost_reset(sBIOSPostInfo *psInfo, struct file *filp)
{
	UINT8 u8IOIndex = 0;
	UINT8 u8Tmp = 0;

	/*Check File ID*/
	if (((&S_aBIOSPostData[FIFO_IOADDR0].u8FileID) != filp->private_data) &&
		((&S_aBIOSPostData[FIFO_IOADDR1].u8FileID) != filp->private_data))
	{
		/*illegal */
		return -EACCES;
	}

	u8IOIndex = psInfo->u8IOAddrEnFlag;

	switch(psInfo->u8IOAddrEnFlag)
	{
		case FIFO_IOADDR0:
		case FIFO_IOADDR1:
			S_aBIOSPostData[u8IOIndex].sIOAddrBuf.u16In = 0;
			S_aBIOSPostData[u8IOIndex].sIOAddrBuf.u16Out = 0;
			S_aBIOSPostData[u8IOIndex].sIOAddrBuf.u16Size = 0;

			/*Enable FIFO Ready Interrupt and FIFO Capture of I/O address*/
			u8Tmp = ioread8(BIOS_PS_REG_FIFO_ENABLE)|FIFO_READY_INT_ENABLE;
			u8Tmp |= (FIFO_IOADDR_ENABLE >> psInfo->u8IOAddrEnFlag);
			iowrite8(u8Tmp, BIOS_PS_REG_FIFO_ENABLE);
			break;

		default:
			/* IO Address invalid */
			return -EINVAL;
			break;
	}
	return (0);
}


static int aess_biospost_clearall(void)
{
	/* Set the initial value for data structure */
	memset(S_aBIOSPostData, 0, sizeof(sBIOSPostStruct) * IO_NUMBER);

	/* Release IRQ */
	free_irq(BIOS_KCS_HIB_INT, (void *) &S_aBIOSPostData);
	return (0);
}


/******************************************************************************
*   FUNCTION        :   aess_biospost_isr
******************************************************************************/
/**
 *  @brief      The function is the entry function of interrupt handler
 *
 *  @return     IRQ_NONE if the IRQ is not biospost .
 *              IRQ_HANDLED if biospsot interrupt occured.
 *
 *  @dependency None
 *
 *  @limitation None
 *
 *  @warning    None
 *
 *  @note       None
 *
 *  @internal   Function Type: Internal function \n
 *
 *****************************************************************************/
static irqreturn_t aess_biospost_isr (int irq, void *dev_id)
{
	#if POST_CODE_INTS_DISABLE_WHEN_TO_MANY_DUP
	UINT8 u8DisableISR;
	UINT32 i;
	#endif
	UINT8 u8Tmp = 0;
	unsigned long u32Flag = 0;
	UINT32 u32Index = 0;
	UINT8 u8IOIndex= 0;
	UINT8 u8Data = 0;
	static UINT8 S_u8ISRFlag = 0;
	DEBUG_MSG("BIOS Post Code ISR\n");

	S_u8ISRFlag = 0;

	spin_lock_irqsave(&S_spinlock, u32Flag);
	u8Tmp = ioread8(BIOS_PS_REG_FIFO_STATUS);
	while(FIFO_DATA_VALID&u8Tmp)
	{
		u8IOIndex = (u8Tmp&FIFO_ADDR_DECODE);

		/*Read data from FIFO to clear interrupt*/
		u8Data = ioread8(BIOS_PS_REG_FIFO_DATA);
		//printk("Data in BIOS H/W FIFO is %x\n",u8Data);

		#if NEWDATA_OVERWRITE_OLDDATA_WHEN_OVERFLOW
		if(S_aBIOSPostData[u8IOIndex].sIOAddrBuf.u16Size == BIOSPOST_KFIFO_SIZE)
		{
			/*New data overwrite old data.*/
			S_aBIOSPostData[u8IOIndex].sIOAddrBuf.u16Out &= BIOSPOST_KFIFO_SIZE_MINUS_1;
			S_aBIOSPostData[u8IOIndex].sIOAddrBuf.u16Out++;
			S_aBIOSPostData[u8IOIndex].sIOAddrBuf.u16Size--;
		}
		S_aBIOSPostData[u8IOIndex].sIOAddrBuf.u16In &= BIOSPOST_KFIFO_SIZE_MINUS_1;
		u32Index = S_aBIOSPostData[u8IOIndex].sIOAddrBuf.u16In;
		S_aBIOSPostData[u8IOIndex].sIOAddrBuf.au8Buffer[u32Index]= u8Data;
		S_aBIOSPostData[u8IOIndex].sIOAddrBuf.u16In++;
		S_aBIOSPostData[u8IOIndex].sIOAddrBuf.u16Size++;


		#else
		/*New data is skipped.*/
		if(S_aBIOSPostData[u8IOIndex].sIOAddrBuf.u16Size < BIOSPOST_KFIFO_SIZE)
		{
			S_aBIOSPostData[u8IOIndex].sIOAddrBuf.u16In &= BIOSPOST_KFIFO_SIZE_MINUS_1;
			u32Index = S_aBIOSPostData[u8IOIndex].sIOAddrBuf.u16In;
			S_aBIOSPostData[u8IOIndex].sIOAddrBuf.au8Buffer[u32Index]= u8Data;
			S_aBIOSPostData[u8IOIndex].sIOAddrBuf.u16In++;
			S_aBIOSPostData[u8IOIndex].sIOAddrBuf.u16Size++;

		}
		#endif

		// The following code block prevents iDRAC crashes when BIOS
		// goes into a spin loop sending a constant stream of error POST
		// codes, such as "CPU configuration error."
		#if POST_CODE_INTS_DISABLE_WHEN_TO_MANY_DUP
		if (u32Index > DUPLICATE_POST_CODE_COUNT)
		{
			u8DisableISR = POST_CODE_INTS_DISABLE;
			for (i=u32Index; i >= u32Index - DUPLICATE_POST_CODE_COUNT; i--)
			{
				if (u8Data != S_aBIOSPostData[u8IOIndex].sIOAddrBuf.au8Buffer[i-1])
				{
					u8DisableISR = POST_CODE_INTS_NO_ACTION;
					break;  // Last 10 POST Codes are not all equal
				}
			}

			if (u8DisableISR == POST_CODE_INTS_DISABLE)
			{
				DEBUG_MSG("Disabling POST CODE interrupts\n");
				u8Tmp = ioread8(BIOS_PS_REG_FIFO_ENABLE) & ~(FIFO_READY_INT_ENABLE);
				u8Tmp &= ~(FIFO_IOADDR_ENABLE >> u8IOIndex);
				iowrite8(u8Tmp, BIOS_PS_REG_FIFO_ENABLE);
			}
		}
		#endif

		if(u8Tmp&FIFO_OVERFLOW)
		{
			DEBUG_MSG("BIOS Post Codes FIFO Overflow!!!\n");
		}
		u8Tmp = ioread8(BIOS_PS_REG_FIFO_STATUS);
		S_u8ISRFlag =1;
	}

	spin_unlock_irqrestore(&S_spinlock, u32Flag);

	if(1==S_u8ISRFlag)
	{
		return IRQ_HANDLED;
	}else
	{
		return IRQ_NONE;
	}
}


/******************************************************************************
*   FUNCTION        :   aess_biospost_ioctl
******************************************************************************/
/**
 *  @brief      The API is invoked when user space application issues an IOCTL command.
 *              The following is the list of commands that supported currently.
 *              1.	AESS_BIOSPOSTDRV_INIT
 *                  Initialize Linux BIOS Post Codes driver according to the given parameters.
 *              2.	AESS_BIOSPOSTDRV_READ
 *                  Copy data from driver to user space application.
 *              3.	AESS_BIOSPOSTDRV_RESET
 *                  Reset index and size of a data buffer to zero.
 *
 *  @return     0 if driver execute the command success.
 *              Others if execute the command fail.
 *
 *  @dependency None
 *
 *  @limitation None
 *
 *  @warning    None
 *
 *  @note       None
 *
 *  @internal   Function Type: Global function \n
 *
 *****************************************************************************/
static long aess_biospost_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int err_check;
	sBIOSPostInfo psInfo;

	if (copy_from_user(&psInfo, (void __user *) arg, sizeof(psInfo)))
	{
		PERROR("copy_from_user error!!\n");
	    return -EFAULT;
	}

	switch (cmd)
	{
		case AESS_BIOSPOSTDRV_INIT:
			err_check = aess_biospost_drv_init(&psInfo, filp);
			break;

		case AESS_BIOSPOSTDRV_READ:
			err_check = aess_biospost_read(&psInfo, filp);
			break;

		case AESS_BIOSPOSTDRV_RESET:
			err_check = aess_biospost_reset(&psInfo, filp);
			break;

		default:
			printk(KERN_ERR "KN:aess_BIOSPOST_ioctl, command error\n");
			err_check = -EINVAL;
	}
    
    if (err_check == 0) {
		if (copy_to_user((void __user *)arg, &psInfo, sizeof(psInfo)))
    	{
    		PERROR("copy_to_user error!!\n");
    	    return -EFAULT;
    	}
    }

	/* 0->ok, minus->fail */
	return err_check;
}


struct file_operations aess_biospost_fops = {
	.open = aess_biospost_open,
	.unlocked_ioctl = aess_biospost_ioctl,
	.release = aess_biospost_close,
};


/******************************************************************************
*   FUNCTION        :   aess_biospost_init
******************************************************************************/
/**
 *  @brief      The function is invoked when biospost module is loaded into the kernel.
 *
 *  @return     0 if driver initial success.
 *              Others if initial fail.
 *
 *  @dependency None
 *
 *  @limitation None
 *
 *  @warning    None
 *
 *  @note       None
 *
 *  @internal   Function Type: Global function  \n
 *
 *****************************************************************************/
int __init aess_biospost_init(void)
{
	int result = 0;

	biospost_cdev = cdev_alloc();
	biospost_cdev->ops = &aess_biospost_fops;

	/** - initial S_spinlok */
	spin_lock_init(&S_spinlock);

	if (majornum <= 0)
	{
		/* allocate a device number */
		result = alloc_chrdev_region(&biospost_dev, 0, 1, driver_name);
		majornum = MAJOR(biospost_dev);
	}
	else
	{
		biospost_dev = MKDEV(majornum, 0);
		result = register_chrdev_region(biospost_dev, 1, driver_name);
	}

	/** - allocate device numbers */
	//result = alloc_chrdev_region(&biospost_dev, 0, 1, driver_name);

	if (result < 0) {
		printk (KERN_ERR "KN:Registering the BIOSPOST device failed with %d\n", MAJOR(biospost_dev));
		return result;
	}
	printk("mknod /dev/aess_biospostdrv c %d 0\n", MAJOR(biospost_dev));

	/** - add driver as a char device to the system */
	cdev_add(biospost_cdev, biospost_dev, 1);

	/* Register IRQ*/
	result = request_irq(BIOS_KCS_HIB_INT, aess_biospost_isr, IRQF_SHARED,
						 driver_name, (void *) &S_aBIOSPostData);

	if (result < 0)
	{
		printk(KERN_ERR "request_irq error! - %d\n", result);
		return result;
	}

	/** - initial debugfs related resource */
	//aess_debugfs_default_create(driver_name, NULL, 0, 0);

	/** - Register driver with sysfs class. */
	sys_class = class_create(THIS_MODULE, driver_name);
	device_create(sys_class, NULL, biospost_dev, NULL, driver_name);

	/** - register sysfs */
	result = platform_driver_register(&aess_biospost_device_driver);
	if (result)
	{
		printk(KERN_ERR "%s(): can't register sysfs.\n", __FUNCTION__);
		return result;
	}

	/** - create sysfs files */
	result = driver_create_file(&(aess_biospost_device_driver.driver), &drv_attr_interruptflag);
	if (result)
	{
		printk(KERN_ERR "%s(): Fail to create sysfs attrb.\n", __FUNCTION__);
		return result;
	}

	return result;
}


/******************************************************************************
*   FUNCTION        :   aess_biospost_exit
******************************************************************************/
/**
 *  @brief      The function is invoked when biospost driver be removed from kernal.
 *
 *  @return     None.
 *
 *  @dependency None
 *
 *  @limitation None
 *
 *  @warning    None
 *
 *  @note       None
 *
 *  @internal   Function Type: Global function \n
 *
 *****************************************************************************/
void __exit aess_biospost_exit(void)
{
	aess_biospost_clearall();

	DEBUG_MSG("aess_BIOSPOST_exit\n");

	/** - Destory sysfs class. */
	device_destroy(sys_class, biospost_dev);
	class_destroy(sys_class);

	/** - remove device driver */
	cdev_del(biospost_cdev);

	/** - unregister device driver */
	unregister_chrdev_region(biospost_dev, 1);

	/** - remove debugfs module */
	//aess_debugfs_remove(driver_name,NULL,0,0);

	/** - Remove sysfs files. */
	driver_remove_file(&(aess_biospost_device_driver.driver), &drv_attr_interruptflag);

	/** - Unregister sysfs. */
	platform_driver_unregister(&(aess_biospost_device_driver));

	return;
}

MODULE_DESCRIPTION("AESS BIOSPOST Driver");
MODULE_AUTHOR("Justin Lee <justin.lee@emersion.com>");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("0.0.0.5");
module_param(driver_name, charp, S_IRUGO);
module_init(aess_biospost_init);
module_exit(aess_biospost_exit);

