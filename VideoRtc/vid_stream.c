


#include <time.h>
#include <stdint.h>
#include <stdio.h>



#include "utils.h"
#include "basedef.h"
#include "vid_codec.h"
#include "callCplus.h"

#include "vid_stream.h"
#include "video_rtc_api.h"

#if defined(PJMEDIA_HAS_VIDEO) && (PJMEDIA_HAS_VIDEO != 0)

#define THIS_FILE			"vid_stream.c"
#define ERRLEVEL			1
#define LOGERR_(expr)			stream_perror expr
#define TRC_(expr)			log_trace(expr)
#define SIGNATURE			PJMEDIA_SIG_PORT_VID_STREAM
#endif

/* Tracing jitter buffer operations in a stream session to a CSV file.
 * The trace will contain JB operation timestamp, frame info, RTP info, and
 * the JB state right after the operation.
 */

#ifndef PJMEDIA_VSTREAM_SIZE
#   define PJMEDIA_VSTREAM_SIZE	1000
#endif

#ifndef PJMEDIA_VSTREAM_INC
#   define PJMEDIA_VSTREAM_INC	1000
#endif

typedef enum {
	_SOFT_CODING_SOFT_DECODING_,
	_SOFT_CODING_HARD_DECODING_,
	_HARD_CODING_SOFT_DECODING_,
	_HARD_CODING_HARD_DECODING_
}pjsua_call_hardware_e;

/*add for h264 and h265 define by 180732700, 20191113*/
typedef enum {
	AVC_NAL_SINGLE_NAL_MIN	= 1,
	AVC_NAL_UNIT_IDR = 5,
    AVC_NAL_UNIT_SEI = 6,
	AVC_NAL_UNIT_SPS = 7,
	AVC_NAL_UNIT_PPS = 8,
    AVC_NAL_SINGLE_NAL_MAX	= 23,
    AVC_NAL_STAP_A		= 24,
    AVC_NAL_FU_TYPE_A	= 28,
}avc_nalu_type_e;

typedef enum {
	HEVC_NAL_UNIT_IDR = 19,
	HEVC_NAL_UNIT_VPS = 32,	//32
	HEVC_NAL_UNIT_SPS = 33,	//33
	HEVC_NAL_UNIT_PPS = 34,	//34
	HEVC_NAL_FU_TYPE  = 49,
}hevc_nalu_type_e;
/*end for h264 and h265 define by 180732700, 20191113*/

/* add by 180732700 for call type 20200423*/
typedef enum {
    _PNAS_CALL_TYPE_FULL_DUPLEX_VOICE_SINGLE_CALL = 1,
	_PNAS_CALL_TYPE_HALF_DUPLEX_SINGLE_VOICE_CALL = 2,
	_PNAS_CALL_TYPE_VOICE_GROUP_CALL = 3,
	_PNAS_CALL_TYPE_ENV_LISTENING_CALL = 4,
	_PNAS_CALL_TYPE_FULL_DUPLEX_VOICE_VIDEO_SINGLE_CALL = 10,
	_PNAS_CALL_TYPE_SAME_VOICE_VIDEO_SOURCE_GROUP_CALL = 11,
	_PNAS_CALL_TYPE_DIFFERENT_VOICE_VIDEO_SOURCE_GROUP_CALL = 12,
	_PNAS_CALL_TYPE_VIDEO_PUSH_CALL = 13,
	_PNAS_CALL_TYPE_VIDEO_UP_PULL_CALL = 14,
	_PNAS_CALL_TYPE_VIDEO_DOWN_PULL_CALL = 15,
	_PNAS_CALL_TYPE_VIDEO_BACK_TRANSFER_CALL = 16,
	_PNAS_CALL_TYPE_ENV_WATCHING_CALL = 17,
	_PNAS_CALL_TYPE_HALF_DUPLEX_ACCEPT_CALL = 18,
	_PNAS_CALL_TYPE_VIDEO_GROUP_PUSH_CALL = 25,
	_PNAS_CALL_TYPE_VOICE_BROADCAST = 160,
	_PNAS_CALL_TYPE_VOICE_VIDEO_BROADCAST = 161,
	_PNAS_CALL_TYPE_VIDEO_BROADCAST = 162
}pnas_call_type_e;


pjmedia_vid_stream g_vid_stream;

