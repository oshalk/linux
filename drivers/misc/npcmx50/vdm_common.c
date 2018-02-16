/********************************************************
 *
 * file : vdm_common.c
 *
 *
 ****************************************************/

#include <linux/types.h> /* size_t */
#include <linux/delay.h>
#include <mach/hal.h>
#define  PRINTF printk

extern int vdm_virt_addr;
extern int vdma_virt_addr;

extern void npcmx50_spin_lock_irqsave(unsigned long *flags);
extern void npcmx50_spin_unlock_irqrestore(unsigned long flags);

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>

#undef NPCMX50_VDM_BASE_ADDR
#undef NPCMX50_VDMA_BASE_ADDR
#define NPCMX50_VDM_BASE_ADDR vdm_virt_addr
#define NPCMX50_VDMA_BASE_ADDR vdma_virt_addr
#endif

#include "vdm_common.h"
#include "vdm_api.h"
#include "CircularBuffer.h"


#define ADD_BIT(pos)   (1<<pos)

uint32_t vdma_rx_buff_size_int,*vdma_rx_buff,*vdma_rx_buff_virt_addr;

static uint8_t ready_for_transmit=0;


extern void nano_delay(uint32_t nsec);


/* function : vdma_is_data_ready
 *
 *
 *
 */
VDMA_STATUS_t vdma_is_data_ready(void)
{
	uint32_t regVal = REG_READ(VDMA_ECTL_REG );
	VDMA_STATUS_t retVal;
	//clear flags :

	if( regVal & ( 1 << VDMA_ECTL_REG_DRDY_BIT_POS ) )
	{
		regVal |= ( 1 << VDMA_ECTL_REG_DRDY_BIT_POS );
		if( regVal & ( 1 << VDMA_ECTL_REG_HALT_BIT_POS ) )
		{
			regVal |= ( 1 << VDMA_ECTL_REG_HALT_BIT_POS );
			retVal = VDMA_STATUS_DATA_READY_WITH_OVERFLOWED;
		}
		else
		{
			retVal = VDMA_STATUS_DATA_READY;
		}
	}
	else
	{
		if( regVal & ( 1 << VDMA_ECTL_REG_HALT_BIT_POS ) )
		{
			regVal |= ( 1 << VDMA_ECTL_REG_HALT_BIT_POS );
			retVal = VDMA_STATUS_OVERFLOWED;
		}
		else
		{
			retVal = VDMA_STATUS_NONE;
		}

	}

	REG_WRITE(VDMA_ECTL_REG  , regVal  );

	return retVal;

}

 /* function : test_buffer_state
*
*
*
*/
uint32_t get_buffer_state(uint32_t *pCurrBuffReadPos,uint32_t *pCurrBuffWritePos)
{
	uint32_t numOfElementsInBuf ;

	*pCurrBuffWritePos =  (REG_READ(VDMA_CDST_REG) - (uint32_t)vdma_rx_buff) >> 2; // we are working with uint32_t
	*pCurrBuffReadPos =  	(REG_READ(VDMA_ERDPNT_REG) - (uint32_t)vdma_rx_buff) >> 2; // we are working with uint32_t

	if(*pCurrBuffWritePos >= *pCurrBuffReadPos)
	{
		numOfElementsInBuf=*pCurrBuffWritePos - *pCurrBuffReadPos;
	}
	else
	{
		numOfElementsInBuf=vdma_rx_buff_size_int - (*pCurrBuffReadPos - *pCurrBuffWritePos)  ;
	}

	return numOfElementsInBuf;
}

/* function : vdma_copy_packets_from_buffer
 *
 *
 *
 */
