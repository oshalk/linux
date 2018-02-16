/*
 * $RCSfile$
 * $Revision$
 * $Date$
 * $Author$
 *
 * NPCMX50 On chip SSPI driver.
 *
 * Copyright (C) 2006 Avocent Corp.
 *
 * This file is subject to the terms and conditions of the GNU
 * General Public License. This program is distributed in the hope
 * that it will be useful, but WITHOUT ANY WARRANTY; without even
 * the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 */

#define AESSSSPIDRV_C
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/semaphore.h>
#include <linux/sched.h>
#include <asm/atomic.h>
#include <asm/bitops.h>
#include <linux/spinlock.h>

#include <mach/hal.h>

#include <mach/regs_npcm750_clk.h>
#include <mach/regs_npcm750_gcr.h>



//#include <mach/regs_npcm750_clk.h>


//#ifdef CONFIG_MACH_NPCM750
//#include <mach/../../BMC_HAL/Modules/aic/Poleg_IP/aic_regs.h>
//#else
//#include <mach/../../BMC_HAL/Modules/aic/Hermon_IP/aic_regs.h>
//#endif


#include "aess_sspidrv.h"
#include "aess_gpiodrv.h" 

static int pspi_virt_addr=0;

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

#undef NPCMX50_PSPI_BASE_ADDR

#define NPCMX50_PSPI_BASE_ADDR(module)   (pspi_virt_addr + ((module) * 0x1000L))

#undef PSPI1_INTERRUPT
#undef PSPI2_INTERRUPT
#define PSPI1_INTERRUPT pspi_irq[0]
#define PSPI2_INTERRUPT pspi_irq[1]
int pspi_irq[2];

static const struct of_device_id pspi_dt_npcm750_match[] = {
       { .compatible = "nuvoton,npcm750-pspi" },
       { /*sentinel*/ },
};
#endif


/* enable to print out message */
//#define SSPI_DEBUG

#ifdef SSPI_DEBUG
#define DEBUGP(fmt, args...)  printk("SSPI: %s() " fmt, __func__ , ##args)
#else
#define DEBUGP(fmt, args...)
#endif
#define PERROR(fmt, args...)  printk("SSPI: %s() " fmt, __func__ , ##args)

static int majornum = 0;           /* default to dynamic major */
module_param(majornum, int, 0);
MODULE_PARM_DESC(majornum, "Major device number");

extern void npcmx50_spin_lock_irqsave(unsigned long *flags);
extern void npcmx50_spin_unlock_irqrestore(unsigned long flags);
extern void npcmx50_atomic_io_modify(void __iomem *reg, u32 mask, u32 set);


#undef PSPI_DATA
#undef PSPI_CTL1
#undef PSPI_STAT

/*---------------------------------------------------------------------------------------------------------*/
/*                                                Registers                                                */
/*---------------------------------------------------------------------------------------------------------*/
#define PSPI_DATA(module)       (NPCMX50_PSPI_BASE_ADDR(module) + 0x00),    NPCMX50_PSPI_ACCESS,   16
#define PSPI_CTL1(module)       (NPCMX50_PSPI_BASE_ADDR(module) + 0x02),    NPCMX50_PSPI_ACCESS,   16
#define PSPI_STAT(module)       (NPCMX50_PSPI_BASE_ADDR(module) + 0x04),    NPCMX50_PSPI_ACCESS,    8


/*---------------------------------------------------------------------------------------------------------*/
/*                                                 Fields                                                  */
/*---------------------------------------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------------------------------------*/
/* PSPI_CTL1 fields                                                                                        */
/*---------------------------------------------------------------------------------------------------------*/
#define PSPI_CTL1_SPIEN         0, 1
#define PSPI_CTL1_EIR           5, 1
#define PSPI_CTL1_EIW           6, 1
#define PSPI_CTL1_SCM           7, 1
#define PSPI_CTL1_SCIDL         8, 1
#define PSPI_CTL1_SCDV6_0       9, 7


/*---------------------------------------------------------------------------------------------------------*/
/* PSPI_STAT fields                                                                                        */
/*---------------------------------------------------------------------------------------------------------*/
#define PSPI_STAT_BSY           0, 1
#define PSPI_STAT_RBF           1, 1



#define SSPI_GCR_BASE_REG        NPCMX50_GCR_BASE_ADDR
#define SSPI_GCR_PDID_REG        (SSPI_GCR_BASE_REG+0x0)  /* read only reg */

#define SSPI_GPIO_MODULE0_BASE_ADDR                          NPCMX50_GPIO_BASE_ADDR(0)
#define SSPI_GPIO_MODULE1_BASE_ADDR                          NPCMX50_GPIO_BASE_ADDR(1)