static void on_rx_rtp(void *useData, void *pkt, pj_ssize_t bytes_read)
{
	//log_error("on_rx_rtp bytes_read:%d.", bytes_read);
	pjmedia_vid_stream *stream = (pjmedia_vid_stream *)useData;
	//rtp_packet_recv(stream, pkt, bytes_read);
	//calculation_video_rtp_loss

	int result = ringbuffer_write(stream->ringbuf, (unsigned char*)pkt, (unsigned)bytes_read);
	if(SEQ_DISCONTINUOUS == result) {
		//send nack
		LostPackets *lp = &stream->ringbuf->lostPack;
		log_error("on_rx_rtp discontinuous pack number:%d begin:%d end:%d\n", lp->packCount, lp->bgnSeq, lp->endSeq);

        char rtcp[100] = {0};
        pj_size_t size = 0;

        rtcp_build_rtcp_nack_(rtcp, &size, lp->bgnSeq, lp->packCount);
        transport_send_rtcp(stream->trans, rtcp, (pj_uint32_t)size);
//		if(lp->packCount>0) {
//			char buffer[PJMEDIA_MAX_MTU] = {0};
//			rtcp_nack_packet *nack = (rtcp_nack_packet*)buffer;
//			nack->number 	= lp->packCount;
//			nack->start_seq = lp->bgnSeq;
//			nack->end_seq	= lp->endSeq;
//			transport_send_rtcp(stream->trans, buffer, sizeof(rtcp_nack_packet));
//			log_error("on_rx_rtp resend rtcp length:%d\n", sizeof(rtcp_nack_packet));
//		}

	}
}

static void on_rx_rtcp(void *useData, void *pkt, pj_ssize_t bytes_read)
{
    long start_time = get_currenttime_us();
	log_debug("on_rx_rtcp bytes_read:%d.", (int)bytes_read);
    pjmedia_rtcp_common *common = (pjmedia_rtcp_common*)pkt;
    pjmedia_rtcp_sr_pkt *sr = NULL;
    pjmedia_rtcp_rr_pkt *rr = NULL;
    pjmedia_rtcp_nack_pkg *nack = NULL;
    pjmedia_vid_stream  *stream = (pjmedia_vid_stream *)useData;
    char rtcp[100] = {0};
    pj_size_t size = 0;
    switch(common->pt)
    {
        case RTCP_SR:
            sr = (pjmedia_rtcp_sr_pkt*)pkt;
            rtcp_build_rtcp_rr(rtcp, &size, sr->rr.lsr, (int)(get_currenttime_us()-start_time));
            if(size>0){
                transport_send_rtcp(stream->trans, rtcp, (pj_uint32_t)size);
                log_debug("rtcp recv sr and send size:%d lsr:%d", size, sr->rr.lsr);
            }
            break;
            
        case RTCP_RR:
            rr = (pjmedia_rtcp_rr_pkt*)pkt;
            int rrtus = (int)(get_currenttime_us() - rr->rr.lsr);
            if(stream->network_cb)
                stream->network_cb(rrtus - rr->rr.dlsr, 0, 0);
            log_debug("rtcp recv rr rtt time:%d lsr:%d us delay:%d us",
                      rrtus, rr->rr.lsr, rr->rr.dlsr );
            break;
            
        case RTCP_NACK:
            nack = (pjmedia_rtcp_nack_pkg *)pkt;
            unsigned    base_seq = nack->nack.base_seq;
            unsigned    count = nack->nack.flag;
            resend_losted_package(stream->trans, base_seq, count);
            log_debug("rtcp recv nack begin_seq:%d count:%d", base_seq, count);
            break;
    }
	//rtcp_nack_packet *nack = (rtcp_nack_packet*)pkt;
	//log_error("on_rx_rtcp recv number:%d startseq:%d endseq:%d\n", nack->number, nack->start_seq, nack->end_seq);
}


int stream_create(const char*localAddr, unsigned short localRtpPort, int codecType)
{
    int status = 0;
    
    memset(&g_vid_stream, 0, sizeof(pjmedia_vid_stream));
    g_vid_stream.fmt_id = (codecType==H264_HARD_CODEC)?PJMEDIA_FORMAT_H264:PJMEDIA_FORMAT_H265;
    g_vid_stream.codecType = codecType;
    
    g_vid_stream.rtp_session.out_pt = RTP_PT_H264;
    g_vid_stream.rtp_session.out_extseq = 0;
    g_vid_stream.rto_to_h264_obj = Launch_CPlus(g_vid_stream.fmt_id);
    status = ringbuffer_create(0, RESEND_SUPPORT, &g_vid_stream.ringbuf);
    
    status = transport_udp_create(&g_vid_stream.trans, localAddr, localRtpPort, &on_rx_rtp, &on_rx_rtcp);
    if(status<0) {
        log_error("transport_udp_create failed.");
        return status;
    }
    
    g_vid_stream.vid_port.useStream = &g_vid_stream;
    g_vid_stream.trans->user_stream = &g_vid_stream;
    
    return status;
}