void __vdma_copy_packets_from_buffer(receive_packet_func_t receive_packet_func , uint8_t bufferWasOverflowed)
{
	uint32_t numOfElementsInBuf ;
	uint32_t testReadPos , lengthOfPacketLeft,lengthOfPacket;
	uint32_t PCIeHeaderUINT32;
	uint32_t num_of_items_to_read_from_buff ;
	uint32_t currBuffReadPos,currBuffWritePos;
	uint32_t BDF;


//	PRINTF("<1>vdm: irq \n");
//	PRINTF("<1>vdm: vdma_rx_buff addr=0x%x \n", (uint32_t)vdma_rx_buff );


//	currBuffWritePos =  (REG_READ(VDMA_CDST_REG) - (uint32_t)vdma_rx_buff) >> 2; // we are working with uint32_t
//	currBuffReadPos =  	(REG_READ(VDMA_ERDPNT_REG) - (uint32_t)vdma_rx_buff) >> 2; // we are working with uint32_t
//
//	if(currBuffWritePos >= currBuffReadPos)
//	{
//		numOfElementsInBuf=currBuffWritePos-currBuffReadPos;
//	}
//	else
//	{
//		numOfElementsInBuf=vdma_rx_buff_size_int - (currBuffReadPos - currBuffWritePos)  ;
//	}

	numOfElementsInBuf = get_buffer_state(&currBuffReadPos,&currBuffWritePos);

#if 1

	while (numOfElementsInBuf >= PCIe_MSG_HEADER_SIZE_INT)
	{
		int wasMisReception;
		//PRINTF("<1>vdm: currBuffReadPos = 0x%x \n", (uint32_t)currBuffReadPos );

		// check if find next vdm packet
		lengthOfPacket = 0;
		testReadPos=(currBuffReadPos + 2) % vdma_rx_buff_size_int; // third UINT32 in packet includes vdm number
		PCIeHeaderUINT32=vdma_rx_buff_virt_addr[testReadPos];
		wasMisReception=0 ;
		while((PCIeHeaderUINT32 & VDM_VENDOR_ID_MASK_LSB_IN_U32) != VDM_VENDOR_ID_LSB_IN_U32)
		{
			if(0==wasMisReception)
			{
				PRINTF("<1>vdm: rx err addr=0x%x \n", (uint32_t)vdma_rx_buff + (currBuffReadPos<<2));
			}
			wasMisReception=1;
//			numOfElementsInBuf--;

			currBuffReadPos = (currBuffReadPos+1) % vdma_rx_buff_size_int;
			REG_WRITE(VDMA_ERDPNT_REG, (currBuffReadPos << 2) + (uint32_t)vdma_rx_buff );

			numOfElementsInBuf = get_buffer_state(&currBuffReadPos,&currBuffWritePos);

			if(PCIe_MSG_HEADER_SIZE_INT > numOfElementsInBuf)
			{
				break; // buffer is less then header
			}
			testReadPos=(currBuffReadPos + 2) % vdma_rx_buff_size_int;
			PCIeHeaderUINT32=vdma_rx_buff_virt_addr[testReadPos];
		}

//		if(wasMisReception)
//		{
//			PRINTF("<1>vdm: rx err \n");
//		}

		if(numOfElementsInBuf >= PCIe_MSG_HEADER_SIZE_INT) // packet was found
		{
			PCIeHeaderUINT32 = vdma_rx_buff_virt_addr[currBuffReadPos] ;// read first UINT32 in packet
			if((PCIeHeaderUINT32 & PCIe_HEADER_FMT_FIELD_MASK) == PCIe_HEADER_FMT_MSG_NO_PAYLOAD)
			{
				lengthOfPacket=4;
			}
			else
			{
				lengthOfPacket =( (PCIeHeaderUINT32 >> 8) & 0x0300) + (PCIeHeaderUINT32  >> 24) + 4;// 4 for pcie header length
			}

			if( (0 == lengthOfPacket) || (MAX_PACKET_LENGTH < lengthOfPacket) ) //invalid packet length
			{
				currBuffReadPos = (currBuffReadPos+1) % vdma_rx_buff_size_int;
			}
			else
			{
				if(numOfElementsInBuf>=lengthOfPacket)
				{
					BDF=vdma_rx_buff_virt_addr[(currBuffReadPos + 1)  % vdma_rx_buff_size_int ] ;// read second UINT32 in packet
					BDF=((BDF>>8) & 0xff) + ((BDF & 0xff)<<8);

					lengthOfPacketLeft=lengthOfPacket;
					while(lengthOfPacketLeft)
					{
						if(currBuffWritePos>=currBuffReadPos)
						{
							num_of_items_to_read_from_buff=currBuffWritePos-currBuffReadPos;
						}
						else
						{
							num_of_items_to_read_from_buff = vdma_rx_buff_size_int - currBuffReadPos ;
						}

						if (num_of_items_to_read_from_buff > lengthOfPacketLeft)
						{
							num_of_items_to_read_from_buff = lengthOfPacketLeft;
						}

						lengthOfPacketLeft -= num_of_items_to_read_from_buff;

						receive_packet_func(BDF , &vdma_rx_buff_virt_addr[currBuffReadPos] ,
								num_of_items_to_read_from_buff, (0 == lengthOfPacketLeft) ? 1 : 0);

						currBuffReadPos = (currBuffReadPos + num_of_items_to_read_from_buff) % vdma_rx_buff_size_int;

					}
	//				numOfElementsInBuf -= lengthOfPacket;
				}
				else
				{
					break; // no full packet found
				}
			}

			REG_WRITE(VDMA_ERDPNT_REG, (currBuffReadPos << 2) + (uint32_t)vdma_rx_buff );
			numOfElementsInBuf = get_buffer_state(&currBuffReadPos,&currBuffWritePos);

		}//while (numOfElementsInBuf >= PCIe_MSG_HEADER_SIZE_BYTES)
	}
#endif

	// reset buffer
	if(bufferWasOverflowed)
	{
		vdm_init_common(vdma_rx_buff , vdma_rx_buff_virt_addr , vdma_rx_buff_size_int * sizeof(uint32_t) );
	}
}

