#define DEBUG
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/types.h> /* size_t */
#include <linux/interrupt.h>
#include <linux/version.h>
#include <asm/uaccess.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/workqueue.h>	/* We scheduale tasks here */
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/spinlock.h>

#include <mach/hal.h>
#include "vdm_module.h"
#include "vdm_api.h"
#include "CircularBuffer.h"
#include <linux/delay.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

void __iomem * vdm_virt_addr  = NULL;
void __iomem * vdma_virt_addr = NULL;
#undef NPCMX50_VDMA_INTERRUPT
#define NPCMX50_VDMA_INTERRUPT vdma_interrupt
int vdma_interrupt;

static const struct of_device_id vdm_dt_npcm750_match[] = {
       { .compatible = "nuvoton,npcm750-vdm" },
       { /*sentinel*/ },
};

#endif


#define  AIC_GEN_MTCP   31

//#DEBUG

static int majornum = 0;           /* default to dynamic major */
module_param(majornum, int, 0);
MODULE_PARM_DESC(majornum, "Major device number");


// TODO: Avi: check if exist in BMC_HAL
#define AIC_GEN				0x84
#define AIC_GRSR			0x88

#define AIC_GEN_REG					(AIC_BASE_ADDR + AIC_GEN), AIC_ACCESS, 32
#define AIC_GRSR_REG				(AIC_BASE_ADDR + AIC_GRSR), AIC_ACCESS, 32

#define DEVICE_NAME 	"vdm"

#define VDMA_BUFF_BYTE_SIZE (16*1024) //  buffer size in bytes

#define RESET_POLL_TICK_mS	100
#define VDM_STATUS_OPENED	0
#define VDM_STATUS_CLOSED	1

typedef struct my_struct {
        struct list_head list;
        uint16_t mBDF;
        uint8_t *mprxBuffer;
        uint8_t BDF_is_set;
        uint32_t mprxBufferLength;
        uint8_t *mptxBuffer;
        uint32_t mptxBufferLength;
        CircularBuffer_t circularBuffer;
        uint32_t last_errors;
        uint32_t dbg_counter;
        wait_queue_head_t       in_data_available_wait;
} vdm_instance_t;

extern unsigned long loops_per_jiffy;
unsigned long loops_per_jiffy_for_ndelay ;

static uint32_t *vdma_buff;
static uint32_t *vdma_buff_virt_addr;
static uint32_t disable_TX=0;
static uint8_t stop_reset_poll=0;
static uint8_t vdm_status = VDM_STATUS_CLOSED;
static uint8_t reset_detect_poll = 1;
static uint8_t reset_detect_poll_is_running = 0;

static struct tasklet_struct   vdm_tasklet;
static struct tasklet_struct   vdm_tasklet_with_overflow;
static spinlock_t lock;

static dev_t vdm_dev;
static struct cdev *vdm_cdev;
static struct platform_device *vdm_pdev=NULL;
static struct class *vdm_class;
static struct device *vdm_sysfs_device;


static LIST_HEAD(vdm_instances_list);

static int dummy_vdma_dev;

static vdm_instance_t *pVDM_Instance_Default = NULL ;

static void vdm_tasklet_function( unsigned long data );
static void vdm_tasklet_function_with_overflow( unsigned long data );

static void reset_poll_routine(struct work_struct *irrelevant);
static DECLARE_DELAYED_WORK(reset_poll_work, reset_poll_routine);
static uint32_t vdm_data_for_platform;

extern void __loop_nanodelay(unsigned long usecs);

/* function : ndelay
 *
 *
 *
 */
void nano_delay(uint32_t nsec)
{
	__loop_nanodelay(nsec);
}

/* function : copy_to_user_wrapper
 *
 *
 *
 */
static void *copy_to_user_wrapper(void *dest, const void *src, size_t n)
{
	uint32_t bytes_copied;
	bytes_copied=copy_to_user(dest,src,n);
	return dest;
}

/* function : reset_poll_routine
 *
 *
 *
 */
static void reset_poll_routine(struct work_struct *irrelevant)
{
	unsigned long flags;
	struct list_head *pList;
	vdm_instance_t *pVDM_Instance;

	spin_lock_irqsave(&lock,   flags);

	reset_detect_poll_is_running = 0;

	if (vdm_is_in_reset())
    {
//		pr_debug("%s : vdm reset detected\n",__FUNCTION__);
    	// wake up all vdm_instances
    	list_for_each(pList, &vdm_instances_list)
    	{
    		pVDM_Instance = list_entry(pList, vdm_instance_t, list);
    		pVDM_Instance->last_errors |= PCIE_VDM_ERR_BUS_RESET_OCCURED;
    		wake_up_interruptible(&pVDM_Instance->in_data_available_wait);
    	}
    }
	else
	{
		/*
		 * If we wants to stop task
		 */
		if ((stop_reset_poll == 0) && (0 == reset_detect_poll_is_running))
		{
			reset_detect_poll_is_running = 1;
			schedule_delayed_work(&reset_poll_work, RESET_POLL_TICK_mS);
		}
	}
	spin_unlock_irqrestore(&lock,   flags);
}

/* function : bdf_U16_to_bdf_arg
 *
 *
 *
 */
