/**
 *
 *<center>
 *               Avocent Corporation. Proprietary Information.
 * \n<em>
 *      This software is supplied under the terms of a license agreement or
 *      nondisclosure agreement with Avocent Corporation, or one of its 
 *      affiliates, andaffiliates, and may not be used, disseminated, or 
 *      distributed except in accordance with the terms of that agreement.
 *
 *      3541 Gateway Boulevard Fremont, San Jose, CA 94538 U.S.A.
 *\n
 *                  US phone: 510-771-6100
 *
 *        Copyright &copy; 2001-2009 Avocent Corporation. All rights reserved.
 *</em> </center>
 *
 *----------------------------------------------------------------------------\n
 *  MODULES     kernel include file (Driver debug message handler)\n
 *----------------------------------------------------------------------------\n
 *  @file   aess_debugfs.h
 *  @brief  Provide debugging meschanism for kernel driver
 *
 *  @internal
 */
 
/******************************************************************************
* Content
* ----------------
*   aess_debugfs_chkbufflg()      - If DEBUG_CLRBUF_MSK is set, then clear S_sDbgInfo.au8debugbuf buffer
*   aess_debugfs_read()           - This is a callback function when user read outmessage file in user space
*                                   This function will output debug message to user space.
*   aess_debugfs_sprintf()        - Put debug message in a buffer.
*   aess_debugfs_default_create() - Create drivername folder in /sys/kernel/deubg.
*                                   Create ctrlflag file in /sys/kernel/debug/$DriverName             
*                                   Create outmessage file in /sys/kernel/debug/$DriverName
*   aess_debugfs_create_file()    - Create $Filename file in /sys/kernel/debug/$DriverName, where $Filename is defined by user
*   aess_debugfs_remove()         - Always return REMOVE_SUCCESSFUL
******************************************************************************/
 

#ifdef CONFIG_DEBUG_FS

/******************************************************************************
 * Header File
 *****************************************************************************/
#include <linux/debugfs.h>
#include <linux/spinlock.h>
/******************************************************************************
 * Constant value definitions
 *****************************************************************************/
#define DEBUGFS_CTRL_FLAG     "ctrlflag"
#define DEBUGFS_OUT_MSG       "outmessage"
#define GET_DEBUG_LEVEL(A,B,C) ((A & 0x0f) >= B && (A & C))
#define DEBUG_URGENT_LEVEL             1
#define DEBUG_ERROR_LEVEL              2
#define DEBUG_WARN_LEVEL               3
#define DEBUG_INFO_LEVEL               4
#define DEBUG_MSG_LEVEL                5
#define DEBUG_URGENT_BITMASK           0x01000000
#define DEBUG_ERROR_BITMASK            0x02000000
#define DEBUG_WARN_BITMASK             0x04000000
#define DEBUG_INFO_BITMASK             0x08000000
#define DEBUG_MSG_BITMASK             0x10000000
#define DEBUG_OUTLOC_MSK               0x70
#define DEBUG_OUTLOC_FILE              0x00
#define DEBUG_OUTLOC_CONSOLE           0x10
#define DEBUG_CLRBUF_MSK               0x80
#define DEBUG_FILE_SIZE               (4096) /*Fix buffer size as 4KB*/
#define DEBUG_CTRLFLAG_NO             (64)   /*The number of control flag*/
#define DRIVER_NAME_LENGTH            (32) 
#define DBG_TYPE64                      3
#define DBG_TYPE32                      2
#define DBG_TYPE16                      1
#define DBG_TYPE8                       0
#define DEFAULT_CREATE_SUCCESSFUL       0
#define CREATE_FILE_SUCCESSFUL          0
#define REMOVE_SUCCESSFUL               0



/******************************************************************************
*   STRUCT       :  sDrvDbgInfo
******************************************************************************/
/**
 *  @brief   Contains debugging message and flags
 *
 *****************************************************************************/
typedef struct 
{
    /**Point to the entry folder of a driver.*/
    struct dentry *psDriverDir;

    /**Point to the file in user space. The file is mapped to a variable in a dirver.*/
    struct dentry *apsCtrlFlag[DEBUG_CTRLFLAG_NO];

    /**Point to the file in user space. The file is mapped to au8debugbuf in a driver */
    struct dentry *psOutMsg;

    /**Private data of a file. It is not used currently.*/
    s32 s32PrivateData;

    /**The corresponding file of this variable in user space is ctrlflag. User can use this file to control debug message */
    u32 u32CtrlFlag;

    /**The current size in au8debugbuf .*/
    u32 u32BufSize;

    /**Record the name of driver.*/
    u8 au8DrvName[DRIVER_NAME_LENGTH];

    /**Record the debug message, if the output location is a file.*/
    u8 au8debugbuf[DEBUG_FILE_SIZE];

    /**Record the number of files created in user space.*/
    u8 u8CtrlFlagNo;
} sDrvDbgInfo;

