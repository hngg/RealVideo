

#include <sys/time.h>
#include <stdlib.h>
#include "glog.h"
#include "rtp.h"
#include "utils.h"
#include "jitter_buffer.h"


pj_status_t ringbuffer_create(unsigned packet_size, pj_bool_t resend_support, RingBuffer **p_jb)
{
	RingBuffer*prb = ringbuffer_alloc( VIDEO_PACKET_LENGTH, MAX_PACKET_NUMBER);
    prb->resend_support = resend_support;
	ringbuffer_init(prb);

	*p_jb = prb;
	return PJ_SUCCESS;
}

RingBuffer* ringbuffer_alloc( unsigned mtu, unsigned packetNum)
{
	RingBuffer* pRingBuffer = (RingBuffer*)malloc(sizeof(RingBuffer));
	if(!pRingBuffer)
	{
		log_error("allocate memory for _RingBuffer_ST_ failure.");
		return NULL;
	}
	
	pRingBuffer->xbuffer = (unsigned char*)malloc(mtu*packetNum);
	if(!pRingBuffer->xbuffer)
	{
		log_error("allocate memory for xbuffer failure.");
		return NULL;
	}	
	
	pj_bzero(pRingBuffer->xbuffer, mtu*packetNum);
	
	return pRingBuffer;
}

void ringbuffer_destory(RingBuffer* pRingBuffer)
{
	//PJ_UNUSED_ARG(pRingBuffer);
    if(pRingBuffer->xbuffer) {
		free(pRingBuffer->xbuffer);
<<<<<<< HEAD
        pRingBuffer->xbuffer = NULL;
    }

    if(pRingBuffer) {
        free(pRingBuffer);
        pRingBuffer = NULL;
    }
    
=======
    }

	free(pRingBuffer);

>>>>>>> d0e83e775b61c141acf0f986720c005b7d0f6a80
	return;
}

void ringbuffer_init(RingBuffer* pRingBuffer)
{
	if(!pRingBuffer)
	{
		log_error("pRingBuffer is null.");
		return;
	}
    pRingBuffer->uPrePos         = -1; //int
    pRingBuffer->uPreSeq         = 0;  //unsigned short
    pRingBuffer->uPreReadPos     = -1; //int
    pRingBuffer->uPreReadSeq     = 0;  //unsigned short
    pRingBuffer->uPreReadTS      = 0;
    pRingBuffer->uPreTime        = 0;
    
	pRingBuffer->uPacketSize = VIDEO_PACKET_LENGTH;
	pRingBuffer->uPacketNum  = MAX_PACKET_NUMBER;
	pRingBuffer->uMaxPktWin  = MAX_RTP_SEQ_WINDOW;

    pRingBuffer->uBufsizeTotal   = 0;
    pRingBuffer->uPacketCount    = 0;
	pRingBuffer->uCuPktLost 	 = 0;
    pRingBuffer->status          = SEQ_OK;
    
    pRingBuffer->uMaxPktTime = (!pRingBuffer->resend_support)?0:MAX_LOST_WAIT_TIME_MS;
    
	log_debug("finish,resend_support:%d,uMaxPktTime:%d MAX_PACKET_NUMBER:%d\n", pRingBuffer->resend_support, pRingBuffer->uMaxPktTime, pRingBuffer->uPacketNum);
}

