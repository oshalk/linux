/*
 * $RCSfile$
 * $Revision$
 * $Date$
 * $Author$
 *
 * VSC 452 KCS driver.
 *   
 * Copyright (C) 2006 Avocent Corp.
 * 
 * This file is subject to the terms and conditions of the GNU 
 * General Public License. This program is distributed in the hope 
 * that it will be useful, but WITHOUT ANY WARRANTY; without even 
 * the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
 * PURPOSE.  See the GNU General Public License for more details.
 */  

#define AESSKCSDRV_C
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/bitops.h>
#include <linux/delay.h>

#include <mach/hal.h>

#include "aess_kcsdrv.h"
//#include "../eventhandler/aess_eventhandlerdrv.h"

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>

void *kcs_virt_addr;
#undef LPC_REG_BASE_ADDR
#define LPC_REG_BASE_ADDR		(kcs_virt_addr)
#undef KCS_HIB_INT
#define KCS_HIB_INT kcs_irq
int kcs_irq;

static const struct of_device_id kcs_dt_npcm750_match[] = {
	{ .compatible = "nuvoton,npcm750-kcs" },
	{ /*sentinel*/ },
};
#endif

#define KCS_DEBUG 0

/* Debugging Message */
#ifdef KCS_DEBUG
#define DEBUG_MSG(fmt, args...)  printk("KCS: %s() " fmt, __func__ , ##args)
#else
#define DEBUG_MSG(fmt, args...)
#endif
#define PERROR(fmt, args...)	 printk("KCS: %s(): " fmt, __func__, ##args)
#define     COMPCODE_NODE_BUSY                  0xC0 

static int majornum = 0;           /* default to dynamic major */
module_param(majornum, int, 0);
MODULE_PARM_DESC(majornum, "Major device number");

/* driver name will passed by insmod command */
static char *driver_name="aess_kcsdrv";
static u8 kcs_device_open = 0;

static spinlock_t lock;

#define SUPPORT_SMI 1
/* Linux device data structure */
dev_t kcs_dev;
DECLARE_TASKLET(lpcreset_tasklet, aess_kcsdrv_bh, 0);

struct cdev *kcs_cdev;
#ifdef KCS_DEBUG
static void aess_kcs_showregisters(void)
{
	printk("KCS: 0Ch: %x\n",*((UINT8 *)LPC_REG_BASE_ADDR+0x0C));
	printk("KCS: 0Eh: %x\n",*((UINT8 *)LPC_REG_BASE_ADDR+0x0E));
	printk("KCS: 10h: %x\n",*((UINT8 *)LPC_REG_BASE_ADDR+0x10));
	printk("KCS: 12h: %x\n",*((UINT8 *)LPC_REG_BASE_ADDR+0x12));
	printk("KCS: 14h: %x\n",*((UINT8 *)LPC_REG_BASE_ADDR+0x14));
	printk("KCS: 16h: %x\n",*((UINT8 *)LPC_REG_BASE_ADDR+0x16));
	printk("KCS: 18h: %x\n",*((UINT8 *)LPC_REG_BASE_ADDR+0x18));
	printk("KCS: 1Ah: %x\n",*((UINT8 *)LPC_REG_BASE_ADDR+0x1A));
	printk("KCS: 1Ch: %x\n",*((UINT8 *)LPC_REG_BASE_ADDR+0x1C));
  printk("BIOS:42h: %x\n",*((UINT8 *)LPC_REG_BASE_ADDR+0x42));
  printk("BIOS:44h: %x\n",*((UINT8 *)LPC_REG_BASE_ADDR+0x44));
  printk("BIOS:46h: %x\n",*((UINT8 *)LPC_REG_BASE_ADDR+0x46));
  printk("BIOS:48h: %x\n",*((UINT8 *)LPC_REG_BASE_ADDR+0x48));
  printk("BIOS:4Ah: %x\n",*((UINT8 *)LPC_REG_BASE_ADDR+0x4A));
  printk("BIOS:4Ch: %x\n",*((UINT8 *)LPC_REG_BASE_ADDR+0x4C));
	
}
#endif 
static int aess_kcs_open(struct inode *inode, struct file *filp)
{ 
	unsigned long flags;
    DEBUG_MSG("aess_kcs_open!\n");
	spin_lock_irqsave(&lock,   flags);

    /* Set the initial value for data structure */    
    //move to aess_kcs_init()  memset(S_aKCSData, 0, sizeof(sKCSStruct) * KCS_TOTAL_CHANNEL);
  
    kcs_device_open = 1;
	spin_unlock_irqrestore(&lock,   flags);
    return (STATUS_OK);  
} 
  
  
static int aess_kcs_close(struct inode* inode, struct file *flip)
{ 
	unsigned long flags;
  DEBUG_MSG("aess_kcs_close exit!\n");
  
	spin_lock_irqsave(&lock,   flags);
	/* disable all IBF interrupts */
	aess_kcs_setreg(KCS_CH_0, KCS_REG_IBF, DISABLE_IBF_INT);
	aess_kcs_setreg(KCS_CH_1, KCS_REG_IBF, DISABLE_IBF_INT);
	aess_kcs_setreg(KCS_CH_2, KCS_REG_IBF, DISABLE_IBF_INT);
  
  /* Disable LPC reset interrupt*/
 	aess_kcs_setreg(KCS_CH_NONE, BIOS_REG_FIFO_INT, LPC_RST_INT_DISABLE);
    kcs_device_open = 0;
	spin_unlock_irqrestore(&lock,   flags);

  return 0;
}