static void bdf_U16_to_bdf_arg(uint16_t bdf_U16, bdf_arg_t *apBdf)
{
	apBdf->bus = ( bdf_U16 >> 8 ) & 0xff ;
	apBdf->device = ( bdf_U16 >> 3 ) & 0x1f ;
	apBdf->function = bdf_U16  & 0x07 ;
}

/* function : bdf_arg_to_bdf_U16
 *
 *
 *
 */
static uint16_t bdf_arg_to_bdf_U16(bdf_arg_t *apBdf)
{
	return ( ((apBdf->bus & 0xff) << 8) + ((apBdf->device & 0x1f) << 3) + (apBdf->function & 7) ) ;
}

/* function : _vdm_open
 *
 *
 *
 */
static int _vdm_open(void)
{


	if(VDM_STATUS_CLOSED == vdm_status)
	{
		/*-----------------------------------------------------------------------------------------------------*/
		/* init vdm hw module                                                                                    */
		/*-----------------------------------------------------------------------------------------------------*/
		if(vdm_init_common(vdma_buff, vdma_buff_virt_addr , VDMA_BUFF_BYTE_SIZE))
		{
			return -EINVAL;
		}



		/*-----------------------------------------------------------------------------------------------------*/
		/* Init tasklets.                                                                                      */
		/*-----------------------------------------------------------------------------------------------------*/
		tasklet_init(&vdm_tasklet, vdm_tasklet_function, 0);
		tasklet_init(&vdm_tasklet_with_overflow, vdm_tasklet_function_with_overflow, 0);

		vdm_status = VDM_STATUS_OPENED;
	}

    return 0 ;
}

/* function : _vdm_close
 *
 *
 *
 */
static void _vdm_close(void)
{
	struct list_head *pList;
	vdm_instance_t *pVDM_Instance;

	vdm_exit_common();
	if(VDM_STATUS_OPENED == vdm_status)
	{
		tasklet_kill( &vdm_tasklet );
		tasklet_kill( &vdm_tasklet_with_overflow );

		// reinit vdm_instances
		list_for_each(pList, &vdm_instances_list)
		{
			pVDM_Instance = list_entry(pList, vdm_instance_t, list);
			cbInit(&pVDM_Instance->circularBuffer, (pVDM_Instance->mprxBufferLength/sizeof(uint32_t)) ,
			            		pVDM_Instance->mprxBuffer , sizeof(uint32_t),memcpy,copy_to_user_wrapper);
			pVDM_Instance->BDF_is_set = 0;

		}
		vdm_status = VDM_STATUS_CLOSED;

	}

}

/* function : vdm_read
 *
 *
 *
 */
