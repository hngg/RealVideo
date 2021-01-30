

#ifndef __MED_COMMAND_H__
#define __MED_COMMAND_H__

#include "types.h"
#include "transport_udp.h"
#include "video_rtc_api.h"

typedef struct pjmedia_vid_command
{
    transport_udp       *trans;
    int                 codecType;
    on_comm_recv        comm_cb;

    void                *user_stream;    //struct pjmedia_vid_stream point address
    pj_bool_t           attached;
    
    //unsigned          out_rtcp_pkt_size;
    char                out_buff[PJMEDIA_MAX_MTU];  /**< Outgoing RTCP packet.        */
    
}pjmedia_vid_command;


#endif