static long aess_kcs_ioctl(struct file *flip, unsigned int cmd, unsigned long arg)
{   
	int err_check;
	unsigned long flags;
	sKCSInfo KCSInfo;
	sKCSInfo *psInfo = &KCSInfo;

    DEBUG_MSG("aess_kcs_ioctl!\n");
	/* Check the structure address is OK */
	spin_lock_irqsave(&lock,   flags);

	if (copy_from_user(&KCSInfo, (void __user *) arg, sizeof(KCSInfo)))
	{
		PERROR("copy_from_user error!!\n");
	    err_check = -EFAULT;
        goto aess_kcs_ioctl_exit;
	}
 
	switch(cmd)
	{
		case AESS_KCSDRV_INIT:

			/* Check the buffer address exist in the structure is OK */
			err_check = aess_kcs_channel_init(psInfo);         
			break;
					
		case AESS_KCSDRV_READ:
            {
                UINT8 Data[KCS_PACKAGE_MAX];
                UINT8 *Data_orig = psInfo->pu8Data;
                psInfo->pu8Data = Data;
                
    			err_check = aess_kcs_read(psInfo);
                       
                if (err_check == 0) {
            		if (copy_to_user((void __user *)Data_orig, &Data, sizeof(Data)))
                	{
                		PERROR("copy_to_user error!!\n");
                	    err_check = -EFAULT;
                        goto aess_kcs_ioctl_exit;
                	}
                }
                psInfo->pu8Data = Data_orig; 
    		}
			break;
			   
		case AESS_KCSDRV_WRITE:
            {
                UINT8 Data[KCS_PACKAGE_MAX];
                UINT8 *Data_orig = psInfo->pu8Data;
                psInfo->pu8Data = Data;
                
            	if (copy_from_user(&Data, (void __user *)Data_orig, sizeof(Data)))
            	{
            		PERROR("copy_from_user error!!\n");
            	    err_check = -EFAULT;
                    goto aess_kcs_ioctl_exit;
            	}
    			err_check = aess_kcs_write(psInfo);
                    
                psInfo->pu8Data = Data_orig;
    		}
			break;  
        case AESS_KCSDRV_SWSMI:
 #ifdef SUPPORT_SMI
            /*Pull down nSMI*/
            aess_kcs_setreg(psInfo->u8Channel, KCS_REG_IC, SET_SWIB);
            mdelay(10);

            /*Pull up nSMI*/
            aess_kcs_setreg(psInfo->u8Channel, KCS_REG_IC, CLEAR_SWIB);
 #endif
            err_check = STATUS_OK;
            break;    
        case AESS_KCSDRV_SETCBID:          
            S_sLPCResetCBID.u16CBFunDriverID = psInfo->u16CBFunDriverID;
            S_sLPCResetCBID.u32CBFunEventID = psInfo->u32CBFunEventID;
            err_check = STATUS_OK;
            break;    
		default:
			printk(KERN_ERR "KN:aess_kcs_ioctl, command error\n");
			err_check = -EINVAL;
	}

    if (err_check == 0) {
		if (copy_to_user((void __user *)arg, &KCSInfo, sizeof(KCSInfo)))
    	{
    		PERROR("copy_to_user error!!\n");
    	    err_check = -EFAULT;
    	}
    }
    
aess_kcs_ioctl_exit:	
	/* 0->ok, minus->fail */
	spin_unlock_irqrestore(&lock,   flags);
	return err_check;
}

static int aess_kcs_channel_init(sKCSInfo *psInfo)
{
 
    DEBUG_MSG("aess_kcs_channel_init!\n");
	/* Enable LPC reset interrupt*/
 	aess_kcs_setreg(KCS_CH_NONE, BIOS_REG_FIFO_INT, LPC_RST_INT_ENABLE);

	
	/* Check channel number */
	if (psInfo->u8Channel < KCS_TOTAL_CHANNEL)
	{
		/* Save Channel ID */
		S_aKCSData[psInfo->u8Channel].u8Channel = psInfo->u8Channel;        
		
		/* Save Event ID */
		S_aKCSData[psInfo->u8Channel].u16DriverID = psInfo->u16DriverID;
	
		S_aKCSData[psInfo->u8Channel].u32KCSRxOKEvent = psInfo->u32KCSRxOKEvent;
		
		S_aKCSData[psInfo->u8Channel].u32KCSTxOKEvent = psInfo->u32KCSTxOKEvent;
		
		S_aKCSData[psInfo->u8Channel].u32KCSTxFailEvent = psInfo->u32KCSTxFailEvent;
	
		/* Save KCS base address and offset */
		S_aKCSData[psInfo->u8Channel].u16AddrBase = psInfo->u16BaseAddress;
	
		/* set HIF state machine to idle state */
		S_aKCSData[psInfo->u8Channel].u8HIFISRState = KCS_IDLE_STATE;
	
		/* clear error code */
		S_aKCSData[psInfo->u8Channel].u8HIFStatusCode = KCS_NO_ERROR;
	
		/* KCS SeqNo init */
		S_aKCSData[psInfo->u8Channel].u8KCSSeqNo = 0;

		/* set initial state to Error state to mit the KCS spec */
		aess_kcs_setstate(psInfo->u8Channel, KCS_STATUS_ERROR_STATE);

 #ifdef SUPPORT_SMI 
    /*Set pin function to nSMI*/
        aess_kcs_setreg(KCS_CH_NONE, GC_REG_MFSEL_SMI, ENABLE_SMI_FUNCTION);
 #endif

		/* initial register of host interface */        
		switch(psInfo->u8Channel)
		{
			case KCS_CH_0:
             /* enable KCS CH0 IBF interrupt */
             aess_kcs_setreg(KCS_CH_0, KCS_REG_IBF, ENABLE_IBF_INT);
             /*Enable Software SMI and Disable Hardware SMI*/
             aess_kcs_setreg(KCS_CH_0, KCS_REG_IE, DISABLE_HWSMI);
             aess_kcs_setreg(KCS_CH_0, KCS_REG_IE, ENABLE_SWSMI);   
				break;

			case KCS_CH_1:
             /* enable KCS CH1 IBF interrupt */
             aess_kcs_setreg(KCS_CH_1, KCS_REG_IBF, ENABLE_IBF_INT);
             /*Enable Software SMI and Disable Hardware SMI*/
             aess_kcs_setreg(KCS_CH_1, KCS_REG_IE, DISABLE_HWSMI);
             aess_kcs_setreg(KCS_CH_1, KCS_REG_IE, ENABLE_SWSMI);
                break;

			case KCS_CH_2:
             /* enable KCS CH2 IBF interrupt */
	         aess_kcs_setreg(KCS_CH_2, KCS_REG_IBF, ENABLE_IBF_INT);

             /*Enable Software SMI and Disable Hardware SMI*/
             aess_kcs_setreg(KCS_CH_2, KCS_REG_IE, DISABLE_HWSMI);
             aess_kcs_setreg(KCS_CH_2, KCS_REG_IE, ENABLE_SWSMI);
				break;

			default:
				/* Won't happen */
				break;
		}
		return STATUS_OK;        
	} 
	/* u8Channel invalid */
	return -ECHRNG;
}