static ssize_t vdm_read(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{ 
	long ret=0;
	unsigned long flags;
	uint32_t num_of_items_to_read_from_buff =0 ;//, StartPosToRead;
	vdm_instance_t *pVDM_Instance;
	pVDM_Instance=filp->private_data;

	count = count/sizeof(uint32_t); // count is in bytes. we need to convert it to words



	while(1)
	{

		spin_lock_irqsave(&lock,   flags);
    	if(0 == pVDM_Instance->BDF_is_set)
    	{
    		ret = -ENOENT  ;
			pr_debug("%s : BDF was not set\n",__FUNCTION__);
			goto unlock_and_exit;
    	}

		if (vdm_is_in_reset())
		{
			pVDM_Instance->last_errors |= PCIE_VDM_ERR_BUS_RESET_OCCURED;
			pr_debug("%s : vdm reset detected\n",__FUNCTION__);
			goto unlock_and_exit;
		}

		if (signal_pending(current))
		{
			ret = -EINTR;
			goto unlock_and_exit;
		}

		num_of_items_to_read_from_buff = cbRead(&pVDM_Instance->circularBuffer, buf, count);

		ret = num_of_items_to_read_from_buff<<2;

		if (num_of_items_to_read_from_buff)
		{
			pVDM_Instance->dbg_counter -= ret;
			pr_debug("%s : bdf=0x%X bytes in instance buffer %d\n",__FUNCTION__ ,
					pVDM_Instance->mBDF,pVDM_Instance->dbg_counter );
			goto unlock_and_exit ;
		}
		else
		{
			if(filp->f_flags & O_NONBLOCK)
			{
				ret = -EAGAIN;
				goto unlock_and_exit;
			}
		}

		if (vdm_is_in_reset())
		{
			pVDM_Instance->last_errors |= PCIE_VDM_ERR_BUS_RESET_OCCURED;
			pr_debug("%s : vdm reset detected\n",__FUNCTION__);
			goto unlock_and_exit;
		}

		stop_reset_poll = 0;
		if ((reset_detect_poll) && (0 == reset_detect_poll_is_running))
		{
			reset_detect_poll_is_running = 1;
			schedule_delayed_work(&reset_poll_work, RESET_POLL_TICK_mS);
		}

		spin_unlock_irqrestore(&lock,   flags);


		wait_event_interruptible( pVDM_Instance->in_data_available_wait ,
				(vdm_is_in_reset() || ( 0 != cbGetNumOfElements(&pVDM_Instance->circularBuffer) ) ));



	}

unlock_and_exit:
	stop_reset_poll = 1;
	if(pVDM_Instance->last_errors)
	{
		ret = -EIO;
	}
	spin_unlock_irqrestore(&lock,   flags);
	return (ret) ;
  
}

/* function : vdm_write
 *
 *
 *
 */
static ssize_t vdm_write( struct file *filp, const char *buf, size_t count, loff_t *f_pos)
{
	long ret ;
	unsigned long flags;
	uint8_t route_type;
	vdm_instance_t *pVDM_Instance;
	pVDM_Instance=filp->private_data;
	ret = count ;
	spin_lock_irqsave(&lock,   flags);
	
	if (vdm_is_in_reset())
    {
		pVDM_Instance->last_errors |= PCIE_VDM_ERR_BUS_RESET_OCCURED;
		ret = -EIO;
    	goto failed;
    }

	if(0 != disable_TX)
	{
		pr_debug("<1> vdm module TX stopped  \n");
		ret = 0;
    	goto failed;
	}
	//pr_debug("<1> vdm module write start  \n");

	if((count-1) > pVDM_Instance->mptxBufferLength)
	{
		pr_debug("<1> vdm module data length is too big\n");
		ret = -EINVAL;
		goto failed;
	}
	if(count < 2)
	{
		pr_debug("<1> vdm module : wrong buffer\n");
		ret = -EINVAL;
		goto failed;
	}

	if(copy_from_user( pVDM_Instance->mptxBuffer , &buf[1] , count-1))
	{
		ret = -EFAULT;
		goto failed;
	}

	if(0 == pVDM_Instance->BDF_is_set)
	{
		ret = -ENOENT  ;
		pr_debug("%s : BDF was not set\n",__FUNCTION__);
		goto failed;
	}

	route_type=buf[0];
	if	(	(PCIe_HEADER_ROUTE_BY_ID 	!= route_type) &&
			(PCIe_HEADER_ROUTE_TO_RC 	!= route_type) &&
			(PCIe_HEADER_ROUTE_FROM_RC 	!= route_type) )
	{
		route_type=PCIe_HEADER_ROUTE_FROM_RC;
	}

	if (0 == vdm_SendMessage(route_type , pVDM_Instance->mBDF , pVDM_Instance->mptxBuffer,count-1))
	{
		ret = count;
	}




failed :

	if(pVDM_Instance->last_errors)
	{
		ret = -EIO;
	}

    spin_unlock_irqrestore(&lock,   flags);

	//pr_debug("<1> vdm module write end \n");

	return ret;
  
}






/* function : vdm_ioctl
 *
 *
 *
 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35))
static int vdm_ioctl(struct inode *i, struct file *f, unsigned int cmd, unsigned long arg)
#else
static long vdm_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
#endif
{
	long ret = 0 ;
	unsigned long flags;
	vdm_instance_t *pVDM_Instance;
	pVDM_Instance=filp->private_data;
	pr_debug("<1> vdm : cmd = %d \n",cmd);
	spin_lock_irqsave(&lock,   flags);

    if (vdm_is_in_reset())
    {
    	pVDM_Instance->last_errors |= PCIE_VDM_ERR_BUS_RESET_OCCURED;
    }

    switch (cmd)
    {
        case PCIE_VDM_GET_BDF:
            {
                bdf_arg_t bdf_arg;
            	//pVDM_Instance->mBDF=bdf_arg_to_bdf_U16((bdf_arg_t *)arg);
            	bdf_U16_to_bdf_arg(pVDM_Instance->mBDF, &bdf_arg);
            	if(0 == pVDM_Instance->BDF_is_set)
            	{
            		ret = -ENOENT  ;
            	}
                
        		if (copy_to_user((void __user *)arg, &bdf_arg, sizeof(bdf_arg)))
            	{
            	    return -EFAULT;
            	}
            }
            break;
        case PCIE_VDM_SET_BDF :
            {
                bdf_arg_t bdf_arg;
                
            	//bdf_U16_to_bdf_arg(pVDM_Instance->mBDF,(bdf_arg_t *)arg);
            	if (copy_from_user(&bdf_arg, (void __user *) arg, sizeof(bdf_arg)))
            	{
            	    return -EFAULT;
            	}
            	pVDM_Instance->mBDF=bdf_arg_to_bdf_U16(&bdf_arg);
                pr_debug("<1> vdm : PCIE_VDM_SET_BDF - pVDM_Instance->mBDF = %04X\n", pVDM_Instance->mBDF); //AJ DBG
            	if(0 == pVDM_Instance->mBDF)
            	{
            		pVDM_Instance_Default = pVDM_Instance;
            	}
            	pVDM_Instance->BDF_is_set = 1;
            }
            break;

        case PCIE_VDM_SET_TRANSMIT_BUFFER_SIZE:
        	kfree(pVDM_Instance->mptxBuffer);
        	pVDM_Instance->mptxBufferLength=(uint32_t)arg;
        	pVDM_Instance->mptxBuffer=kmalloc(pVDM_Instance->mptxBufferLength,GFP_KERNEL);
            if (pVDM_Instance->mptxBuffer == NULL)
            {
            	ret = -EINVAL;
        	}
            break;

        case PCIE_VDM_SET_RECEIVE_BUFFER_SIZE:
        	kfree(pVDM_Instance->mprxBuffer);

        	pVDM_Instance->mprxBufferLength=(uint32_t)arg;
        	pVDM_Instance->mprxBuffer=kmalloc((uint32_t)arg,GFP_KERNEL);
            if (pVDM_Instance->mprxBuffer == NULL)
            {
        		ret = -EINVAL;
                break;
        	}
            cbInit(&pVDM_Instance->circularBuffer, (pVDM_Instance->mprxBufferLength/sizeof(uint32_t)) ,
            		pVDM_Instance->mprxBuffer , sizeof(uint32_t),memcpy,copy_to_user_wrapper);

            break;

        case PCIE_VDM_STOP_VDM_TX:
			if(0 == (uint32_t)arg)
			{
				disable_TX=0;
			}
			else
			{
				disable_TX=1;
			}
            break;
			
        case PCIE_VDM_STOP_VDM_RX:
			if(0 == (uint32_t)arg)
			{
				vdm_enable_rx();
			}
			else
			{
				vdm_disable_rx();
			}
            break;
			
        case PCIE_VDM_RESET:
        	vdm_reset();
            break;

        case PCIE_VDM_SET_RX_TIMEOUT:
        	if (vdm_set_timeout((VDM_RX_TIMEOUT_t)arg) )
        	{
        		ret = -EINVAL;
        	}
            break;

        case PCIE_VDM_REINIT:
        	_vdm_close();
        	vdm_reset();
        	_vdm_open();
        	break;

        case PCIE_VDM_SET_RESET_DETECT_POLL:
        	reset_detect_poll = (uint32_t)arg;
        	break;

        case PCIE_VDM_GET_ERRORS:
    		if (copy_to_user((void __user *)arg, &pVDM_Instance->last_errors, sizeof(pVDM_Instance->last_errors)))
        	{
        	    return -EFAULT;
        	}
        	break;

        case PCIE_VDM_CLEAR_ERRORS:
        	{
        		uint32_t last_errors =  (uint32_t)arg;
        		if(PCIE_VDM_ERR_HW_FIFO_OVERFLOW & last_errors)
        		{
        			vdm_clear_errors(VDM_ERR_FIFO_OVERFLOW);
        		}
        		if(PCIE_VDM_ERR_USER_BUFFER_OVERFLOW & last_errors)
        		{
                    cbInit(&pVDM_Instance->circularBuffer, (pVDM_Instance->mprxBufferLength/sizeof(uint32_t)) ,
                    		pVDM_Instance->mprxBuffer , sizeof(uint32_t),memcpy,copy_to_user_wrapper);
        		}
        		pVDM_Instance->last_errors &= (~last_errors);
        	}
        	break;

        default:
        	ret = -EINVAL;
    }

    if (pVDM_Instance->last_errors & PCIE_VDM_ERR_BUS_RESET_OCCURED)
    {
    	ret = -EIO;
    }
	spin_unlock_irqrestore(&lock,   flags);

    return ret;
}




/* function : vdm_open
 *
 *
 *
 */
static int vdm_open(struct inode *inode, struct file *filp)
{
	unsigned long flags;
	vdm_instance_t *pVDM_Instance;

    pVDM_Instance=kmalloc(sizeof(vdm_instance_t),GFP_KERNEL);
    if (NULL == pVDM_Instance )
    {
		return -EINVAL;
	}

    memset(pVDM_Instance,0,sizeof(vdm_instance_t));
	pVDM_Instance->BDF_is_set = 0;
	pVDM_Instance->last_errors = 0;
	pVDM_Instance->dbg_counter = 0;
	pVDM_Instance->mptxBufferLength=_1KB_;// default data buffer
	pVDM_Instance->mptxBuffer=kmalloc(pVDM_Instance->mptxBufferLength,GFP_KERNEL);
    if (pVDM_Instance->mptxBuffer == NULL)
    {
        kfree(pVDM_Instance);
		return -EINVAL;
	}
	pVDM_Instance->mprxBufferLength=_64KB_;// default data buffer
	pVDM_Instance->mprxBuffer=kmalloc(pVDM_Instance->mprxBufferLength,GFP_KERNEL);
    if (pVDM_Instance->mprxBuffer == NULL)
    {
        kfree(pVDM_Instance->mptxBuffer);
        kfree(pVDM_Instance);
		return -EINVAL;
	}
    init_waitqueue_head(&pVDM_Instance->in_data_available_wait);

    cbInit(&pVDM_Instance->circularBuffer, (pVDM_Instance->mprxBufferLength/sizeof(uint32_t)) ,
    		pVDM_Instance->mprxBuffer , sizeof(uint32_t),memcpy,copy_to_user_wrapper);

	filp->private_data=pVDM_Instance;
	INIT_LIST_HEAD(&pVDM_Instance->list);

	spin_lock_irqsave(&lock,   flags);
	list_add ( &pVDM_Instance->list , &vdm_instances_list ) ;
	spin_unlock_irqrestore(&lock,   flags);

	pr_debug("<1> vdm module open \n");
  /* Success */
	return 0;
}

/* function : vdm_release
 *
 *
 *
 */
static int vdm_release(struct inode *inode, struct file *filp)
{
	vdm_instance_t *pVDM_Instance;
	pVDM_Instance=filp->private_data;
	kfree(pVDM_Instance->mptxBuffer);
	kfree(pVDM_Instance->mprxBuffer);
	list_del ( &pVDM_Instance->list ) ;

	if(pVDM_Instance_Default == pVDM_Instance)
	{
		pVDM_Instance_Default = NULL;
	}

	kfree(pVDM_Instance);
	pr_debug("<1> vdm module release \n");
  /* Success */
  return 0;
}



/* function : vdm_poll
 *
 *
 *
 */
static unsigned int vdm_poll(struct file *filp, poll_table *wait)
{
	unsigned long key;
	unsigned long flags;
	unsigned int mask;
	vdm_instance_t *pVDM_Instance;
	pVDM_Instance=filp->private_data;

	mask = 0;


	poll_wait(filp, &pVDM_Instance->in_data_available_wait,  wait);

	key = poll_requested_events(wait);

	spin_lock_irqsave(&lock,   flags);

	if(vdm_is_in_reset())
	{
		pVDM_Instance->last_errors |= PCIE_VDM_ERR_BUS_RESET_OCCURED;
		//		pr_debug("%s : vdm reset detected\n",__FUNCTION__);
		stop_reset_poll = 1;
		goto poll_exit;
	}

    if (cbGetNumOfElements(&pVDM_Instance->circularBuffer))
    {
        mask |= POLLIN | POLLRDNORM;    /* readable */

    }
    else
    {
    	if(POLLIN & key)
    	{
    		stop_reset_poll = 0;
    		if ((reset_detect_poll) && (0 == reset_detect_poll_is_running))
    		{
    			reset_detect_poll_is_running = 1;
    			schedule_delayed_work(&reset_poll_work, RESET_POLL_TICK_mS);
    		}
    	}
    }

    if ( vdm_is_ready_for_write())
        mask |= POLLOUT | POLLWRNORM;   /* writable */



	if(key & mask)
	{
		stop_reset_poll = 1;
	}

poll_exit :
	if(pVDM_Instance->last_errors)
	{
		mask |= POLLHUP;
	}
	spin_unlock_irqrestore(&lock,   flags);

    return mask;
}



static void receive_packet_func(uint16_t aBDF , uint32_t *data , uint32_t NumOfWords ,uint8_t isThisLastDataInPacket)
{
	uint32_t  num_of_items_written;
	struct list_head *pList;
	vdm_instance_t *pVDM_Instance;

	// find instance with proper BDF
	list_for_each(pList, &vdm_instances_list)
	{
		pVDM_Instance = list_entry(pList, vdm_instance_t, list);
        pr_debug("%s : Checking VDM_Instance->mBDF = %04X received = %04X\n",__FUNCTION__, pVDM_Instance->mBDF, aBDF); //AJ DBG
		if(pVDM_Instance->mBDF == aBDF)
		{
			goto found_instance;
		}
	}

	pVDM_Instance = pVDM_Instance_Default;

found_instance :

	if(NULL != pVDM_Instance)
	{
		pVDM_Instance->dbg_counter += (NumOfWords*4);
		pr_debug( "vdm %s : bytes in instance  %d\n",__FUNCTION__,pVDM_Instance->dbg_counter );
		num_of_items_written = cbWrite(&pVDM_Instance->circularBuffer, data, NumOfWords);
		if( num_of_items_written != NumOfWords )
		{
			pr_debug("<1>vdm:user buffer overflowed\n");
			pVDM_Instance->last_errors |= PCIE_VDM_ERR_USER_BUFFER_OVERFLOW;
		}
		wake_up_interruptible(&pVDM_Instance->in_data_available_wait);
	}
	else
	{
		pr_debug( "<1>  vdm : data discard because no instance found with proper BDF 0x%X\n",aBDF);
	}



}

/* function : vdm_tasklet_function
 *
 *
 *
 */
static void vdm_tasklet_function( unsigned long data )
{
	unsigned long flags;
	//pr_debug("<1>vdm:vdm_tasklet_function\n");
	spin_lock_irqsave(&lock,   flags);
	vdma_copy_packets_from_buffer(receive_packet_func);
	spin_unlock_irqrestore(&lock,   flags);
}

/* function : vdm_tasklet_function
 *
 *
 *
 */
static void vdm_tasklet_function_with_overflow( unsigned long data )
{
	unsigned long flags;
	pr_debug("<1>vdm:vdm_tasklet_function with overflow\n");

	spin_lock_irqsave(&lock,   flags);

	vdma_copy_packets_from_buffer_with_overflow(receive_packet_func);
	spin_unlock_irqrestore(&lock,   flags);

}

/* function : vdma_isr
 *
 *
 *
 */
static irqreturn_t vdma_isr(int irq, void *dev_id)
{
	struct list_head *pList;
	vdm_instance_t *pVDM_Instance;

	irqreturn_t irqreturn = IRQ_NONE;

	VDMA_STATUS_t status  ;

	spin_lock(&lock);

	status = vdma_is_data_ready();
	if(VDM_ERR_FIFO_OVERFLOW & vdm_get_errors() )
	{
		list_for_each(pList, &vdm_instances_list)
		{
			pr_debug("<1>vdm:vdm fifo overflowed\n");
			pVDM_Instance = list_entry(pList, vdm_instance_t, list);
			pVDM_Instance->last_errors |= PCIE_VDM_ERR_HW_FIFO_OVERFLOW;
		}
	}

	if(VDMA_STATUS_NONE != status)
	{
//		pr_debug("<1>vdm:vdma_isr 1\n");
		if((VDMA_STATUS_OVERFLOWED == status) || (VDMA_STATUS_DATA_READY_WITH_OVERFLOWED == status))
		{
			pr_debug("<1>vdm:vdma overflowed\n");
			// find instance with proper BDF
			list_for_each(pList, &vdm_instances_list)
			{
				pVDM_Instance = list_entry(pList, vdm_instance_t, list);
				pVDM_Instance->last_errors |= PCIE_VDM_ERR_DMA_BUFFER_OVERFLOW;
			}
			tasklet_schedule( &vdm_tasklet_with_overflow );
		}
		else
		{
			tasklet_schedule( &vdm_tasklet );
		}
	}
	spin_unlock(&lock);


	irqreturn = IRQ_HANDLED;

	return irqreturn;

}

//#define  _USE_VDMA_POLLING

#ifdef _USE_VDMA_POLLING

static void task_routine(void *);
static int stop_task = 0;		/* set this to 1 for shutdown */
static struct workqueue_struct *my_workqueue = 0;
static DECLARE_DELAYED_WORK(Task, task_routine);

#define MY_WORK_QUEUE_NAME "WQsched.c"


/* function : task_routine
 *
 *
 *
 */
static void task_routine(void *irrelevant)
{

	vdma_isr(0,0);

	/*
	 * If cleanup wants us to stop_task
	 */
	if (stop_task == 0)
		queue_delayed_work(my_workqueue, &Task, 1000);
}
#endif


/*---------------------------------------------------------------------------------------------------------*/
/* Function:        vdm_probe                                                                            */
/*                                                                                                         */
/* Parameters:                                                                                             */
/*                  pdev -                                                                                 */
/*                                                                                                         */
/* Returns:                                                                                                */
/* Side effects:                                                                                           */
/* Description:                                                                                            */
/*                  This routine performs probe for multiple slots                                         */
/*---------------------------------------------------------------------------------------------------------*/
static int vdm_probe(struct platform_device *pdev)
{
    int ret=0;

    if(pdev == NULL)
	{
		pr_debug("<1> vdm probe ERROR : pdev=NULL\n" );
	}

	vdm_pdev=pdev;

    /*-----------------------------------------------------------------------------------------------------*/
    /* Set up the chip as the driver data of the device                                                    */
    /*-----------------------------------------------------------------------------------------------------*/
    platform_set_drvdata(pdev, &vdm_data_for_platform);

    return ret;
}



/*---------------------------------------------------------------------------------------------------------*/
/* Function:        vdm_remove                                                                           */
/*                                                                                                         */
/* Parameters:                                                                                             */
/*                  pdev -                                                                                 */
/*                                                                                                         */
/* Returns:                                                                                                */
/* Side effects:                                                                                           */
/* Description:                                                                                            */
/*                  This routine handles driver uninitialization                                           */
/*---------------------------------------------------------------------------------------------------------*/
static int vdm_remove(struct platform_device *pdev)
{

    platform_set_drvdata(pdev, NULL);

    return 0;
}

/* Structure that declares the usual file */
/* access functions */
static struct file_operations vdm_fops =
{
  .read = vdm_read,
  .write = vdm_write,
  .open = vdm_open,
  .release = vdm_release,
  .poll = vdm_poll,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35))
  .release = vdm_ioctl
