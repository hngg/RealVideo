#ifndef __VID_PORT_H__
#define __VID_PORT_H__

#include "os.h"
#ifdef __ANDROID__
#include "mediacodec.h"
#endif

#include "video_rtc_api.h"

struct pjmedia_vid_port
{
    void*useStream;
    pj_thread_t jbuf_recv_thread;
    
    #ifdef __ANDROID__
    void*surface;
    MediaCodecDecoder *decoder;
    #endif
    
    on_rtp_frame rtp_cb;
};

typedef struct pjmedia_vid_port pjmedia_vid_port;

int vid_port_create();
int vid_port_destroy();

int vid_port_start(pjmedia_vid_port *vidPort);
int vid_port_stop(pjmedia_vid_port *vidPort);


#endif