static int aess_kcs_read(sKCSInfo *psInfo)
{
	UINT8 u8Count; 
	UINT8 u8HIFDataSize;
	UINT8 u8TempSeqNum;
	unsigned long flags;

	DEBUG_MSG("aess_kcs_read!\n");


	if (psInfo->u8Channel < KCS_TOTAL_CHANNEL)
	{
		spin_lock_irqsave(&lock,   flags);
	    if (S_aKCSData[psInfo->u8Channel].u8HIFISRState == KCS_TRANSFER_DATA_STATE)
	    {
            /* started while we are copying */
            u8TempSeqNum = S_aKCSData[psInfo->u8Channel].u8KCSSeqNo;
            u8HIFDataSize = S_aKCSData[psInfo->u8Channel].u8HIFInDataSize;

            /* check length of KCS packet to make sure it is a valid message   
               before passing to upper layer for processing.                   
               Siliently discard if KCS packet length not valid.             */
		
            if ((u8HIFDataSize == 0) || (u8HIFDataSize > (KCS_PACKAGE_MAX + 1)))
            {
                /* invalid KCS packet length */
				spin_unlock_irqrestore(&lock,   flags);
                return -EMSGSIZE;
            }

            /* save incoming messages */
            for (u8Count = 0; u8Count < u8HIFDataSize; u8Count++)
            {
                psInfo->pu8Data[u8Count] = S_aKCSData[psInfo->u8Channel].au8HIFReceiveBuf[u8Count];
            }

            /* save length of incoming messages */
            *psInfo->pu8ReadLength = u8HIFDataSize;

            /* enable KCS_CH_1 interrupt
               SMSTPCR.BIT.B0 = 0;
             */

            /* Now compare the temp sequence number to the Global one to make 
               sure another Write Start command hasn't been issued on KCS 
               while we were copying this message.
               If a Write Start was issued, throw away this message and wait 
               for new one.                                                    */ 
            if (u8TempSeqNum == S_aKCSData[psInfo->u8Channel].u8KCSSeqNo)
            {
				spin_unlock_irqrestore(&lock,   flags);
                return (STATUS_OK);
            }
            else
            {
				spin_unlock_irqrestore(&lock,   flags);
                return (STATUS_FAIL);
            }
        }
        else
        {
             return (STATUS_FAIL);
        }
    }
    /* u8Channel invalid */
    return -ECHRNG;
}

static int aess_kcs_write(sKCSInfo *psInfo)
{
	UINT8 u8Count = 0;
	unsigned long flags;
	
	DEBUG_MSG("aess_kcs_write!\n");


	if (psInfo->u8Channel < KCS_TOTAL_CHANNEL)
	{
		spin_lock_irqsave(&lock,   flags);
		if (psInfo->u8Control == SMS_ATN_SET)
		{
			/* Set KCS interface system attention bit */
			aess_kcs_setreg(psInfo->u8Channel, KCS_REG_FOCTL, *psInfo->pu8Data);
		}
		else
		{

			if (S_aKCSData[psInfo->u8Channel].u8HIFISRState == KCS_TRANSFER_DATA_STATE)
			{
				/* Compare the sequence number, if it's the response of latest
				   KCS request, then remove the extra byte, if not, drop it */
				if ((S_aKCSData[psInfo->u8Channel].u8KCSSeqNo) != psInfo->pu8Data[psInfo->u8WriteLength - 1])
				{
					spin_unlock_irqrestore(&lock,   flags);
					return (STATUS_FAIL);
				}
				else
				{
					psInfo->u8WriteLength--;
				}

				/* Store output messages in local buffer */
				for (u8Count = 0; u8Count < psInfo->u8WriteLength; u8Count++)
				{
					S_aKCSData[psInfo->u8Channel].au8HIFTransferBuf[u8Count] = *psInfo->pu8Data++;
				}

				/* save pointer of output buffer */
				S_aKCSData[psInfo->u8Channel].pu8HIFOutDataPtr = S_aKCSData[psInfo->u8Channel].au8HIFTransferBuf;

				/* save length of output buffer */
				S_aKCSData[psInfo->u8Channel].u8HIFOutDataSize = psInfo->u8WriteLength;

				/* Send first byte */
				S_aKCSData[psInfo->u8Channel].u8HIFOutDataSize--;

				/* OBF (read), put one byte to data out register */
				aess_kcs_setreg(psInfo->u8Channel, KCS_REG_ODR, *S_aKCSData[psInfo->u8Channel].pu8HIFOutDataPtr++);
			}
			else
			{
				/* if KCS machine state is not in transfer data 
				   state then return STATUS_FAIL */
				spin_unlock_irqrestore(&lock,   flags);
				return (STATUS_FAIL);
			}
		}
		spin_unlock_irqrestore(&lock,   flags);
		return (STATUS_OK);
	}
	else
	{                  
		return (STATUS_FAIL);
	}  

}