#else
  .unlocked_ioctl = vdm_ioctl
#endif
};


/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                           Device Declaration                                            */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/

static void vdm_device_release(struct device *dev)
{
     
}

#define DRIVER_NAME         "vdm"
static struct resource vdm_resources[] /*=
{
    [0] = {
        .start  = NPCMX50_PA_SDHC(SD2_DEV),
        .end    = NPCMX50_PA_SDHC(SD2_DEV) + NPCMX50_SZ_SDHC - 1,
        .flags  = IORESOURCE_MEM,
    },
}*/;

//u64 vdm_dmamask = 0xffffffff;
static u64 vdm_dmamask = DMA_BIT_MASK(32);

static struct platform_device vdm_device =
{
    .name       = DRIVER_NAME,
    .id         = 0,
    .dev        = {
                .dma_mask           = &vdm_dmamask,
                .coherent_dma_mask  = 0xffffffff,
                .release            = vdm_device_release,
    },
    .resource   = NULL,//vdm_resources,
    .num_resources  = 0,//ARRAY_SIZE(vdm_resources),
};

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
/*                                           Driver Declaration                                            */
/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
static struct platform_driver vdm_driver =
{
    .driver     = {
        .name   = DRIVER_NAME,
        .owner  = THIS_MODULE,
    },
    .probe      = vdm_probe,
    .remove     = vdm_remove,
    .suspend    = NULL,
    .resume     = NULL,
};