/* function : vdma_copy_packets_from_buffer
 *
 *
 *
 */
void vdma_copy_packets_from_buffer(receive_packet_func_t receive_packet_func )
{
	__vdma_copy_packets_from_buffer(receive_packet_func,0);
}

/* function : vdma_copy_packets_from_buffer_with_overflow
 *
 *
 *
 */
void vdma_copy_packets_from_buffer_with_overflow(receive_packet_func_t receive_packet_func )
{
	__vdma_copy_packets_from_buffer(receive_packet_func,1);
}

/* function : vdm_SendMessage
 *
 *
 *
 */
int vdm_SendMessage(uint8_t route_type, uint16_t aBDF,uint8_t  *apData,uint32_t aLength)
{
	uint32_t 	i,txData;
    uint8_t     idx;
	uint8_t 	tag,paddingBytesNum;
	uint8_t 	currLength;
	uint32_t pci_message_header[4]={0};

	ready_for_transmit=0;

	pci_message_header[0]=
			// ( ( ((1) << 5)+ ( (0x13/*1 0011 (broadcasting) */) << 0)) << 0) + // fmt + type . filled dynamically
			( ( ( PCIe_TC0 << 4)+  (PCIe_NO_IDO << 2) +  	// TC + atr[2]
					(PCIe_NO_TH << 0) ) << 8   ) + 			// TH
			( ( (PCIe_NO_TD << 7) + (PCIe_NO_EP << 6) +     // TD  + EP
					(PCIe_NO_RLX_ORDER_NO_SNOOP << 4) +     // atr[1,0]
					(PCIe_NO_AT << 2)  ) << 16); 			// AT
			//+ ( (0) << 24) ;// length . filled dynamically

	pci_message_header[1]=
			( (0) << 0) + //  request ID filled automatically by HW
			( ( PCIe_TAG ) << 8) + // defult tag
			( (PCIe_MSG_VDM_TYPE1) << 24 ) ;//message code (vd type1)


	pci_message_header[2]=
			// ( (0) << 0) + // bus_num (reserved in broadcasting) . filled dynamically
			// ( ( (0 << 3) + (0 << 0) ) << 8) + // dev_num + func_num (reserved in broadcasting) . filled dynamically
			( ( (VDM_VENDOR_ID >> 8) & 0xff) << 16) +// vendor id msb
			( ( VDM_VENDOR_ID & 0xff ) << 24 ) ;// vendor id lsb

//	PRINTF("<1>vdm:vdm_SendMessage \n"  );

	// data will be split by PCIe_MAX_PAYLOAD_SIZE_BYTES
	while(aLength)
	{
		if(aLength>PCIe_MAX_PAYLOAD_SIZE_BYTES)
		{
			currLength=PCIe_MAX_PAYLOAD_SIZE_BYTES;
		}
		else
		{
			currLength=aLength;
		}
		aLength-=currLength;

		paddingBytesNum =  ( currLength % 4);
		if ( 0 != paddingBytesNum )
		{
			paddingBytesNum = 4 - paddingBytesNum;
		}

		/*********** fill first U32 of TLP :  ***************/
		pci_message_header[0] &= (~PCIe_HEADER_FMT_FIELD_MASK);
		pci_message_header[0] &= (~PCIe_HEADER_LENGTH_FIELD_MASK);
		if(currLength > 4)
		{
			pci_message_header[0] |= PCIe_HEADER_FMT_MSG_WITH_PAYLOAD;
			pci_message_header[0] |= ((    ((currLength-1)/4) & 0xff) << 24);
			pci_message_header[0] |= ( ((  ((currLength-1)/4) >> 8) & 0x3) << 16);
		}
		else
		{
			pci_message_header[0] |= PCIe_HEADER_FMT_MSG_NO_PAYLOAD;
		}

		/* route_type icludes tlp type and route mechanism*/
		pci_message_header[0] &= (~PCIe_HEADER_ROUTE_FIELD_MASK);
		pci_message_header[0] |= route_type;

		pci_message_header[0] &= (~PCIe_HEADER_ATTR_MASK);
		pci_message_header[0] |= PCIe_HEADER_ATTR_TEST;


		/*********** fill second U32 of TLP :  ***************/
		pci_message_header[1] &= (~PCIe_HEADER_TAG_FIELD_MASK);
		tag = (paddingBytesNum & 3 ) << 4 ;
		pci_message_header[1] |= (tag << 16);


		/*********** fill third U32 of TLP :  ***************/
		pci_message_header[2] &= (~PCIe_HEADER_DEST_BDF_FIELD_MASK);

		if(PCIe_HEADER_ROUTE_BY_ID == route_type)
		{
			pci_message_header[2] |=
					( ( (aBDF >> 8) & 0xff) ) +
					( ( aBDF & 0xff ) << 8 ) ;
		}


		REG_WRITE(VDM_TXF_REG,pci_message_header[0] );
		REG_WRITE(VDM_TXF_REG,pci_message_header[1] );
		REG_WRITE(VDM_TXF_REG,pci_message_header[2] );


		while(currLength >3 )
		{
			txData=(apData[3]<<24) + (apData[2]<<16) + (apData[1]<<8) + (apData[0]) ;
			REG_WRITE(VDM_TXF_REG,txData);
			apData+=4;
			currLength-=4;
		}


		txData=0;
        idx=0;
		while(currLength)
		{
            /*
			txData |=*apData ;
			apData++;
			currLength--;
			txData = txData << 8;
            */
            /*
            txData |= (*apData << ((3 - currLength)*8));
            apData++;
            currLength--;
            */

            txData |= (*apData << idx*8);
            idx++;
            apData++;
            currLength--;
		}
		//paddingBytesNum--;
		//txData = txData << (paddingBytesNum*8);
		REG_WRITE(VDM_TXF_REG,txData);


        //Check if VDM Module is Enabled Before TX START 
        if(READ_REG_BIT(VDM_CNT_REG  , VDM_CNT_REG_VDM_ENABLE_BIT_POS)) 
            SET_REG_BIT(VDM_CNT_REG, VDM_CNT_REG_START_TX_BIT);

//		PRINTF("<1>vdm: pci_message_header[0] = 0x%X \n",pci_message_header[0] );
//		PRINTF("<1>vdm: pci_message_header[1] = 0x%X \n",pci_message_header[1] );
//		PRINTF("<1>vdm: pci_message_header[2] = 0x%X \n",pci_message_header[2] );
//		PRINTF("<1>vdm:txData = 0x%X \n",txData );
//
//		PRINTF("<1>vdm: status reg 0x%X= 0x%X \n",VDM_STAT_REG_ADDR , REG_READ(VDM_STAT_REG) );
//		PRINTF("<1>vdm: cont reg 0x%X = 0x%X \n",VDM_CNT_REG_ADDR,REG_READ(VDM_CNT_REG) );


		// test VDM_CNT_REG_VDM_ENABLE_BIT_POS to see that PCIe bus was not reset
		for (i=0 ; i< SEND_TIMEOUT; i++)
		{
			if((READ_REG_BIT(VDM_STAT_REG  , VDM_TX_DONE_BIT_POS) == 1) || vdm_is_in_reset())
			{
				break ;
			}
			else
			{
				nano_delay(100);
			}

		}

		REG_WRITE(VDM_STAT_REG  , 1 << VDM_TX_DONE_BIT_POS);// clear status
		ready_for_transmit =1;

		if((SEND_TIMEOUT == i) || vdm_is_in_reset())
		{
			return -EIO;
		}

	}

	return 0;


}