static int aess_kcs_clearall()
{
	/* Set the initial value for data structure */
	memset(S_aKCSData, 0, sizeof(sKCSStruct) * KCS_TOTAL_CHANNEL);  

	/* Release IRQ */
	free_irq(KCS_HIB_INT, (void *) &S_aKCSData[0]);
	return (STATUS_OK);
}

static void aess_kcs_setstate(UINT8 u8Channel, UINT8 u8State)
{
	aess_kcs_setreg(u8Channel, KCS_REG_STATUS, u8State);
}

static void aess_kcs_errorhandler(UINT8 u8Channel)
{
    if (u8Channel >= KCS_TOTAL_CHANNEL)
    {
        printk(KERN_ERR "KCS: %s() u8Channel = %d >= KCS_TOTAL_CHANNEL = %d\n", __func__, u8Channel, KCS_TOTAL_CHANNEL);
        return;
    }
	/* set BMC state to ERROR state */
	aess_kcs_setstate(u8Channel, KCS_STATUS_ERROR_STATE);
	S_aKCSData[u8Channel].u8HIFISRState = KCS_ERROR_STATE;
	
	/* clear the IBF */
	aess_kcs_getreg(u8Channel , KCS_REG_IDR);
	
	/* KCS SeqNo increase*/
	S_aKCSData[u8Channel].u8KCSSeqNo++;    
}