#define MAX_DDR_SIZE   (_256MB_ - _16MB_)

/* function : vdm_init
 *
 *
 *
 */
static int vdm_init(void) 
{
	int ret = 0;
	
#ifdef CONFIG_OF
	struct device_node *np=NULL;
	struct resource res;
#endif

	loops_per_jiffy_for_ndelay = loops_per_jiffy / 1000;
	printk("vdm : loops for ndelay %lu  \n",loops_per_jiffy_for_ndelay);

	 /*-----------------------------------------------------------------------------------------------------*/
    /* Register the platform device driver.                                                                */
    /*-----------------------------------------------------------------------------------------------------*/
    if ((ret=platform_driver_register(&vdm_driver)))
    {
        pr_debug(KERN_ERR DRIVER_NAME " platform_driver_register error, rc=%d\n", ret);
        return ret;
    }

    /*-----------------------------------------------------------------------------------------------------*/
    /* Register the platform device.                                                                       */
    /*-----------------------------------------------------------------------------------------------------*/
    if ((ret=platform_device_register(&vdm_device)))
    {
        pr_debug(KERN_ERR DRIVER_NAME " platform_device_register error, rc=%d\n", ret);
        goto device_regestry_failed ;
    }

    /*-----------------------------------------------------------------------------------------------------*/
    /* init device                                                                                    */
    /*-----------------------------------------------------------------------------------------------------*/

	if (majornum <= 0)
	{
		/* allocate a device number */
		ret = alloc_chrdev_region(&vdm_dev, 0, 1, "vdm");
		majornum = MAJOR(vdm_dev);
	}
	else
	{
		vdm_dev = MKDEV(majornum, 0);
		ret = register_chrdev_region(vdm_dev, 1, "vdm");
	}

	printk(" vdm : mknod /dev/npcmx50_vdm c %d 0\n", MAJOR(vdm_dev));
	if (ret)
	{
		pr_debug(" vdm : Registering the character device failed with %d\n", MAJOR(vdm_dev));
		goto alloc_chrdev_region_failed;
	}

	/* Add a char device */
	vdm_cdev = cdev_alloc();
	vdm_cdev->owner = THIS_MODULE;
	vdm_cdev->ops = &vdm_fops;

	cdev_add(vdm_cdev, vdm_dev, 1);
	if(NULL==vdm_pdev)
	{
		pr_debug( "<1>  vdm : vdm_pdev=NULL \n" );
		ret = -EINVAL;
		goto char_dev_add_failed ;
	}
	
#ifdef CONFIG_OF
	np = of_find_matching_node(NULL, vdm_dt_npcm750_match);
	if (np==NULL){
		 	pr_info("Failed to find of_find_matching_node\n");
            }

	ret = of_address_to_resource(np, 0, &res);
	if (ret) {
		pr_info("\t\t\t of_address_to_resource fail ret %d \n",ret);
		return -EINVAL;
	}

	vdm_virt_addr = ioremap(res.start, resource_size(&res));
	
	if (!vdm_virt_addr) {
		pr_info("\t\t\t vdm_virt_addr fail \n");
		return -ENOMEM;
	}
	printk(KERN_INFO "\t\t\t* VDM base is 0x%08X ,res.start 0x%08X \n", (u32)vdm_virt_addr,res.start);
	
	ret = of_address_to_resource(np, 1, &res);
	if (ret) {
		pr_info("\t\t\t of_address_to_resource fail ret %d \n",ret);
		return -EINVAL;
	}
	
	vdma_virt_addr = ioremap(res.start, resource_size(&res));
	
	if (!vdma_virt_addr) {
		pr_info("\t\t\t vdma_virt_addr fail \n");
		return -ENOMEM;
	}
	
	printk(KERN_INFO "\t\t\t* VDM base is 0x%08X ,res.start 0x%08X \n", (u32)vdma_virt_addr,res.start);
#endif

	vdma_buff_virt_addr = dma_alloc_coherent(&vdm_pdev->dev, VDMA_BUFF_BYTE_SIZE , (dma_addr_t*)&vdma_buff, GFP_KERNEL);
	printk( "<1>  vdm : dma_alloc_coherent virt_addr=0x%x ,phys_addr=0x%x   \n",
			(unsigned int)vdma_buff_virt_addr, (unsigned int)vdma_buff);
	
	if ((NULL==vdma_buff_virt_addr) || (NULL==vdma_buff))
	{
		pr_debug( "<1>  vdm : vdma_buff_virt_addr or  vdma_buff allocation failed \n" );
		ret = -EINVAL;
		goto dma_alloc_coherent_failed ;
	}

	spin_lock_init(&lock);

	ret = _vdm_open();
	if (0 != ret)
	{
        pr_debug(KERN_ERR DRIVER_NAME "_vdm_open error, rc=%d\n", ret);
        goto vdm_open_failed;
	}

#ifdef _USE_VDMA_POLLING
	my_workqueue = create_workqueue(MY_WORK_QUEUE_NAME);
	queue_delayed_work(my_workqueue, &Task, 1000);
	pr_debug("<1> vdma polling is used   \n");
#else

    /*-----------------------------------------------------------------------------------------------------*/
    /* Init interrupts.                                                                                      */
    /*-----------------------------------------------------------------------------------------------------*/
#ifdef CONFIG_MACH_NPCM750

#ifdef CONFIG_OF
		vdma_interrupt = irq_of_parse_and_map(np, 0);
	if (!vdma_interrupt) {
		printk(KERN_ERR "%s - failed to map irq\n", __FUNCTION__);
		ret = -1;
		goto request_irq_failed;
	}
#endif
	ret = request_irq(NPCMX50_VDMA_INTERRUPT,
#elif defined CONFIG_MACH_NPCM650
	ret = request_irq(UART0_INT/* shared with vdma module*/,
#endif
								   vdma_isr,
								   IRQF_SHARED ,
								   "VDMA",
								   (void *) &dummy_vdma_dev);
	if (0 != ret)
	{
	    pr_debug( "<1>  vdma : request_irq result  =%d  \n",ret);
        goto request_irq_failed;
	}


//	SET_REG_BIT(AIC_GEN_REG, AIC_GEN_MTCP);
//	result = request_irq(MCTP_INT,
//								   (void *) vdm_isr,
//								   IRQF_SAMPLE_RANDOM ,
//								   "VDMX",
//								   (void *) NULL);
//	pr_debug("<1> vdma request_irq return = %d \n",result);
#endif

	vdm_class = class_create(THIS_MODULE, "vdm");
	if (IS_ERR(vdm_class))
	{
	    pr_debug( "<1>  vdma :class_create failed  \n");
        goto class_create_failed;
	}

	vdm_sysfs_device  =device_create(vdm_class, NULL, vdm_dev, NULL, "vdm");
	if (IS_ERR(vdm_sysfs_device))
	{
	    pr_debug( "<1>  vdma :device_create failed  \n");
        goto device_create_failed;
	}
	pr_debug("<1> vdm module init done\n");
	return 0;

device_create_failed:
	class_destroy(vdm_class);
class_create_failed:
#ifndef _USE_VDMA_POLLING
#if defined CONFIG_MACH_NPCM750
	free_irq(NPCMX50_VDMA_INTERRUPT,(void *) &dummy_vdma_dev);
#elif defined CONFIG_MACH_NPCM650
	free_irq(UART0_INT,(void *) &dummy_vdma_dev);
#endif
#endif
request_irq_failed :
	_vdm_close();
vdm_open_failed :
	dma_free_coherent(&vdm_pdev->dev, VDMA_BUFF_BYTE_SIZE , vdma_buff_virt_addr , (dma_addr_t)vdma_buff);
dma_alloc_coherent_failed :
	cdev_del(vdm_cdev);
char_dev_add_failed :
	unregister_chrdev_region(vdm_dev, 1);
alloc_chrdev_region_failed :
	platform_device_unregister(&vdm_device);
device_regestry_failed :
	platform_device_put(&vdm_device);
	platform_driver_unregister(&vdm_driver);

	return ret;
}



/* function : vdm_exit
 *
 *
 *
 */
static void vdm_exit(void) 
{

	struct list_head *pList;
	vdm_instance_t *pVDM_Instance;
	unsigned long flags;




	spin_lock_irqsave(&lock,   flags);

	device_destroy(vdm_class , vdm_dev);
	class_destroy(vdm_class);

	reset_detect_poll = 0;
	stop_reset_poll = 1;
	spin_unlock_irqrestore(&lock,   flags);
	cancel_delayed_work(&reset_poll_work);
	flush_scheduled_work();	/* wait till polling work finished */


#ifdef _USE_VDMA_POLLING
	stop_task = 1;		/* keep intrp_routine from queueing itself */
	cancel_delayed_work(&Task);	/* no "new ones" */
	flush_workqueue(my_workqueue);	/* wait till all "old ones" finished */
	destroy_workqueue(my_workqueue);
#endif

	spin_lock_irqsave(&lock,   flags);

	//	free_irq(MCTP_INT, (void *)&dummy_vdma_dev);
#ifndef _USE_VDMA_POLLING
#ifdef CONFIG_MACH_NPCM750
	free_irq(NPCMX50_VDMA_INTERRUPT,(void *) &dummy_vdma_dev);
#elif defined CONFIG_MACH_NPCM650
	free_irq(UART0_INT,(void *) &dummy_vdma_dev);
#endif
#endif

	_vdm_close();

	// clear list of vdm_instances
	list_for_each(pList, &vdm_instances_list)
	{
		pVDM_Instance = list_entry(pList, vdm_instance_t, list);
		kfree(pVDM_Instance->mptxBuffer);
		kfree(pVDM_Instance->mprxBuffer);
		list_del ( &pVDM_Instance->list ) ;
		kfree(pVDM_Instance);
	}

	vdm_exit_common();




	dma_free_coherent(&vdm_pdev->dev, VDMA_BUFF_BYTE_SIZE , vdma_buff_virt_addr , (dma_addr_t)vdma_buff);	

	/* release the char device */
	cdev_del(vdm_cdev);

	/* release the device number */
	unregister_chrdev_region(vdm_dev, 1);

    platform_device_unregister(&vdm_device);
    platform_driver_unregister(&vdm_driver);

	spin_unlock_irqrestore(&lock,   flags);

	pr_debug("<1> vdm module exit\n");

}




module_init(vdm_init);
module_exit(vdm_exit);
MODULE_DESCRIPTION("AESS VDM Driver");
MODULE_LICENSE("GPL v2");