int ringbuffer_write(RingBuffer* pRingBuffer, unsigned char* pRtpPkt, unsigned uRtpLen)
{
	int status = SEQ_OK;
	unsigned short packSeq = 0;//the newest package sequence
	Packet_Store_ST_ *pStorePkt = NULL;
	int index = 0, getLostPack = 0;
	pj_ssize_t uCurTS;
	
	
	if(!pRingBuffer || !pRtpPkt)
	{
		log_error("parameter error, pRingBuffer:%p, pRtpPkt:%p", pRingBuffer, pRtpPkt);
		return SEQ_NORMAL_ERROR;
	}

	if(uRtpLen <= RTP_HEAD_LENGTH || uRtpLen > (pRingBuffer->uPacketSize - sizeof(Packet_Store_ST_)))
	{
		log_error("parameter error, uRtpLen:%u", uRtpLen);
		return SEQ_NORMAL_ERROR;
	}

	packSeq = pj_ntohs(*(unsigned short*)(pRtpPkt+2));

	/* check max rtp seq winodow */
	// nSeqGap = (packSeq - pRingBuffer->uPreSeq)%65536;
	// if(abs(nSeqGap) > pRingBuffer->uMaxPktWin)
	// {
	// 	log_error("current seq:%u, prev seq:%u, gap:%d, is over max rtp seq windows:%d", 
	// 		packSeq, pRingBuffer->uPreSeq, nSeqGap, pRingBuffer->uMaxPktWin);
	// 	pjmedia_ringbuffer_init(pRingBuffer);
	// 	nSeqGap = 1;
	// }

	/* check rtp packet recv delay */
	uCurTS = getCurrentTimeMs();
	if(0 == pRingBuffer->uPreTime)
	{
		pRingBuffer->uPreTime = uCurTS;
	}

	if((uCurTS - pRingBuffer->uPreTime) > MAX_LOST_WAIT_TIME_UNKNOW)
	{
		log_error("current ts:%ld, prev ts:%ld is over max rtp wait time:%u", 
			uCurTS, pRingBuffer->uPreTime, pRingBuffer->uMaxPktTime);
        
		ringbuffer_init(pRingBuffer);
		//nSeqGap = 1;
	}

	/* store the rtp packet */
	index = packSeq%pRingBuffer->uPacketNum;
	pStorePkt = (Packet_Store_ST_*)(pRingBuffer->xbuffer + index*pRingBuffer->uPacketSize);	
	pStorePkt->uForRead 	= 1;
	pStorePkt->uResendFlg 	= 0;
    pStorePkt->uPktSeq 		= packSeq;		
    pStorePkt->uTimeStamp 	= uCurTS;
	pStorePkt->uPktLen 		= uRtpLen;	
	pj_memcpy(pStorePkt->xRtpBuf, pRtpPkt, uRtpLen);

	/* update the ringbuffer record */
    if(-1 != pRingBuffer->uPrePos)//packSeq first seq maybe 0
    {
        //packSeq!=pRingBuffer->uPreSeq because they maybe zero at the same time
        if(packSeq!=pRingBuffer->uPreSeq && (packSeq - pRingBuffer->uPreSeq != 1))
        {
            LostPackets *lostPack = &pRingBuffer->lostPack;
            //disorder or get the lost package
            if(packSeq < pRingBuffer->uPreSeq)
            {
                //reget the lost package
                if(packSeq>=lostPack->bgnSeq && packSeq<=lostPack->endSeq)
                {
                    getLostPack = 1;
                    log_debug("reget discontinuous package seq:%d", packSeq);
                }
                else//judge as disorder
                {
                    pRingBuffer->status = status = SEQ_DISORDER;
                    
                    log_error("find disorder package diff:%d cur:%d last:%d \n",
                        packSeq-pRingBuffer->uPreSeq-1, packSeq, pRingBuffer->uPreSeq);
                }
            }
            else //lost package
            {
                if(MAX_RTP_SEQ_VALUE!=(pRingBuffer->uPreSeq-packSeq)) {
                    if(packSeq>pRingBuffer->uPreSeq) {
                        lostPack->bgnSeq = pRingBuffer->uPreSeq + 1;
                        lostPack->endSeq = packSeq - 1;
                        lostPack->packCount = lostPack->endSeq - lostPack->bgnSeq +1;
                        pRingBuffer->status = status = SEQ_DISCONTINUOUS;
                        
                        log_error("find discontinuous package diff:%d cur:%d last:%d \n",
                            packSeq-pRingBuffer->uPreSeq-1, packSeq, pRingBuffer->uPreSeq);
                    }
                }
            }
        }//if discontinuous
    }
    else
    {
        pRingBuffer->uPreReadSeq = packSeq; //first read sequence
    }
    
    if(!getLostPack)
    {
        pRingBuffer->uPrePos        = index;
        pRingBuffer->uPreSeq        = packSeq;    //current can read packet seq
        pRingBuffer->uPreTime       = uCurTS;
    }

    pRingBuffer->uBufsizeTotal += uRtpLen;
    pRingBuffer->uPacketCount++;
    
	log_debug("rtp seq:%u, len:%d, to buffer pos:%d", packSeq, pStorePkt->uPktLen, index);
	
	return status;
}