#define SSPI_GPIO_REG_MODULE0_PORT1_DATAOUT_ADDR          (SSPI_GPIO_MODULE0_BASE_ADDR + 0x34)
#define SSPI_GPIO_REG_MODULE1_PORT0_DATAOUT_ADDR          (SSPI_GPIO_MODULE1_BASE_ADDR + 0x1C)


/* definitions for control and status register */
#define SSPI_ENABLE            0x01
#define SSPI_EIR               0x20
#define SSPI_EIW               0x40
#define SSPI_SCM               0x80
#define SSPI_SCIDL             0x100
#define SSPI_SCDV6_0           0x3F
#define SSPI_STAT_MASK	       0x3
#define SSPI_BSY               0x01
#define SSPI_RBF               0x02


/* definitions for GCR Multi Function Chip Select register */
#define GCR_SSPISEL_ENABLE     	0x80000000

#define SSPI_DRV_MAX_RETRY    	5
#define SSPI_TRANSACTION_DONE 	0x0
#define SSPI_RECEIVED_DONE 		0x0

/* Others*/
#define SHIFT_9_BITS   9
#define SHIFT_7_BITS   7
#define NUMBER_1       1
#define NUMBER_3       3
#define NUMBER_4       4
#define NUMBER_5       5
#define NUMBER_7       7
#define NUMBER_8       8
#define NUMBER_F       0xF


#ifdef CONFIG_MACH_NPCM750
/* SSPI chip selects */
#define SSPI1_CHIP_SELECT_PIN   203
#define SSPI2_CHIP_SELECT_PIN   20
#elif defined (NPCM650)
/* SSPI chip selects */
#define SSPI1_CHIP_SELECT_PIN   131
#define SSPI2_CHIP_SELECT_PIN   20
#endif

extern void npcmx50_gcr_mux_pspi(UINT devNum);


/* driver name will passed by insmod command */
static char *driver_name="aess_sspidrv";

/* record the kernel spi initial state */
static int S_sspi_init_state = 0;
static UINT8 devNum = PSPI1_DEV;
#define NPCM650_VERSION_NUM       0x0926650


dev_t sspi_dev;
struct cdev *sspi_cdev;

struct semaphore sspi_sem;

#define SSPI_DRV_MAX_MSG_SIZE             512
static spinlock_t lock;

/******************************************************************************
*   STRUCT      :   sSPIDrvInfoType
******************************************************************************/
/**
 *  @brief   structure of SPI driver information
 *
 *****************************************************************************/
typedef struct
{

	/** Wait queue */
	wait_queue_head_t wq;

	/** Msg transmit buffer */
	UINT8 pu8MsgSendBuffer[SSPI_DRV_MAX_MSG_SIZE];

	/** Msg transmit data length */
	UINT32 u32MsgSendDataSize;

	/** Msg receive buffer */
	UINT8 pu8MsgRecBuffer[SSPI_DRV_MAX_MSG_SIZE];

	/** Msg receive data length */
	UINT32 u32MsgRecDataSize;

	/** Current start tracking count */
	UINT16 u16CurIndex;

	/** Condition checking for wait queue */
	UINT8 wq_flag;

	/** write or read process, 0:write 1:read */
	UINT8 wr_flag;

} sSSPIDrvBufType;

sSSPIDrvBufType sspi_buffer;