/* function : vdm_is_in_reset
 *
 *
 *
 */
uint8_t vdm_is_in_reset(void)
{
	return (0 == READ_REG_BIT(VDM_CNT_REG  , VDM_CNT_REG_VDM_ENABLE_BIT_POS)) ? 1 : 0;
}


/* function : vdm_reset
 *
 *
 *
 */
void vdm_reset(void)
{
	CLEAR_REG_BIT(VDM_CNT_REG  , VDM_ENABLE_FIELD_POS);
	SET_REG_BIT(VDM_CNT_REG  , VDM_ENABLE_FIELD_POS);
}


/* function : vdm_is_ready_for_write
 *
 *
 *
 */
uint8_t vdm_set_timeout(VDM_RX_TIMEOUT_t timeout)
{
	uint32_t regVal;
	if(timeout > VDM_RX_TIMEOUT_8u)
	{
		return 1;
	}

	regVal=REG_READ(VDM_CNT_REG) ;
	regVal &= (~VDM_RX_TIMEOUT_FIELD_MASK);
	regVal |= (timeout << VDM_RX_TIMEOUT_FIELD_POS);
	return 0;
}

/* function : vdm_disable_rx
 *
 *
 *
 */
void vdm_disable_rx(void)
{
	uint32_t reg_val;
	reg_val =  ADD_BIT(VDM_FLT_REG_FLT_ENABLE_BIT_POS) + 0xFFFF;  // filtering
	REG_WRITE(VDM_FLT_REG, reg_val);
}