static irqreturn_t aess_kcs_isr (int irq, void *dev_id)
{
 
   aess_kcs_showregisters();
   
  if (aess_kcs_getreg(KCS_CH_ALL, KCS_REG_INT_STS))
   {
		spin_lock(&lock);
       /* Process the interrupt for each channel */
       if (aess_kcs_getreg(KCS_CH_0, KCS_REG_IBF))
       {
            /* KCS 0 */
            aess_kcs_isr_handler(KCS_CH_0);
       }
		 
       if (aess_kcs_getreg(KCS_CH_1, KCS_REG_IBF))
       {
            /* KCS 1 */
            aess_kcs_isr_handler(KCS_CH_1);
       }
		
       if (aess_kcs_getreg(KCS_CH_2, KCS_REG_IBF))
       {
            /* KCS 2 */
            aess_kcs_isr_handler(KCS_CH_2);
       }

       if (aess_kcs_getreg(KCS_CH_NONE, BIOS_REG_FIFO_LPCRST_CHGSTS))
       {
            /*LPC Reset*/
            aess_lpc_rst_isr_handler();        
       }
   	 spin_unlock (&lock );
      return IRQ_HANDLED;
   }
   else
   {
      return IRQ_NONE;
   }  	 
}
static void aess_lpc_rst_isr_handler(void) 
{
    UINT8 i = 0;

    printk("LPC Reset Interrupt\n");		

    /*Clear LPC reset interrupt*/
    aess_kcs_setreg(KCS_CH_NONE, BIOS_REG_FIFO_LPCRST_CHGSTS, LPC_RST_CHG_STS_CLEAR);
    for (i=0;i< KCS_TOTAL_CHANNEL;i++)
    {
        /* set HIF state machine to idle state */
		    S_aKCSData[i].u8HIFISRState = KCS_IDLE_STATE;
	
		    /* clear error code */
		    S_aKCSData[i].u8HIFStatusCode = KCS_NO_ERROR;
	

		    /* KCS SeqNo init */
		    S_aKCSData[i].u8KCSSeqNo = 0;

		    /* set initial state to Error state to mit the KCS spec */
		    aess_kcs_setstate(i, KCS_STATUS_ERROR_STATE);
    }
    /* Schedule  aess_kcsdrv_bh function */        
	tasklet_schedule(&lpcreset_tasklet);
    #if 0
   
     /* enable KCS CH0~2 IBF interrupt */
	   aess_kcs_setreg(KCS_CH_0, KCS_REG_IBF, ENABLE_IBF_INT);
	   aess_kcs_setreg(KCS_CH_1, KCS_REG_IBF, ENABLE_IBF_INT);
	   aess_kcs_setreg(KCS_CH_2, KCS_REG_IBF, ENABLE_IBF_INT);
	   
	   /*Enable LPC reset*/
	   aess_kcs_setreg(KCS_CH_NONE, BIOS_REG_FIFO_INT, LPC_RST_INT_ENABLE);
    #endif
}
static void aess_kcs_isr_handler (UINT8 u8Channel)
{
	sKCSStruct *sInfo = &S_aKCSData[u8Channel];
	UINT8   u8Command = 0;
	UINT8   u8Data = 0;
	sKCSInfo sDummyInfo;   
	UINT8   pu8OutData[20];


	if (aess_kcs_getreg(sInfo->u8Channel , KCS_REG_IBF))
	{
		/* Command in */

		if (KCS_COMMAND == aess_kcs_getreg(sInfo->u8Channel , KCS_REG_CD))
		{
			/* BMC sets status to WRITE_STATE */
			aess_kcs_setstate(sInfo->u8Channel, KCS_STATUS_WRITE_STATE);
 
	 		/* dummy data to generate OBF (wr_start) */
			aess_kcs_setreg(sInfo->u8Channel, KCS_REG_ODR, KCS_DUMMY_DATA);
       
			/* get the command byte and  clear the IBF */
			u8Command = aess_kcs_getreg(sInfo->u8Channel , KCS_REG_CMD);

			if (KCS_WRITE_START == u8Command)
			{
				/* if "WRITE START" command be issued from SMS then set
				KCS machine state back to receive data state and clear
				receive buffer size */
				switch (sInfo->u8HIFISRState)
				{
					case KCS_TRANSFER_DATA_STATE: /* falls through */
					case KCS_TRANSFER_DATA_END_STATE:
						/** Send fail event to tell high-level firmware that
							KCS driver has been terminate message transfer */

					case KCS_IDLE_STATE: /* falls through */
					case KCS_ERROR_STATE: /* falls through */
					case KCS_RECEIVE_DATA_STATE: /* falls through */
					case KCS_RECEIVE_DATA_END_STATE: /* falls through */
					case KCS_RESPONSE_STATUS_PHASE1_STATE: /* falls through */
					case KCS_RESPONSE_STATUS_PHASE2_STATE:
						sInfo->u8HIFISRState = KCS_RECEIVE_DATA_STATE;

						sInfo->u8HIFInDataSize = 0;

						sInfo->u8HIFStatusCode = KCS_NO_ERROR;
						
						++sInfo->u8KCSSeqNo;
						break;
					default:    /* impossible case in normal */
						/* if the state is not expected then record error
						code [FF] and go to KCS error handler sub-routine */
						sInfo->u8HIFStatusCode = KCS_UNSPECIFIED_ERROR;

						/* dummy data to generate OBF (wr_start) */
						aess_kcs_setreg(sInfo->u8Channel, KCS_REG_ODR, KCS_DUMMY_DATA);

						aess_kcs_errorhandler(sInfo->u8Channel);
				}
			}
			else if ((KCS_WRITE_END == u8Command) && (KCS_RECEIVE_DATA_STATE == sInfo->u8HIFISRState))
			{
				/* if " WRITE END" command be issued from SMS then set
				   KCS machine state to receive last byte state */
				sInfo->u8HIFISRState = KCS_RECEIVE_DATA_END_STATE;
			}
			else if (KCS_ABORT_CMD == u8Command)
			{
				switch (sInfo->u8HIFISRState)
				{
					case KCS_ERROR_STATE: /* falls through */
					case KCS_IDLE_STATE:
						sInfo->u8HIFISRState = KCS_RESPONSE_STATUS_PHASE1_STATE;
						sInfo->u8HIFStatusCode = KCS_ABORTED_BY_COMMAND;
						break;
					case KCS_TRANSFER_DATA_STATE: /* falls through */
					case KCS_TRANSFER_DATA_END_STATE:
						/* Send fail event to tell high-level firmware that
						   KCS driver has been terminate message transfer */
						
					case KCS_RECEIVE_DATA_STATE: /* falls through */
					case KCS_RECEIVE_DATA_END_STATE: /* falls through */
					case KCS_RESPONSE_STATUS_PHASE1_STATE: /* falls through */
					case KCS_RESPONSE_STATUS_PHASE2_STATE:
						sInfo->u8HIFStatusCode = KCS_ABORTED_BY_COMMAND;
						sInfo->u8HIFISRState = KCS_RESPONSE_STATUS_PHASE1_STATE;
						break;
					default:
						/* if the state is not expected then record error
						code [FF] and go to KCS error handler sub-routine */
						sInfo->u8HIFStatusCode = KCS_UNSPECIFIED_ERROR;

						/* dummy data to generate OBF (wr_start) */
						aess_kcs_setreg(sInfo->u8Channel, KCS_REG_ODR, KCS_DUMMY_DATA);

						aess_kcs_errorhandler(sInfo->u8Channel);
				}
			}
			else   
			{
				/* if the command is not expected then record error
				code [02] and go to KCS error handler sub-routine */
				switch (sInfo->u8HIFISRState)
				{
					case KCS_TRANSFER_DATA_STATE: /* falls through */
					case KCS_TRANSFER_DATA_END_STATE:
						/* Send fail event to tell high-level firmware that
						   KCS driver has been terminate message transfer */
 
					case KCS_IDLE_STATE: /* falls through */
					case KCS_ERROR_STATE: /* falls through */
					case KCS_RECEIVE_DATA_STATE: /* falls through */
					case KCS_RECEIVE_DATA_END_STATE: /* falls through */
					case KCS_RESPONSE_STATUS_PHASE1_STATE: /* falls through */
					case KCS_RESPONSE_STATUS_PHASE2_STATE:
						sInfo->u8HIFStatusCode = KCS_ILLEGAL_CONTROL_CODE;
						aess_kcs_errorhandler(sInfo->u8Channel);
						break;
					default:    /* impossible case in normal */
						/* if the state is not expected then record error
						code [FF] and go to KCS error handler sub-routine */
						sInfo->u8HIFStatusCode = KCS_UNSPECIFIED_ERROR;

						/* dummy data to generate OBF (wr_start) */
						aess_kcs_setreg(sInfo->u8Channel, KCS_REG_ODR, KCS_DUMMY_DATA);

						aess_kcs_errorhandler(sInfo->u8Channel);
				}
			}
		}   /* end of if (COMMAND  == REG_STR.EQU.BIT.CD) */
		else        /* Data in */
		{
			switch (sInfo->u8HIFISRState)
			{
				case KCS_RECEIVE_DATA_STATE:
					/* dummy data to generate OBF (wr_data)*/
						aess_kcs_setreg(sInfo->u8Channel, KCS_REG_ODR, KCS_DUMMY_DATA);

					/* get the data byte and clear the IBF by H/W*/
					u8Data = aess_kcs_getreg(sInfo->u8Channel , KCS_REG_IDR);

					if (sInfo->u8HIFInDataSize < KCS_PACKAGE_MAX)
					{
						/* save incoming byte from SMS */
						sInfo->au8HIFReceiveBuf[sInfo->u8HIFInDataSize++] = u8Data;
					}
					else
					{
						/* if the length of incoming message over 40 bytes 
						   then record error code [06] */
						sInfo->u8HIFStatusCode = KCS_LENGTH_ERROR;
						
						aess_kcs_errorhandler(sInfo->u8Channel);
					}
					break;

				case KCS_RECEIVE_DATA_END_STATE:
				
					/* BMC sets status to READ_STATE */
					aess_kcs_setstate(sInfo->u8Channel, KCS_STATUS_READ_STATE);

					/* get the data byte and clear the IBF by H/W*/
					u8Data = aess_kcs_getreg(sInfo->u8Channel , KCS_REG_IDR);
  
					/* change KCS machine state to transfer data state */
					sInfo->u8HIFISRState = KCS_TRANSFER_DATA_STATE;

					if (sInfo->u8HIFInDataSize < KCS_PACKAGE_MAX)
					{
						/* save the last incoming byte from SMS */
						sInfo->au8HIFReceiveBuf[sInfo->u8HIFInDataSize++] = u8Data;
						sInfo->au8HIFReceiveBuf[sInfo->u8HIFInDataSize++] = sInfo->u8KCSSeqNo;

                        if(kcs_device_open == 1) 
                        {
						/* Send event to tell high-level firmware that KCS
						driver has been got a incoming message from SMS */
						
						//aess_generate_driver_event(sInfo->u16DriverID, sInfo->u32KCSRxOKEvent);
						}
						else
						{
							sDummyInfo.u8Channel = u8Channel;
							
                            sDummyInfo.u8WriteLength = 4;
                            pu8OutData[0] = sInfo->au8HIFReceiveBuf[0] | 0x04;
                            pu8OutData[1] = sInfo->au8HIFReceiveBuf[1]  ;
                            pu8OutData[2] = COMPCODE_NODE_BUSY;
                            pu8OutData[3] = sInfo->u8KCSSeqNo;
                            sDummyInfo.pu8Data = pu8OutData;
							aess_kcs_write(&sDummyInfo);
					    }
					}
					else
					{

						/* if the length of incoming message over 40 bytes then
						record error code [06] */
						sInfo->u8HIFStatusCode = KCS_LENGTH_ERROR;

						aess_kcs_errorhandler(sInfo->u8Channel);
					}

					break;

				case KCS_TRANSFER_DATA_STATE:
					/* get the data byte and clear the IBF by H/W*/
					u8Data = aess_kcs_getreg(sInfo->u8Channel , KCS_REG_IDR);
									
					if (KCS_READ == u8Data)
					{
						if (sInfo->u8HIFOutDataSize > 0)
						{
							sInfo->u8HIFOutDataSize--;
							if (0 == sInfo->u8HIFOutDataSize)
							{
								/* Transfer End */
								sInfo->u8HIFISRState = KCS_TRANSFER_DATA_END_STATE;
							}
              
							/* OBF (read), put one byte of output message to
							data out register */
							aess_kcs_setreg(sInfo->u8Channel, KCS_REG_ODR, *sInfo->pu8HIFOutDataPtr++);
						}
						else
						{

							/* if the length less than 0 then send fail event to
							   tell high-level firmware that KCS driver has
							   been terminate message transfer */


							/* record error code [06] */
							sInfo->u8HIFStatusCode = KCS_LENGTH_ERROR;
							aess_kcs_errorhandler(sInfo->u8Channel);
						}
					}
					else
					{

						/* if receive a not expacted control code then send fail
						event to tell high-level firmware that KCS driver has
						been terminate message transfer */

						/* record error code [02] */
						sInfo->u8HIFStatusCode = KCS_ILLEGAL_CONTROL_CODE;

						aess_kcs_errorhandler(sInfo->u8Channel);
					}
					break;

				/* Transfer End */
				case KCS_TRANSFER_DATA_END_STATE:
					/* set BMC status to IDLE state */
					aess_kcs_setstate(sInfo->u8Channel, KCS_STATUS_IDLE_STATE);

					/* OBF (read), write a dummy byte to data out register */
					aess_kcs_setreg(sInfo->u8Channel, KCS_REG_ODR, KCS_DUMMY_DATA);

					/* get the data byte and clear the IBF by H/W*/
					u8Data = aess_kcs_getreg(sInfo->u8Channel , KCS_REG_IDR);

					if (KCS_READ == u8Data)
					{
						/* KCS machine state back to idle state */
						sInfo->u8HIFISRState
						= KCS_IDLE_STATE;

						/* record error code [00] */
						sInfo->u8HIFStatusCode
						= KCS_NO_ERROR;
						/* Send OK event to tell high-level firmware that KCS
						driver has been transfer response data completely */

					}
					else
					{
						/* if receive a not expacted control code then send fail
						event to tell high-level firmware that KCS driver has
						been terminate message transfer */


						/* record error code [02] */
						sInfo->u8HIFStatusCode = KCS_ILLEGAL_CONTROL_CODE;
						aess_kcs_errorhandler(sInfo->u8Channel);
					}
					break;

				case KCS_RESPONSE_STATUS_PHASE1_STATE:
					/* Get input data and clear the IBF by H/W*/
					aess_kcs_setstate(sInfo->u8Channel, KCS_STATUS_READ_STATE);
					u8Data = aess_kcs_getreg(sInfo->u8Channel, KCS_REG_IDR);
					if (KCS_ZERO_DATA == u8Data)
					{               										  
						sInfo->u8HIFISRState = KCS_RESPONSE_STATUS_PHASE2_STATE;
						/* OBF (error2)*/
						aess_kcs_setreg(sInfo->u8Channel, KCS_REG_ODR, sInfo->u8HIFStatusCode);
					}
					else
					{
						/* dummy data to generate OBF */
						aess_kcs_setreg(sInfo->u8Channel, KCS_REG_ODR, KCS_DUMMY_DATA);
						sInfo->u8HIFStatusCode = KCS_ILLEGAL_CONTROL_CODE;
						aess_kcs_errorhandler(sInfo->u8Channel);
					}
					break;

				case KCS_RESPONSE_STATUS_PHASE2_STATE:
					/* BMC set status to IDLE state */
					aess_kcs_setstate(sInfo->u8Channel, KCS_STATUS_IDLE_STATE);

					/* dummy data to generate OBF (error3)*/
					aess_kcs_setreg(sInfo->u8Channel, KCS_REG_ODR,
									KCS_DUMMY_DATA);

                    /* Get input data and clear the IBF by H/W*/
					u8Data = aess_kcs_getreg(sInfo->u8Channel , KCS_REG_IDR);

														
					if (KCS_READ == u8Data)
					{
						sInfo->u8HIFStatusCode = KCS_NO_ERROR;
						sInfo->u8HIFISRState = KCS_IDLE_STATE;
					}
					else
					{
						sInfo->u8HIFStatusCode = KCS_ILLEGAL_CONTROL_CODE;
						aess_kcs_errorhandler(sInfo->u8Channel);
					}
					break;
				case KCS_ERROR_STATE:
					/* dummy data to generate OBF */
					aess_kcs_setreg(sInfo->u8Channel, KCS_REG_ODR,
									KCS_DUMMY_DATA);
					
					/* clear the IBF */
					u8Data = aess_kcs_getreg(sInfo->u8Channel , KCS_REG_IDR);
					break;

				default:
					sInfo->u8HIFStatusCode = KCS_UNSPECIFIED_ERROR;

					/* dummy data to generate OBF (wr_start) */
					aess_kcs_setreg(sInfo->u8Channel, KCS_REG_ODR,
									KCS_DUMMY_DATA);
					aess_kcs_errorhandler(sInfo->u8Channel);
					break;

			}  /* end of switch */
		}   /* end of else */
	}   /* end of if (REG_STR.EQU.BIT.IBF) */
}