#ifdef CONFIG_MACH_NPCM750
irqreturn_t aess_sspi_isr_handler(UINT8 u8BusNumber)
{
    UINT8 u8Status = 0;
    UINT16 u16Reading;
    irqreturn_t irqreturn = IRQ_NONE;
	
	spin_lock(&lock);

	u8Status = REG_READ(PSPI_STAT(u8BusNumber)) & SSPI_STAT_MASK;
	DEBUGP("KN: aess_sspi_isr  u8Status = 0x%x \n", u8Status);

	switch(sspi_buffer.wr_flag)
	{
		case 0: //write process
			if (u8Status & SSPI_RBF)
			{
				//read data register for clear interrupt
				/* read a dummy data */
				u16Reading = REG_READ(PSPI_DATA(u8BusNumber));
				DEBUGP("KN: read data = 0x%x \n", u16Reading);
			}

			if ((u8Status & SSPI_BSY) == 0)
			{
				if (sspi_buffer.u16CurIndex < sspi_buffer.u32MsgSendDataSize)
				{
				/* write to SSPI */
					DEBUGP("KN: write data%d = 0x%x \n", sspi_buffer.u16CurIndex, sspi_buffer.pu8MsgSendBuffer[sspi_buffer.u16CurIndex]);
					REG_WRITE(PSPI_DATA(u8BusNumber), (UINT16) sspi_buffer.pu8MsgSendBuffer[sspi_buffer.u16CurIndex]);
					sspi_buffer.u16CurIndex++;
				}
				else
				{
					/* nothing to do in write process */
					if (sspi_buffer.u16CurIndex == 0){
						DEBUGP("KN: do nothing in write processing \n");
						spin_unlock(&lock);
						return IRQ_NONE;
					}

					/* init buf index */
					sspi_buffer.u16CurIndex = 0;

					/* change to read process */
					if (sspi_buffer.u32MsgRecDataSize > 0)
					{
						/* change to read process */
						sspi_buffer.wr_flag = 1;
					}
					else
					{
						/* complete write read command, and disable SSPI */
						REG_WRITE(PSPI_CTL1(u8BusNumber), (UINT16) 0x0);
						DEBUGP("KN: write process disable sspi \n");

						/* set wq_flag to wake up thread */
						sspi_buffer.wq_flag = 1;
						/* wake up */
						wake_up_interruptible(&sspi_buffer.wq);
						/* since interrupt will coming after disable read/write interrupt,
						 so, change wr_flag to 2 and don't do anyting when come back this isr call back function*/
						sspi_buffer.wr_flag = 2;
						break;
					}
				}
			}
			break;
		case 1: //read process
			if (u8Status & SSPI_RBF)
			{
				if (sspi_buffer.u16CurIndex < sspi_buffer.u32MsgRecDataSize)
				{
					/* read data from the register */
					u16Reading = REG_READ(PSPI_DATA(u8BusNumber));
					DEBUGP("KN: read data%d 0x%x \n", sspi_buffer.u16CurIndex, u16Reading);
					sspi_buffer.pu8MsgRecBuffer[sspi_buffer.u16CurIndex] = (UINT8)u16Reading;
					sspi_buffer.u16CurIndex++;
				}
				if (sspi_buffer.u16CurIndex == sspi_buffer.u32MsgRecDataSize)
				{
					/* nothing to do in read process */
					if (sspi_buffer.u16CurIndex == 0){
						DEBUGP("KN: do nothing in read processing \n");
						spin_unlock(&lock);
						return IRQ_NONE;
					}

					/* complete write read command, and disable SSPI */
					REG_WRITE(PSPI_CTL1(u8BusNumber), (UINT16) 0x0);
					DEBUGP("KN: read process disable sspi \n");

					/* set wq_flag to wake up thread */
					sspi_buffer.wq_flag = 1;
					/* wake up */
					wake_up_interruptible(&sspi_buffer.wq);

					/* since interrupt will coming after disable read/write interrupt,
					   so, change wr_flag to 2 and don't do anyting when come back this isr call back function*/
					sspi_buffer.wr_flag = 2;
					break;
				}
			}
			if ((u8Status & SSPI_BSY) == 0)
			{
				//write data register for clear interrupt
				/* write dummy data to the register */
				DEBUGP("KN: write data 0x0 \n");
				REG_WRITE(PSPI_DATA(u8BusNumber), (UINT16) 0x0);
			}
			break;
		default:
			printk(KERN_ERR "KN: default isr wr_flag = %d \n", sspi_buffer.wr_flag);
			break;
	 }

     irqreturn = IRQ_HANDLED;
 	spin_unlock(&lock);

	 return irqreturn;
}
static irqreturn_t aess_sspi1_isr (int irq, void *dev_id)
{
   irqreturn_t irqreturn = IRQ_NONE;

   irqreturn = aess_sspi_isr_handler(PSPI1_DEV);

   return irqreturn;
}

static irqreturn_t aess_sspi2_isr (int irq, void *dev_id)
{
   irqreturn_t irqreturn = IRQ_NONE;

   irqreturn = aess_sspi_isr_handler(PSPI2_DEV);

   return irqreturn;
}

#elif defined (NPCM650)