pj_status_t ringbuffer_read(RingBuffer* pRingBuffer, unsigned char* pOutPkt, unsigned* pOutLen)
{
	pj_status_t status = PJ_FALSE;
	int  curReadSeq = -1, curReadPos = -1, canRead = 0;
	Packet_Store_ST_ *pStorePkt = NULL;
	
	if(!pRingBuffer || !pOutPkt || !pOutLen)
	{
		log_error("parameter error, pRingBuffer:%p, pRtpPkt:%p, pRtpLen:%p",
                  pRingBuffer, pOutPkt, pOutLen);
        
		return status;
	}

    //important two steps, 1:uPrePos!=-1 2:uPreReadPos!=-1 (for the correct read position)
	if(-1 != pRingBuffer->uPreReadPos)//uPreReadSeq
	{
        if(pRingBuffer->uPreReadSeq==pRingBuffer->uPreSeq)
            return status;
        
		curReadSeq = pRingBuffer->uPreReadSeq+1;
	}
	else	//first read
	{
		if(-1 != pRingBuffer->uPrePos) //1 steps(uPrePos != -1)
		{
			curReadSeq = pRingBuffer->uPreReadSeq;
            
            //2 steps(uPreReadPos!=-1 and read one package)
            pRingBuffer->uPreReadPos = curReadSeq%pRingBuffer->uPacketNum;
		}
		else //had not write package to pRingBuffer
		{
			return status;
		}
	}

	if(curReadSeq != -1) {
		curReadPos = curReadSeq%pRingBuffer->uPacketNum;
		pStorePkt = (Packet_Store_ST_ *)(pRingBuffer->xbuffer + curReadPos*pRingBuffer->uPacketSize);
		if( pStorePkt->uForRead )
		{
			canRead = 1;
			//pRingBuffer->uPreReadTS = getCurrentTimeMs();//normal read time
		}
		else
		{
            if(pRingBuffer->status == SEQ_DISCONTINUOUS) {
                if(((getCurrentTimeMs() - pRingBuffer->uPreReadTS) <= MAX_LOST_WAIT_TIME_MS)) {
                    //something to do
                    return PJ_FALSE;
                }
            }
            else
            {
                //calculate lost packate, send fir and find idr frame
                canRead = 1;
                pRingBuffer->uCuPktLost++;
                pRingBuffer->status = SEQ_OK;
                log_warn("lost packet count:%d seq %d\n", pRingBuffer->uCuPktLost, curReadSeq);
            }
		}

		if(canRead) {
			pj_memcpy(pOutPkt, pStorePkt->xRtpBuf, pStorePkt->uPktLen);
			*pOutLen = pStorePkt->uPktLen;

			pStorePkt->uForRead 		= 0;
			pStorePkt->uResendFlg 		= 0;

			pRingBuffer->uPreReadPos 	= curReadPos;
			pRingBuffer->uPreReadSeq 	= curReadSeq;
			pRingBuffer->uPreReadTS 	= pStorePkt->uTimeStamp;

			status = PJ_TRUE;

			log_debug("rtp seq:%u, len:%d, from buffer pos:%u",
                      pStorePkt->uPktSeq, pStorePkt->uPktLen, curReadPos);
		}

	}//curReadSeq != -1

	return status;
}