static void aess_kcsdrv_bh(unsigned long u32Data)
{
	unsigned long flags;
    UINT16  u16CBFunDriverID;
    UINT32 u32CBFunEventID;

    /*Set CBFunEventID to event handler driver*/

	spin_lock_irqsave(&lock,   flags);
	u16CBFunDriverID = S_sLPCResetCBID.u16CBFunDriverID;
    u32CBFunEventID = S_sLPCResetCBID.u32CBFunEventID;
	spin_unlock_irqrestore(&lock,   flags);

    //aess_generate_driver_event(u16CBFunDriverID, u32CBFunEventID);             
}

struct file_operations aess_kcs_fops = {
	.open = aess_kcs_open, 
	.unlocked_ioctl = aess_kcs_ioctl,
	.release = aess_kcs_close,        
};

int __init aess_kcs_init(void)
{
	int result;
	sKCSInfo psInfo;
#ifdef CONFIG_OF
	struct device_node *np;
	struct resource res;

	np = of_find_compatible_node(NULL, NULL, "nuvoton,npcm750-kcs");
	if (np == NULL)
	{
		pr_info("Failed to find of_find_matching_node\n");
		return -ENODEV;
	}

	result = of_address_to_resource(np, 0, &res);
	if (result)
	{
		pr_info("\t\t\t of_address_to_resource fail ret %d \n", result);
		return -EINVAL;
	}

	kcs_virt_addr = ioremap_nocache(res.start, resource_size(&res));

	if (!kcs_virt_addr)
	{
		pr_info("\t\t\t kcs_virt_addr fail \n");
		return -ENOMEM;
	}
	printk(KERN_INFO "\t\t\t* KCS base is 0x%08X ,res.start 0x%08X \n", (u32) kcs_virt_addr, res.start);

	kcs_irq = irq_of_parse_and_map(np, 0);
	if (!kcs_irq) {
		printk(KERN_ERR "%s - failed to map irq\n", __FUNCTION__);
		return (-EAGAIN);
	}
	
#endif

    psInfo.u8Channel=0;
    psInfo.u16DriverID=0;
	
	spin_lock_init(&lock);

	kcs_cdev = cdev_alloc();
	

	kcs_cdev->ops = &aess_kcs_fops;
	
	if (majornum <= 0)
	{
		/* allocate a device number */
		result = alloc_chrdev_region(&kcs_dev, 0, 1, driver_name);
		majornum = MAJOR(kcs_dev);
	}
	else
	{
		kcs_dev = MKDEV(majornum, 0);
		result = register_chrdev_region(kcs_dev, 1, driver_name);
	}
	
	if (result < 0) {
		printk (KERN_ERR "KN:Registering the kcs device failed with %d\n", MAJOR(kcs_dev));
		return result;
	}

	cdev_add(kcs_cdev, kcs_dev, 1);


	/* Register IRQ, assign channel 0 data structure for dev_id */
 	//if (request_irq(KCS_HIB_INT, aess_kcs_isr, IRQF_SHARED|IRQF_SAMPLE_RANDOM, "KCS", (void *) &S_aKCSData[KCS_CH_0])) 
 	// modify by T.M. 12/11/2015
 	if (request_irq(KCS_HIB_INT, aess_kcs_isr, IRQF_SHARED, "KCS", (void *) &S_aKCSData[KCS_CH_0])) 
	{
		return -EBUSY;
	}  
	/* For response time shorten to 20sec */
	/* Set the initial value for data structure */
    memset(S_aKCSData, 0, sizeof(sKCSStruct) * KCS_TOTAL_CHANNEL);
    
	/* Set KCS Channel 0 Software parameters and Hardware */
    /* Save Channel ID */
	S_aKCSData[0].u8Channel = 0;
	/* set HIF state machine to idle state */
	S_aKCSData[0].u8HIFISRState = KCS_IDLE_STATE;
	/* clear error code */
	S_aKCSData[0].u8HIFStatusCode = KCS_NO_ERROR;

	/* KCS SeqNo init */
	S_aKCSData[0].u8KCSSeqNo = 0;
    
	
	aess_kcs_setreg(KCS_CH_NONE, BIOS_REG_FIFO_INT, LPC_RST_INT_ENABLE);
	/* set initial state to Error state to mit the KCS spec */
    aess_kcs_setstate(KCS_CH_0, KCS_STATUS_ERROR_STATE);
#ifdef SUPPORT_SMI
   /*Set pin function to nSMI*/
    aess_kcs_setreg(KCS_CH_NONE, GC_REG_MFSEL_SMI, ENABLE_SMI_FUNCTION);
#endif 
	/* enable KCS CH0 IBF interrupt */
	aess_kcs_setreg(KCS_CH_0, KCS_REG_IBF, ENABLE_IBF_INT);
    /*Enable Software SMI and Disable Hardware SMI*/
    aess_kcs_setreg(KCS_CH_0, KCS_REG_IE, DISABLE_HWSMI);
    aess_kcs_setreg(KCS_CH_0, KCS_REG_IE, ENABLE_SWSMI);


   
    /* Set KCS Channel 1 Software parameters and Hardware */
    /* Save Channel ID */
	S_aKCSData[1].u8Channel = 1;
	/* set HIF state machine to idle state */
	S_aKCSData[1].u8HIFISRState = KCS_IDLE_STATE;
	/* clear error code */
	S_aKCSData[1].u8HIFStatusCode = KCS_NO_ERROR;

	/* KCS SeqNo init */
	S_aKCSData[1].u8KCSSeqNo = 0;
	
    aess_kcs_setreg(KCS_CH_NONE, BIOS_REG_FIFO_INT, LPC_RST_INT_ENABLE);
    aess_kcs_setstate(KCS_CH_1, KCS_STATUS_ERROR_STATE);

#ifdef SUPPORT_SMI
    /*Set pin function to nSMI*/
    aess_kcs_setreg(KCS_CH_NONE, GC_REG_MFSEL_SMI, ENABLE_SMI_FUNCTION);
#endif 

	/* enable KCS CH1 IBF interrupt */
	aess_kcs_setreg(KCS_CH_1, KCS_REG_IBF, ENABLE_IBF_INT);

    /*Enable Software SMI and Disable Hardware SMI*/
    aess_kcs_setreg(KCS_CH_1, KCS_REG_IE, DISABLE_HWSMI);
    aess_kcs_setreg(KCS_CH_1, KCS_REG_IE, ENABLE_SWSMI);

    printk("mknod /dev/aess_kcsdrv c %d 0\n", MAJOR(kcs_dev));
	
	result = aess_kcs_channel_init(&psInfo);         

	return result;
}

void __exit aess_kcs_exit(void)
{
	unsigned long flags;
	spin_lock_irqsave(&lock,   flags);
	aess_kcs_clearall();
	spin_unlock_irqrestore(&lock,   flags);

	DEBUG_MSG("aess_kcs_exit\n");
	cdev_del(kcs_cdev);
	unregister_chrdev_region(kcs_dev, 1);    
	return;
}


MODULE_DESCRIPTION("AESS KCS Driver");
MODULE_AUTHOR("Kopi Hsu <kopi.hsu@avocent.com>");
MODULE_LICENSE("GPL");
module_param(driver_name, charp, S_IRUGO);
module_init(aess_kcs_init);
module_exit(aess_kcs_exit);
#ifdef CONFIG_OF
EXPORT_SYMBOL(kcs_virt_addr);
EXPORT_SYMBOL(kcs_irq);
#endif