static irqreturn_t aess_sspi_isr (int irq, void *dev_id)
{
   UINT8 u8Status = 0;
   UINT16 u16Reading;
   irqreturn_t irqreturn = IRQ_NONE;

  if(READ_REG_BIT(AIC_GRSR, PSPI_GROUP_INTERRUPT(devNum)))
  {
	u8Status = REG_READ(PSPI_STAT(devNum)) & SSPI_STAT_MASK;
	DEBUGP("KN: aess_sspi_isr  u8Status = 0x%x \n", u8Status);

	switch(sspi_buffer.wr_flag)
	{
		case 0: //write process
			if (u8Status & SSPI_RBF)
			{
				//read data register for clear interrupt
				/* read a dummy data */
				u16Reading = REG_READ(PSPI_DATA(devNum));
				DEBUGP("KN: read data = 0x%x \n", u16Reading);
			}

			if ((u8Status & SSPI_BSY) == 0)
			{
				if (sspi_buffer.u16CurIndex < sspi_buffer.u32MsgSendDataSize)
				{
				/* write to SSPI */
					DEBUGP("KN: write data%d = 0x%x \n", sspi_buffer.u16CurIndex, sspi_buffer.pu8MsgSendBuffer[sspi_buffer.u16CurIndex]);
					REG_WRITE(PSPI_DATA(devNum), (UINT16) sspi_buffer.pu8MsgSendBuffer[sspi_buffer.u16CurIndex]);
					sspi_buffer.u16CurIndex++;
				}
				else
				{
					/* nothing to do in write process */
					if (sspi_buffer.u16CurIndex == 0){
						DEBUGP("KN: do nothing in write processing \n");
						return IRQ_NONE;
					}

					/* init buf index */
					sspi_buffer.u16CurIndex = 0;

					/* change to read process */
					if (sspi_buffer.u32MsgRecDataSize > 0)
					{
						/* change to read process */
						sspi_buffer.wr_flag = 1;
					}
					else
					{
						/* complete write read command, and disable SSPI */
						REG_WRITE(PSPI_CTL1(devNum), (UINT16) 0x0);
						DEBUGP("KN: write process disable sspi \n");

						/* set wq_flag to wake up thread */
						sspi_buffer.wq_flag = 1;
						/* wake up */
						wake_up_interruptible(&sspi_buffer.wq);
						/* since interrupt will coming after disable read/write interrupt,
						 so, change wr_flag to 2 and don't do anyting when come back this isr call back function*/
						sspi_buffer.wr_flag = 2;
						break;
					}
				}
			}
			break;
		case 1: //read process
			if (u8Status & SSPI_RBF)
			{
				if (sspi_buffer.u16CurIndex < sspi_buffer.u32MsgRecDataSize)
				{
					/* read data from the register */
					u16Reading = REG_READ(PSPI_DATA(devNum));
					DEBUGP("KN: read data%d 0x%x \n", sspi_buffer.u16CurIndex, u16Reading);
					sspi_buffer.pu8MsgRecBuffer[sspi_buffer.u16CurIndex] = (UINT8)u16Reading;
					sspi_buffer.u16CurIndex++;
				}
				if (sspi_buffer.u16CurIndex == sspi_buffer.u32MsgRecDataSize)
				{
					/* nothing to do in read process */
					if (sspi_buffer.u16CurIndex == 0){
						DEBUGP("KN: do nothing in read processing \n");
						return IRQ_NONE;
					}

					/* complete write read command, and disable SSPI */
					REG_WRITE(PSPI_CTL1(devNum), (UINT16) 0x0);
					DEBUGP("KN: read process disable sspi \n");

					/* set wq_flag to wake up thread */
					sspi_buffer.wq_flag = 1;
					/* wake up */
					wake_up_interruptible(&sspi_buffer.wq);

					/* since interrupt will coming after disable read/write interrupt,
					   so, change wr_flag to 2 and don't do anyting when come back this isr call back function*/
					sspi_buffer.wr_flag = 2;
					break;
				}
			}
			if ((u8Status & SSPI_BSY) == 0)
			{
				//write data register for clear interrupt
				/* write dummy data to the register */
				DEBUGP("KN: write data 0x0 \n");
				REG_WRITE(PSPI_DATA(devNum), (UINT16) 0x0);
			}
			break;
		default:
			printk(KERN_ERR "KN: default isr wr_flag = %d \n", sspi_buffer.wr_flag);
			break;
	 }

	 irqreturn = IRQ_HANDLED;
  }
  return irqreturn;
}
#endif