/* function : vdm_enable_rx
 *
 *
 *
 */
void vdm_enable_rx(void)
{
	uint32_t reg_val;
	reg_val =  ADD_BIT(VDM_FLT_REG_FLT_ENABLE_BIT_POS) + VDM_VENDOR_ID;  // 0xFFFF is invalid vendor ID , so no messages should be received
	REG_WRITE(VDM_FLT_REG, reg_val);
}

/* function : vdm_is_ready_for_write
 *
 *
 *
 */
uint8_t vdm_is_ready_for_write(void)
{
	return ready_for_transmit  ;
}

/* function : vdm_get_errors
 *
 *
 *
 */
uint32_t vdm_get_errors(void)
{
	uint32_t regVal = REG_READ(VDM_STAT_REG );
	uint32_t retVal = 0;
	//clear flags :

	if( regVal & ( 1 << VDM_RX_FULL_BIT_POS ) )
	{
		retVal |= VDM_ERR_FIFO_OVERFLOW;
	}

	return retVal;
}


/* function : vdm_clear_errors
 *
 *
 *
 */
void vdm_clear_errors(uint32_t clear_errors_bitmap)
{
	if(VDM_ERR_FIFO_OVERFLOW | clear_errors_bitmap)
	{
		CLEAR_REG_BIT(VDM_STAT_REG  , VDM_RX_FULL_BIT_POS);
	}
}

/* function : vdm_init_common
 *
 *
 *
 */
