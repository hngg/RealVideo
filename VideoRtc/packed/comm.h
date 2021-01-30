

#ifndef __COMM_H__
#define __COMM_H__

#include "types.h"

#ifndef RTCP_APP

#define RTCP_FIR  192   /* add by j33783 20190805 */
#define RTCP_SR   200
#define RTCP_RR   201
#define RTCP_SDES 202
#define RTCP_BYE  203
#define RTCP_NACK 205
#define RTCP_XR   207

#define RTCP_SR_RR_FALG 1
#define RTCP_SDES_FALG (RTCP_SR_RR_FALG << 1)
#define RTCP_NACK_FALG (RTCP_SDES_FALG << 1)
#define RTCP_BYTE_FALG (RTCP_NACK_FALG << 1)
#define RTCP_FIR_FALG (RTCP_BYTE_FALG << 1)   

#define RTCP_APP 204
#endif



#pragma pack(1)

/**
 * RTCP common header.
 */
typedef struct pjmedia_rtcp_common
{
#if defined(PJ_IS_BIG_ENDIAN) && PJ_IS_BIG_ENDIAN!=0
    unsigned	    version:2;	/**< packet type            */
    unsigned	    p:1;	/**< padding flag           */
    unsigned	    count:5;	/**< varies by payload type */
    unsigned	    pt:8;	/**< payload type           */
#else
    unsigned	    count:5;	/**< varies by payload type */
    unsigned	    p:1;	/**< padding flag           */
    unsigned	    version:2;	/**< packet type            */
    unsigned	    pt:8;	/**< payload type           */
#endif
    unsigned	    length:16;	/**< packet length          */
    pj_uint32_t	    ssrc;	/**< SSRC identification    */
} pjmedia_rtcp_common;


#pragma pack()




#endif