static int aess_sspi_wr(sSSPIDrvInfoType *psSSPIDrvInfo)
{
	unsigned long flags;
	unsigned char u8TmpBuf;
	int     i32Result = 0;
// Trego - Need to fix clock function
//    UINT32 apbClock = CLK_GetAPBFreq(5);
	UINT32 apbClock = ((UINT32) 55000000);
	INT32 divisor  = 0;
	spin_lock_irqsave(&lock,   flags);

	devNum = psSSPIDrvInfo->u8ChipId;

	DEBUGP("KN: aess_sspi_wr start APB Clock = 0x%x\n ",apbClock);

	DEBUGP("KN: u8ProcessTime=%d u8Mode=%d u8ChipId=%d u32Freq=%d\n",
		   psSSPIDrvInfo->u8ProcessTime, psSSPIDrvInfo->u8Mode,
		   psSSPIDrvInfo->u8ChipId, psSSPIDrvInfo->u32Freq);

	/* initiate local data struture */

	/* initiate wait queue */
	init_waitqueue_head(&sspi_buffer.wq);
	/* initiate buffer index */
	sspi_buffer.u16CurIndex = 0;
	/* write process firest */
	sspi_buffer.wr_flag = 0;
	/* clear the flag of condition checking */
	sspi_buffer.wq_flag = 0;

	if (copy_from_user ((void *) &sspi_buffer.pu8MsgSendBuffer[0],
						(void *) psSSPIDrvInfo->pu8MsgSendBuffer,
						psSSPIDrvInfo->u32MsgSendDataSize))
	{
		/* release the chip select */
		DEBUGP("KN: release the chip select \n");
		u8TmpBuf = SET_GPIO_OUTPUT_HIGH;
		if(devNum == PSPI1_DEV)
		{
			aess_gpio_commander(SSPI1_CHIP_SELECT_PIN, GPIO_WRITE, NORMAL_OUTPUT, (void *)&u8TmpBuf);
		}
		else
		{
			aess_gpio_commander(SSPI2_CHIP_SELECT_PIN, GPIO_WRITE, NORMAL_OUTPUT, (void *)&u8TmpBuf);
		}
		/* disable SSPI interface */
		DEBUGP("KN: Error disable SSPI interface \n");
		SET_REG_FIELD(PSPI_CTL1(devNum), PSPI_CTL1_SPIEN, 0);

		return -EFAULT;
	}

	sspi_buffer.u32MsgSendDataSize = psSSPIDrvInfo->u32MsgSendDataSize;
	sspi_buffer.u32MsgRecDataSize = psSSPIDrvInfo->u32MsgRecDataSize;

	/* setting Clock divider */
	divisor = (apbClock / (2* psSSPIDrvInfo->u32Freq)) - 1;
	SET_REG_FIELD(PSPI_CTL1(devNum), PSPI_CTL1_SCDV6_0, divisor);

	switch (psSSPIDrvInfo->u8Mode)
	{
		case SSPI_DRV_MODE_0:
		 /* setting Clock phase */
		 SET_REG_FIELD(PSPI_CTL1(devNum), PSPI_CTL1_SCIDL, 0);
		 /* setting Clock pol */
		 SET_REG_FIELD(PSPI_CTL1(devNum), PSPI_CTL1_SCM, 0);
		   break;

		case SSPI_DRV_MODE_1:
		 /* setting Clock phase */
		 SET_REG_FIELD(PSPI_CTL1(devNum), PSPI_CTL1_SCIDL, 1);
		 /* setting Clock pol */
		 SET_REG_FIELD(PSPI_CTL1(devNum), PSPI_CTL1_SCM, 0);
		   break;

		case SSPI_DRV_MODE_2:
		 /* setting Clock phase */
		 SET_REG_FIELD(PSPI_CTL1(devNum), PSPI_CTL1_SCIDL,0);
		 /* setting Clock pol */
		 SET_REG_FIELD(PSPI_CTL1(devNum), PSPI_CTL1_SCM, 1);
		   break;

		case SSPI_DRV_MODE_3:
		 /* setting Clock phase */
		 SET_REG_FIELD(PSPI_CTL1(devNum), PSPI_CTL1_SCIDL, 1);
		 /* setting Clock pol */
		 SET_REG_FIELD(PSPI_CTL1(devNum), PSPI_CTL1_SCM, 1);
		   break;

		default:
			printk(KERN_ERR "KN: default u8Mode = %d \n",psSSPIDrvInfo->u8Mode );
			i32Result = -EFAULT;
			  break;
	}

	/* set the speed and chip select */
	u8TmpBuf = SET_GPIO_OUTPUT_LOW;
	if(devNum == PSPI1_DEV)
	{
	   aess_gpio_commander(SSPI1_CHIP_SELECT_PIN, GPIO_WRITE, NORMAL_OUTPUT, (void *)&u8TmpBuf);
	   DEBUGP("KN: GPIO: Module-1 port-0 data = 0x%x \n", ioread32(SSPI_GPIO_REG_MODULE1_PORT0_DATAOUT_ADDR));
	}
	else
	{
	   aess_gpio_commander(SSPI2_CHIP_SELECT_PIN, GPIO_WRITE, NORMAL_OUTPUT, (void *)&u8TmpBuf);
	   DEBUGP("KN: GPIO: Module-0 port-0 data = 0x%x \n", ioread32(SSPI_GPIO_REG_MODULE0_PORT1_DATAOUT_ADDR));
	}

	/* enable SSPI interface */
	SET_REG_FIELD(PSPI_CTL1(devNum), PSPI_CTL1_SPIEN, 1);
	DEBUGP("KN: PSPI_CTL1 = 0x%x \n", REG_READ(PSPI_CTL1(devNum)));
	DEBUGP("KN: Enable SSPI \n");

	/* enable SSPI interrupt sources */
	SET_REG_FIELD(PSPI_CTL1(devNum), PSPI_CTL1_EIR, 1);
	SET_REG_FIELD(PSPI_CTL1(devNum), PSPI_CTL1_EIW, 1);

	/* push task go to sleep and wait for SSPI transaction completed */
	DEBUGP("KN: wait_event_interruptible_timeout \n");
	spin_unlock_irqrestore(&lock,   flags);
	if (0 == wait_event_interruptible_timeout(sspi_buffer.wq,
											  sspi_buffer.wq_flag == 1,
											  psSSPIDrvInfo->u8ProcessTime))
	{
		printk(KERN_ERR "KN: timeout for write read processing \n");
		spin_lock_irqsave(&lock,   flags);
		sspi_buffer.wr_flag = 2;
		i32Result =  -EBUSY;
	}
	else
	{
		spin_lock_irqsave(&lock,   flags);
		if ((psSSPIDrvInfo->u32MsgRecDataSize > 0) && (sspi_buffer.wq_flag == 1))
		{
			DEBUGP("KN: copy data to user space, len = %d\n", psSSPIDrvInfo->u32MsgRecDataSize);
    		if (copy_to_user((void __user *)psSSPIDrvInfo->pu8MsgRecBuffer, &sspi_buffer.pu8MsgRecBuffer, sizeof(sspi_buffer.pu8MsgRecBuffer)*psSSPIDrvInfo->u32MsgRecDataSize))
        	{
        		PERROR("copy_to_user error!!\n");
        	    i32Result = -EFAULT;
        	}
		}
	}

	/* release the chip select */
	DEBUGP("KN: release the chip select \n");
	u8TmpBuf = SET_GPIO_OUTPUT_HIGH;
	if(devNum == PSPI1_DEV)
	{
	   aess_gpio_commander(SSPI1_CHIP_SELECT_PIN, GPIO_WRITE, NORMAL_OUTPUT, (void *)&u8TmpBuf);
	   DEBUGP("KN: GPIO: Module-1 port-0 data = 0x%x \n", ioread32(SSPI_GPIO_REG_MODULE1_PORT0_DATAOUT_ADDR));
	}
	else
	{
	   aess_gpio_commander(SSPI2_CHIP_SELECT_PIN, GPIO_WRITE, NORMAL_OUTPUT, (void *)&u8TmpBuf);
	   DEBUGP("KN: GPIO: Module-0 port-0 data = 0x%x \n", ioread32(SSPI_GPIO_REG_MODULE0_PORT1_DATAOUT_ADDR));
	}

	/* disable SSPI interface */
	DEBUGP("KN: disable SSPI interface \n");
	REG_WRITE(PSPI_CTL1(devNum), (UINT16) 0x0);

	/* clear the flag of condition checking */
	sspi_buffer.wq_flag = 0;

	spin_unlock_irqrestore(&lock,   flags);

	DEBUGP("KN: aess_sspi_wr end \n");

	return i32Result;
}