FILE* fp = NULL;
const char*filename = "result.h264";


RTC_API //for ios
int vid_stream_create_ios(const char*localAddr, unsigned short localRtpPort, on_rtp_frame frame_cb, int codecType) {
    int status = 0;
    memset(&g_vid_stream, 0, sizeof(pjmedia_vid_stream));
    
    status = stream_create(localAddr, localRtpPort, codecType);
    
    g_vid_stream.vid_port.rtp_cb = frame_cb;

    //fp = fopen(filename, "wb");
    return status;
}

RTC_API
int vid_stream_create(const char*localAddr, unsigned short localRtpPort, void*surface, int codecType) {
	int status = 0;
    
	status = stream_create(localAddr, localRtpPort, codecType);

    #ifdef __ANDROID__
    g_vid_stream.vid_port.decoder = NULL;
    g_vid_stream.vid_port.surface = surface;
    #endif
    
	//fp = fopen(filename, "wb");
	return status;
}

RTC_API
int vid_stream_destroy() {
	int result = -1;
	transport_udp_destroy(g_vid_stream.trans);  //release transport_udp* trans
    
	if (g_vid_stream.ringbuf) {
		ringbuffer_destory(g_vid_stream.ringbuf); //release IBaseRunLib* jb_opt
		g_vid_stream.ringbuf = NULL;
	}
    
	// if(fp) {
	// 	fclose(fp);
	// 	fp = NULL;
	// }

	return result;
}

int vid_stream_network_callback(on_network_status net_cb) {
    g_vid_stream.network_cb = net_cb;
    return  0;
}

RTC_API
int vid_stream_start(const char*remoteAddr, unsigned short remoteRtpPort) {
	int status = -1;
	status = vid_port_start(&g_vid_stream.vid_port);
	status = transport_udp_start( g_vid_stream.trans, remoteAddr, remoteRtpPort);
	return status;
}

RTC_API
int vid_stream_stop() {
	int status = -1;
	status = vid_port_stop(&g_vid_stream.vid_port);
	transport_udp_stop( g_vid_stream.trans);
	return status;
}

RTC_API
int packet_and_send(char* frameBuffer, int frameLen) {
    return packet_and_send_(&g_vid_stream, frameBuffer, frameLen);
}

int packet_and_send_(pjmedia_vid_stream*stream, char* frameBuffer, int frameLen) {
	int result = -1, ext_len = 0, mark = 0;
	char rtpBuff[2000] = {0};
	pjmedia_frame package_out = {0};

	long timestamp = get_currenttime_us();
	
	char ori_ext = pjmedia_rtp_rotation_ext(90);
	ext_len = pjmedia_video_add_rtp_exten(rtpBuff + sizeof(pjmedia_rtp_hdr), 0xEA, (pj_uint8_t*)&ori_ext, sizeof(pj_uint16_t));
	package_out.buf = rtpBuff + sizeof(pjmedia_rtp_hdr) + ext_len;

	/* Loop while we have frame to send */
	do
	{
		pj_status_t status = 0;
		if(stream->codecType == H264_HARD_CODEC)
			status = get_h264_package(frameBuffer, frameLen, &package_out);
		else
		{
			status = get_h265_package(frameBuffer, frameLen, &package_out);
		}
		
		mark = !package_out.has_more;
		rtp_update_hdr(&stream->rtp_session, rtpBuff, mark, package_out.size, timestamp );//set rtp head parameter and out_extseq++
		pjmedia_rtp_hdr *hdr_data = (pjmedia_rtp_hdr*)rtpBuff;
		hdr_data->x = (ext_len>0)?1:0;
        result = transport_priority_send_rtp(stream->trans, rtpBuff, package_out.size + sizeof(pjmedia_rtp_hdr) + ext_len);
		//result = transport_send_rtp_seq(stream->trans, rtpBuff, package_out.size + sizeof(pjmedia_rtp_hdr) + ext_len, stream->rtp_session.out_extseq);
		if(package_out.enc_packed_pos>=frameLen)
			break;
		package_out.buf = rtpBuff + sizeof(pjmedia_rtp_hdr);
		ext_len = 0;

	}while(1);

	return result;
}