int vdm_init_common(uint32_t *apVdma_rx_buff,uint32_t *apVdma_rx_buff_virt_addr,uint32_t buff_bytes_len)
{
	uint32_t reg_val;
	uint8_t buff_size_in_16kb;
	unsigned long flags;

	vdma_rx_buff	=	apVdma_rx_buff;
	vdma_rx_buff_virt_addr = apVdma_rx_buff_virt_addr;

	buff_size_in_16kb =  (buff_bytes_len) / (16384); // calclulate available 16k blocks
	if((0==buff_size_in_16kb) || (255<buff_size_in_16kb))
	{
		return 1;
	}

	vdma_rx_buff_size_int   = (buff_size_in_16kb * (16384))/sizeof(uint32_t);


#ifdef CONFIG_MACH_NPCM750

	npcmx50_spin_lock_irqsave(&flags);

	/****** configure PCIE phy selection for bridge *********/
	/*************************************************/
	CLEAR_REG_BIT(PHY_SELECT_FOR_PCIE_BRIDGE_REG , PHY_SELECT_FOR_PCIE_BRIDGE_FIELD_POS);

	npcmx50_spin_unlock_irqrestore(flags);

#endif

	/****** configure VDM Module *********/
	/*************************************************/
	reg_val = ADD_BIT(VDM_TX_DONE_BIT_POS) +  ADD_BIT(VDM_RX_DONE_BIT_POS) +  ADD_BIT(VDM_RX_FULL_BIT_POS);  // clear status
	REG_WRITE(VDM_STAT_REG, reg_val);

	//reset :
	REG_WRITE(VDM_CNT_REG, 0x0);// disable module
	reg_val =  (VDM_RX_TIMEOUT_8u << VDM_RX_TIMEOUT_FIELD_POS) +
			ADD_BIT(VDM_CNT_REG_VDM_ENABLE_BIT_POS);  //  (8us delay before packet drop) + enabe
	REG_WRITE(VDM_CNT_REG,reg_val);
	// end of reset

	reg_val =  ADD_BIT(VDM_FLT_REG_FLT_ENABLE_BIT_POS) + VDM_VENDOR_ID;  // filtering
	REG_WRITE(VDM_FLT_REG, reg_val);

	reg_val =  ADD_BIT(VDM_INT_EN_REG_RX_INT_BIT_POS) ;  // enable rx int
	REG_WRITE(VDM_INT_EN_REG,reg_val);






	/****** configure VDMA Module *********/
	/*************************************************/
	REG_WRITE(VDMA_CNT_REG, 0x00);// disable

	REG_WRITE(VDMA_SRCB_REG, PHYS_VDM_RXF_REG_ADDR );// src_addr

	REG_WRITE(VDMA_DSTB_REG,  (uint32_t)apVdma_rx_buff );// dst_addr

	REG_WRITE(VDMA_ESRCSZ_REG, PHYS_VDM_STAT_REG_ADDR);	// size_addr
	REG_WRITE(VDMA_ERDPNT_REG, (uint32_t)apVdma_rx_buff );		// read pointer

	//automatic clear vdmx rx done status bit
	REG_WRITE(VDMA_EST0AD_REG, PHYS_VDM_STAT_REG_ADDR );			// address of clear bit
//	REG_WRITE(VDMA_EST0MK_REG, ADD_BIT( VDM_RX_DONE_BIT_POS) );		// mask of bit
	REG_WRITE(VDMA_EST0MK_REG, 0xffffffff );		// mask of bit
	REG_WRITE(VDMA_EST0DT_REG, ADD_BIT( VDM_RX_DONE_BIT_POS) );		// data to write to masked bit

	reg_val=ADD_BIT(VDMA_ECTL_REG_DRDY_EN_BIT_POS) + ADD_BIT( VDMA_ECTL_REG_DRDY_BIT_POS ) + // (ready int + clear rdy int)
			ADD_BIT(VDMA_ECTL_REG_HALT_INT_EN_BIT_POS) + ADD_BIT( VDMA_ECTL_REG_HALT_BIT_POS ) + // (halt int + clear halt int)
			(buff_size_in_16kb<<VDMA_ECTL_REG_BUFF_SIZE_POS)+ 								// ciclyc buffer size 16k*n
			ADD_BIT(VDMA_ECTL_REG_RETRIGGER_IF_BUFF_NOT_EMPTY_BIT_POS) + 					//(Retrigger if Source Size not Zero )
			ADD_BIT(VDMA_ECTL_REG_AUTO_STATUS_UPDATE_BIT_POS) + 							//(automatic update status register0)
			(VDMA_SIZE_MODIFIER_BITS_16_23_USED << VDMA_ECTL_REG_SIZE_MODIFIER_POS)+		//    (size in bits 16-23)
			ADD_BIT(VDMA_ECTL_REG_SIZE_CYCLIC_BUF_EN_BIT_POS);								// ciclyc buffer;
	REG_WRITE(VDMA_ECTL_REG,  reg_val );

	reg_val= ADD_BIT(VDMA_CNT_REG_BUS_LOCK_BIT_POS)+  	// bus-lock
			//ADD_BIT(VDMA_CNT_REG_BURST_BIT_POS)+ 		//  no 8-byte_burst
			ADD_BIT(VDMA_CNT_REG_ENABLE_POS) ;  		//  enable

	REG_WRITE(VDMA_CNT_REG, reg_val);



	ready_for_transmit = 1;
	return 0;
}


/* function : vdm_exit_common
 *
 *
 *
 */
int vdm_exit_common(void)
{
	uint32_t prev_pointer = 0;
	uint32_t curr_pointer = 1;
	// stop retrigerring
	CLEAR_REG_BIT(VDMA_ECTL_REG  , VDMA_ECTL_REG_RETRIGGER_IF_BUFF_NOT_EMPTY_BIT_POS);

	//wait till dma is not transferring data
	while (prev_pointer != curr_pointer)
	{
		prev_pointer = curr_pointer;
		mdelay(1);
		curr_pointer = REG_READ(VDMA_CDST_REG);
	}

	CLEAR_REG_BIT(VDMA_CNT_REG  , VDMA_CNT_REG_ENABLE_POS);
	CLEAR_REG_BIT(VDM_CNT_REG  , VDM_ENABLE_FIELD_POS);

	return 0;
}