static long aess_sspi_ioctl(struct file *flip, unsigned int cmd, unsigned long arg)
{
	unsigned long flags;
	int result;

	sSSPIDrvInfoType SSPIDrvInfoType;
    sSSPIDrvInfoType *psSSPIDrvInfo = &SSPIDrvInfoType;

	DEBUGP("KN: aess_spi_ioctl\n");
	spin_lock_irqsave(&lock,   flags);

	switch (cmd)
	{
		case AESS_SSPIDRV_WR:

			DEBUGP("KN: AESS_SPIDRV_WR\n");

            if (copy_from_user(&SSPIDrvInfoType, (void __user *) arg, sizeof(SSPIDrvInfoType)))
            {
            	PERROR("copy_from_user error!!\n");
				spin_unlock_irqrestore(&lock,   flags);
                return -EFAULT;
            }
			/* get semaphore */
			if(down_interruptible(&sspi_sem))
			{
				printk(KERN_ERR "KN: bus busy!\n");
				/* device busy */
				spin_unlock_irqrestore(&lock,   flags);
				return -EBUSY;
			}
			/* invoke the handling function */
			spin_unlock_irqrestore(&lock,   flags);
			result = aess_sspi_wr(psSSPIDrvInfo);
			spin_lock_irqsave(&lock,   flags);
			/* release semaphore */
			up(&sspi_sem);
			  break;

		default:
			printk(KERN_ERR "KN: default ioctl cmd = %d \n", cmd);
			result = -ENOIOCTLCMD;
			  break;
	}
	
	spin_unlock_irqrestore(&lock,   flags);
	return result;
}