static sDrvDbgInfo S_sDbgInfo;

static spinlock_t S_debugfs_spinlock = __SPIN_LOCK_UNLOCKED(S_debugfs_spinlock);
/******************************************************************************
*                   Function Prototype Declaration
******************************************************************************/
static int aess_debugfs_sprintf(char *buf, const char *fmt, ...);

/******************************************************************************
 *                   Macro Function definitions
 *****************************************************************************/
#define DRV_URGENT(fmt, args...) \
 if (GET_DEBUG_LEVEL(S_sDbgInfo.u32CtrlFlag, DEBUG_URGENT_LEVEL,DEBUG_URGENT_BITMASK) && (DEBUG_OUTLOC_FILE ==(S_sDbgInfo.u32CtrlFlag&DEBUG_OUTLOC_MSK))) \
    aess_debugfs_sprintf(S_sDbgInfo.au8debugbuf, "Urg:"fmt, ##args); \
 else if (GET_DEBUG_LEVEL(S_sDbgInfo.u32CtrlFlag, DEBUG_URGENT_LEVEL,DEBUG_URGENT_BITMASK) && (DEBUG_OUTLOC_CONSOLE ==(S_sDbgInfo.u32CtrlFlag&DEBUG_OUTLOC_MSK))) \
   printk(KERN_EMERG "[%s] Urg:" fmt, S_sDbgInfo.au8DrvName, ##args);

#define DRV_ERROR(fmt, args...) \
 if (GET_DEBUG_LEVEL(S_sDbgInfo.u32CtrlFlag,DEBUG_ERROR_LEVEL,DEBUG_ERROR_BITMASK)&& (DEBUG_OUTLOC_FILE ==(S_sDbgInfo.u32CtrlFlag&DEBUG_OUTLOC_MSK))) \
      aess_debugfs_sprintf(S_sDbgInfo.au8debugbuf, "Err: "fmt, ##args);\
 else if (GET_DEBUG_LEVEL(S_sDbgInfo.u32CtrlFlag,DEBUG_ERROR_LEVEL,DEBUG_ERROR_BITMASK) && (DEBUG_OUTLOC_CONSOLE ==(S_sDbgInfo.u32CtrlFlag&DEBUG_OUTLOC_MSK)))  \
    printk(KERN_EMERG "[%s] Err: " fmt, S_sDbgInfo.au8DrvName, ##args);

#define DRV_WARN(fmt, args...) \
    if (GET_DEBUG_LEVEL(S_sDbgInfo.u32CtrlFlag,DEBUG_WARN_LEVEL,DEBUG_WARN_BITMASK)&& (DEBUG_OUTLOC_FILE ==(S_sDbgInfo.u32CtrlFlag&DEBUG_OUTLOC_MSK))) \
      aess_debugfs_sprintf(S_sDbgInfo.au8debugbuf, "Warn: "fmt, ##args);\
    else if (GET_DEBUG_LEVEL(S_sDbgInfo.u32CtrlFlag,DEBUG_WARN_LEVEL,DEBUG_WARN_BITMASK) && (DEBUG_OUTLOC_CONSOLE ==(S_sDbgInfo.u32CtrlFlag&DEBUG_OUTLOC_MSK)))  \
     printk(KERN_EMERG "[%s] Warn: " fmt, S_sDbgInfo.au8DrvName, ##args);

#define DRV_INFO(fmt, args...) \
    if (GET_DEBUG_LEVEL(S_sDbgInfo.u32CtrlFlag,DEBUG_INFO_LEVEL,DEBUG_INFO_BITMASK)&& (DEBUG_OUTLOC_FILE ==(S_sDbgInfo.u32CtrlFlag&DEBUG_OUTLOC_MSK))) \
        aess_debugfs_sprintf(S_sDbgInfo.au8debugbuf, "Info: "fmt, ##args);\
    else if (GET_DEBUG_LEVEL(S_sDbgInfo.u32CtrlFlag,DEBUG_INFO_LEVEL,DEBUG_INFO_BITMASK) && (DEBUG_OUTLOC_CONSOLE ==(S_sDbgInfo.u32CtrlFlag&DEBUG_OUTLOC_MSK))) \
     printk(KERN_EMERG "[%s] Info: " fmt, S_sDbgInfo.au8DrvName, ##args);

#define DRV_MSG(fmt, args...) \
    if (GET_DEBUG_LEVEL(S_sDbgInfo.u32CtrlFlag, DEBUG_MSG_LEVEL,DEBUG_MSG_BITMASK )&& (DEBUG_OUTLOC_FILE ==(S_sDbgInfo.u32CtrlFlag&DEBUG_OUTLOC_MSK))) \
        aess_debugfs_sprintf(S_sDbgInfo.au8debugbuf, "Msg: "fmt, ##args);\
    else if (GET_DEBUG_LEVEL(S_sDbgInfo.u32CtrlFlag, DEBUG_MSG_LEVEL,DEBUG_MSG_BITMASK )&& (DEBUG_OUTLOC_CONSOLE ==(S_sDbgInfo.u32CtrlFlag&DEBUG_OUTLOC_MSK))) \
        printk(KERN_EMERG"[%s] Msg: " fmt, S_sDbgInfo.au8DrvName, ##args);

/******************************************************************************
*   FUNCTION        :   aess_debugfs_chkbufflg
******************************************************************************/
/**
 *  @brief      If DEBUG_CLRBUF_MSK is set, then clear S_sDbgInfo.au8debugbuf buffer
 *
 *  @return     None
 *
 *  @dependency None
 *
 *  @limitation None
 *
 *  @warning    None
 *
 *  @note       None
 *
 *  @internal   Function Type: Static function\n
 *
 *****************************************************************************/
   
static void aess_debugfs_chkbufflg(void)
{  
    unsigned long u32Flag = 0;
    spin_lock_irqsave(&S_debugfs_spinlock, u32Flag);
    if(S_sDbgInfo.u32CtrlFlag & DEBUG_CLRBUF_MSK)
    {
        /** Clear buffer*/
        memset(S_sDbgInfo.au8debugbuf,0,DEBUG_FILE_SIZE);

        /** Clear CLRBUF bit in CtrlFlag*/
        S_sDbgInfo.u32CtrlFlag &= (u32)(~DEBUG_CLRBUF_MSK);
        
        /** Clear buffer size*/
        S_sDbgInfo.u32BufSize = 0;
    }
    spin_unlock_irqrestore(&S_debugfs_spinlock, u32Flag);
}


/******************************************************************************
*   FUNCTION        :   aess_debugfs_read
******************************************************************************/
/**
 *  @brief      This is a callback function when user read outmessage file in user space.
 *              This function will output debug message to user space.
 *
 *  @return     Size of data in a buffer
 *
 *  @dependency None
 *
 *  @limitation None
 *
 *  @warning    None
 *
 *  @note       None
 *
 *  @internal   Function Type: Static function\n
 *
 *****************************************************************************/
static ssize_t aess_debugfs_read(
                                /** File pointer*/
                                struct file *file, 
                                
                                /** Buffer send to userspace*/
                                char __user *userbuf,   
                                
                                /** Copied size*/
                                size_t count, 
                                
                                /** position pointer in a file*/
                                loff_t *ppos
                                )                       
{                                                                                 
 
    aess_debugfs_chkbufflg();                                                       
	
    /**Copy data from S_sDbgInfo.au8debugbuf to userbuf*/
    return simple_read_from_buffer(userbuf, count, ppos, S_sDbgInfo.au8debugbuf, S_sDbgInfo.u32BufSize);      
}                                                                                 

static const struct file_operations debugfs_fops = {                      
        .read = aess_debugfs_read                                         
};   


/******************************************************************************
*   FUNCTION        :   aess_debugfs_sprintf
******************************************************************************/
/**
 *  @brief      Put debug message in a buffer.
 *
 *  @return     Size of data which is put into debug message buffer
 *
 *  @dependency None
 *
 *  @limitation None
 *
 *  @warning    None
 *
 *  @note       None
 *
 *  @internal   Function Type: Static function\n
 *
 *****************************************************************************/
static int aess_debugfs_sprintf(
                                 /** The buffer to place the debug message into*/
                                 char *buf, 
                                 
                                 /** The format string to use*/
                                 const char *fmt, 
                                 
                                 /** Arguments for the format string*/
                                 ...
                                 )
{
    unsigned int u16RemainingSize = 0;
    va_list args;
    int i =0;
    unsigned long u32Flag = 0;
    
    aess_debugfs_chkbufflg();
    
    spin_lock_irqsave(&S_debugfs_spinlock, u32Flag);
    if(S_sDbgInfo.u32BufSize < DEBUG_FILE_SIZE)
    {
        u16RemainingSize = DEBUG_FILE_SIZE - S_sDbgInfo.u32BufSize;
        va_start(args, fmt);
        i = vsnprintf(&buf[S_sDbgInfo.u32BufSize],u16RemainingSize ,fmt, args);
        va_end(args);
        if (i >= 0)
            S_sDbgInfo.u32BufSize += i;
	   
        if (S_sDbgInfo.u32BufSize >= DEBUG_FILE_SIZE)
        {
            buf[(DEBUG_FILE_SIZE-1)] ='\n';
            S_sDbgInfo.u32BufSize = 0;
        }
    }
    spin_unlock_irqrestore(&S_debugfs_spinlock, u32Flag);
    return i;
}
/******************************************************************************
*   FUNCTION        :   aess_debugfs_create_file
******************************************************************************/
/**
 *  @brief      Create $Filename file in /sys/kernel/debug/$DriverName, where $Filename is defined by user
 *
 *  @return     CREATE_FILE_SUCCESSFUL for success, -ENODEV or -ENOMEM for failure.
 *
 *  @dependency None
 *
 *  @limitation None
 *
 *  @warning    None
 *
 *  @note       None
 *
 *  @internal   Function Type: Static function\n
 *
 *****************************************************************************/

int aess_debugfs_create_file(
                                    /**Name of driver*/
                                    char *u8pDrvName,
                                    
                                    /**Name of created file*/
                                    char *u8pFileName,
                                    
                                    /**A pointer to the variable that the file should read to and write from*/
                                    void *pCtrFlag,
                                    
                                    /**Data type*/
                                    u32 u32Type,
                                    
                                    /**Reserved for future used*/
                                    u32 u32Reserved2
                                    )
{
    unsigned char u8Index = 0; 
    u8Index = S_sDbgInfo.u8CtrlFlagNo;
    if (S_sDbgInfo.u8CtrlFlagNo >= DEBUG_CTRLFLAG_NO)
    {
        printk(KERN_EMERG"The created ctrl flag is larger than the maximum value.\n");
        return -ENOMEM;
    }
	
    switch (u32Type)
    {
        case DBG_TYPE8:
		        S_sDbgInfo.apsCtrlFlag[u8Index] = debugfs_create_u8(u8pFileName, 0666, S_sDbgInfo.psDriverDir,(u8*)pCtrFlag);
		        break;
		
        case DBG_TYPE16:
		        S_sDbgInfo.apsCtrlFlag[u8Index] = debugfs_create_u16(u8pFileName, 0666, S_sDbgInfo.psDriverDir,(u16*)pCtrFlag);
            break;
		
        case DBG_TYPE32:
            S_sDbgInfo.apsCtrlFlag[u8Index] = debugfs_create_u32(u8pFileName, 0666, S_sDbgInfo.psDriverDir,(u32*)pCtrFlag);
            break;
		
        case DBG_TYPE64:
		
            /** Kernel 2.6.18 doesn't support debugfs_create_u64() function*/
			      S_sDbgInfo.apsCtrlFlag[u8Index] = debugfs_create_u64(u8pFileName, 0666, S_sDbgInfo.psDriverDir,(u64*)pCtrFlag);
		        break	;	  
        default:
            return -ENODEV;
		        break;      
    }

    if (!(S_sDbgInfo.apsCtrlFlag[u8Index])) 
    {
        printk(KERN_EMERG"Fail to create %s file\n",u8pFileName);
        return -ENODEV;
    }

    S_sDbgInfo.u8CtrlFlagNo++;
    return CREATE_FILE_SUCCESSFUL;
}

/******************************************************************************
*   FUNCTION        :   aess_debugfs_default_create
******************************************************************************/
/**
 *  @brief      Create drivername folder in /sys/kernel/deubg.
 *              Create ctrlflag file in /sys/kernel/debug/$DriverName             
 *              Create outmessage file in /sys/kernel/debug/$DriverName
 *  @return     DEFAULT_CREATE_SUCCESSFUL for success, -ENODEV for failure.
 *
 *  @dependency None
 *
 *  @limitation None
 *
 *  @warning    None
 *
 *  @note       None
 *
 *  @internal   Function Type: Static function\n
 *
 *****************************************************************************/
int aess_debugfs_default_create(
                                       /**Name of driver*/
                                       char *u8pDrvName,
                                       
                                       /**Reserved for future used*/
                                       void *pReserved,
                                       
                                       /**Reserved for future used*/
                                       u32 u32Reserved1,
                                       
                                       /**Reserved for future used*/
                                       u32 u32Reserved2
                                       )
{
   
    u8 u8i=0,u8len=0,u8Ret = 0;
    u8len = strlen(u8pDrvName);

    if(u8len > DRIVER_NAME_LENGTH)
    {
        printk(KERN_EMERG"The length of %s is larger than %d.\n",u8pDrvName,DRIVER_NAME_LENGTH);	
        return -ENODEV;
    }
    
    for(u8i=0;u8i<u8len;u8i++)
    {
        S_sDbgInfo.au8DrvName[u8i] = u8pDrvName[u8i];
    }
    
    S_sDbgInfo.u8CtrlFlagNo = 0;
  
    /** Create drivername folder in /sys/kernel/deubg */
	  S_sDbgInfo.psDriverDir = debugfs_create_dir(u8pDrvName, NULL);
	  if (!S_sDbgInfo.psDriverDir) 
	  {
		    printk(KERN_EMERG"Fail to create %s dir.\n",u8pDrvName);
		    return -ENODEV;
	  }
	
	  /** Create ctrlflag file in /sys/kernel/debug/$DriverName, file name is defined in  DEBUGFS_CTRL_FLAG*/
	  u8Ret = aess_debugfs_create_file(u8pDrvName,DEBUGFS_CTRL_FLAG,(u32 *)&S_sDbgInfo.u32CtrlFlag,DBG_TYPE32,0);
	  
	  if ( CREATE_FILE_SUCCESSFUL != u8Ret) 
	  {
		   printk(KERN_EMERG"Fail to create %s file\n",DEBUGFS_CTRL_FLAG);
		   return -ENODEV;
	  }
	  
	  /** Create outmessage file in /sys/kernel/debug/$DriverName */ 
	  S_sDbgInfo.psOutMsg = debugfs_create_file(DEBUGFS_OUT_MSG, 0444, S_sDbgInfo.psDriverDir, &S_sDbgInfo.s32PrivateData, &debugfs_fops);
	  if (!S_sDbgInfo.psOutMsg) 
	  {
		   printk(KERN_EMERG"Fail to create %s file\n",DEBUGFS_OUT_MSG);
		   return -ENODEV;
	  }
	  return DEFAULT_CREATE_SUCCESSFUL;
}




/******************************************************************************
*   FUNCTION        :   aess_debugfs_remove
******************************************************************************/
/**
 *  @brief      Remove all created folder and files in userspace.
 *
 *  @return     Always return REMOVE_SUCCESSFUL
 *
 *  @dependency None
 *
 *  @limitation None
 *
 *  @warning    None
 *
 *  @note       None
 *
 *  @internal   Function Type: Static function\n
 *
 *****************************************************************************/
static int aess_debugfs_remove(
                                /**Name of Driver*/
                                char *u8pDrvName,
                                
                                /**Reserved for future used*/
                                void *pReserved,
                                
                                /**Reserved for future used*/
                                u32 u32Reserved1,
                                
                                /**Reserved for future used*/
                                u32 u32Reserved2
                                )
{
    u8 i =0;
    printk(KERN_EMERG"The number of removed file is %d\n",S_sDbgInfo.u8CtrlFlagNo);
    for(i=0;i<S_sDbgInfo.u8CtrlFlagNo;i++)
    {
        debugfs_remove(S_sDbgInfo.apsCtrlFlag[i]);	
    }
    debugfs_remove(S_sDbgInfo.psOutMsg);
    debugfs_remove(S_sDbgInfo.psDriverDir);
	
	return REMOVE_SUCCESSFUL;
}
#else
/******************************************************************************
*                   Macro Function Definition
******************************************************************************/
#define GET_DEBUG_LEVEL(A,B,C) 0
#define DRV_URGENT(fmt, args...) 
#define DRV_ERROR(fmt, args...)
#define DRV_WARN(fmt, args...)
#define DRV_INFO(fmt, args...)
#define DRV_MSG(fmt, args...)

#endif
/* End of code */