static int aess_sspi_release(struct inode *inode, struct file *filp)
{
	DEBUGP("KN:aess_sspi_release\n");
	return 0;
}


static int aess_sspi_open(struct inode *inode, struct file *filp)
{
	unsigned long flags;

	DEBUGP("KN:aess_sspi_open\n");
	spin_lock_irqsave(&lock,   flags);

	/* if spi interface is not initiated */
	if (!S_sspi_init_state)
	{
        /* initiate semaphores */
        sema_init(&sspi_sem, 1);
        /* spi interface is initiated */
        S_sspi_init_state = 1;	  
    }

	spin_unlock_irqrestore(&lock,   flags);
	return 0;
}


struct file_operations aess_sspi_fops = {
	.owner = THIS_MODULE,
	.open = aess_sspi_open,
	.release = aess_sspi_release,
	.unlocked_ioctl = aess_sspi_ioctl,
};


int __init aess_sspi_init(void)
{
	int result;
	int i;
	unsigned long flags = 0;
	// void __iomem *npcmx50_gcr_reg_base_virtual_address;
	
#ifdef CONFIG_OF
		struct device_node *np=NULL;
		struct resource res;
#endif

	DEBUGP("KN:init aess_sspi_module\n");
	if (ioread32((void*)SSPI_GCR_PDID_REG) < NPCM650_VERSION_NUM)
	{
		printk(KERN_ERR "KN:This version 0x%x don't support SSPI\n", ioread32((void*)SSPI_GCR_PDID_REG));
		return 0;
	}
	/* allocate a char device */
	sspi_cdev = cdev_alloc();
	sspi_cdev->owner = THIS_MODULE;
	sspi_cdev->ops = &aess_sspi_fops;

	if (majornum <= 0)
	{
		/* allocate a device number */
		result = alloc_chrdev_region(&sspi_dev, 0, 1, driver_name);
		DEBUGP("Major number %d\n", MAJOR(sspi_dev));
		majornum = MAJOR(sspi_dev);
	}
	else
	{
		sspi_dev = MKDEV(majornum, 0);
		result = register_chrdev_region(sspi_dev, 1, driver_name);
	}

	if (result)
	{
		printk(KERN_ERR "KN: Registering the character device failed with %d\n", MAJOR(sspi_dev));
		return result;
	}

	/* add a char device */
	cdev_add(sspi_cdev, sspi_dev, 1);
	printk("mknod /dev/aess_sspidrv c %d 0\n", MAJOR(sspi_dev));


#ifdef CONFIG_OF
		np = of_find_compatible_node(NULL, NULL, "nuvoton,npcm750-pspi");
		if (np==NULL){
				pr_info("Failed to find of_find_matching_node\n");
				}
		result = of_address_to_resource(np, 0, &res);
		if (result) {
			pr_info("\t\t\t of_address_to_resource fail ret %d \n",result);
			return -EINVAL;
		}
			
		pspi_virt_addr = (int)ioremap(res.start, resource_size(&res));
			
		if (!pspi_virt_addr) {
			pr_info("\t\t\t pspi_virt_addr fail \n");
			return -ENOMEM;
		}
		printk(KERN_INFO "\t\t\t* PSPI base is 0x%08X ,res.start 0x%08X \n", (u32)pspi_virt_addr,res.start);
#endif

	/*CLK_ResetPSPI(PSPI1_DEV);
	CLK_ResetPSPI(PSPI2_DEV);*/

	npcmx50_spin_lock_irqsave(&flags);
	SET_REG_FIELD(IPSRST2, IPSRST2_PSPI1, 1);
	SET_REG_FIELD(IPSRST2, IPSRST2_PSPI1, 0);
	SET_REG_FIELD(IPSRST2, IPSRST2_PSPI2, 1);
	SET_REG_FIELD(IPSRST2, IPSRST2_PSPI2, 0);
	npcmx50_spin_unlock_irqrestore(flags);

	/* disable SSPI before request irq */
	SET_REG_FIELD(PSPI_CTL1(PSPI1_DEV), PSPI_CTL1_SPIEN, 0);
	SET_REG_FIELD(PSPI_CTL1(PSPI2_DEV), PSPI_CTL1_SPIEN, 0);

	DEBUGP("KN: PSPI1_DEV PSPI_CTL1 = 0x%x \n", REG_READ(PSPI_CTL1(PSPI1_DEV)));
	DEBUGP("KN: PSPI2_DEV PSPI_CTL1 = 0x%x \n", REG_READ(PSPI_CTL1(PSPI2_DEV)));

	spin_lock_init(&lock);

#ifdef CONFIG_MACH_NPCM750
#ifdef CONFIG_OF
	for (i = 0; i < 2; i++) {
		pspi_irq[i] = irq_of_parse_and_map(np, i);
		if (!pspi_irq[i]) {
			printk(KERN_ERR "%s - failed to map irq %d\n", __FUNCTION__, i);
			return -EIO;
		}
	}
#endif
	if (request_irq(PSPI1_INTERRUPT, aess_sspi1_isr, 0, "sspi-1", (void *)sspi_dev))
    {
        DEBUGP("KN: request_irq PSPI1_INTERRUPT failed ! \n");
		return -EBUSY;
	}
	/*if (request_irq(PSPI2_INTERRUPT, aess_sspi2_isr, IRQF_SAMPLE_RANDOM, "sspi-2", sspi_dev))
	T.M. modify 12/11/2015 */

	if (request_irq(PSPI2_INTERRUPT, aess_sspi2_isr, 0, "sspi-2", (void *)sspi_dev))
    {
        DEBUGP("KN: request_irq PSPI2_INTERRUPT failed ! \n");
		return -EBUSY;
	}
#elif defined (NPCM650)
	/*if (request_irq(INT31_GRP_INT, aess_sspi_isr, IRQF_SHARED | IRQF_SAMPLE_RANDOM, "SSPI", &S_sspi_init_state))
	 T.M. modify 12/11/2015 */
	if (request_irq(INT31_GRP_INT, aess_sspi_isr, IRQF_SHARED , "SSPI", &S_sspi_init_state))
	{
		DEBUGP("KN: request_irq INT31_GRP_INT failed ! \n");
		return -EBUSY;
	}
#endif


    npcmx50_spin_lock_irqsave(&flags);
    SET_REG_FIELD(MFSEL3, MFSEL3_PSPI1SEL, 0x2);
    SET_REG_FIELD(MFSEL3, MFSEL3_PSPI2SEL, 0x1);
    npcmx50_spin_unlock_irqrestore(flags);

#ifdef EXAMPLE_SINGLE_REG_IN_ONE_ATOMIC_COMMAND  /* this can replace the 4 lines above */
    npcmx50_atomic_io_modify((void __iomem *)REG_ADDR(MFSEL3), BUILD_FIELD_VAL(MFSEL3_PSPI1SEL, 0xFFFFFFFF), BUILD_FIELD_VAL(MFSEL3_PSPI1SEL, 0x2) );
    npcmx50_atomic_io_modify((void __iomem *)REG_ADDR(MFSEL3), BUILD_FIELD_VAL(MFSEL3_PSPI2SEL, 0xFFFFFFFF), BUILD_FIELD_VAL(MFSEL3_PSPI1SEL, 0x1) );
#endif


#ifdef CONFIG_MACH_NPCM750
#elif defined (NPCM650)
	AIC_EnableGroupInt(PSPI_GROUP_INTERRUPT(PSPI1_DEV));
	AIC_EnableGroupInt(PSPI_GROUP_INTERRUPT(PSPI2_DEV));
#endif
	return 0;
}


void __exit aess_sspi_exit(void)
{
	DEBUGP("exit aess_sspi_module\n");

	/* release the char device */
	cdev_del(sspi_cdev);

	/* release the device number */
	unregister_chrdev_region(sspi_dev, 1);
#ifdef CONFIG_MACH_NPCM750
	free_irq(PSPI1_INTERRUPT, (void *) NULL);
	free_irq(PSPI2_INTERRUPT, (void *) NULL);
#elif defined (NPCM650)
	free_irq(INT31_GRP_INT, (void *) NULL);
#endif
	return;
}

MODULE_DESCRIPTION("AESS SSPI Driver");
MODULE_AUTHOR("Justin Lee <justin.lee@emersion.com>");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("0.0.0.1");
module_param(driver_name, charp, S_IRUGO);
module_init(aess_sspi_init);
module_exit(aess_sspi_exit);
