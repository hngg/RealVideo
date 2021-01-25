
#ifndef __PJMEDIA_VID_STREAM_TEST_H__
#define __PJMEDIA_VID_STREAM_TEST_H__

#include "types.h"
#include "vid_stream.h"

#ifndef __ANDROID__
int stream_send_test(const char *localAddr, unsigned short localPort, const char*remoteAddr,
	unsigned short remotePort, const char*sendFile, int codecType);
int stream_recv_test(const char *localAddr, short localPort, int codecType);

int stream_send_rtcp_test(const char *localAddr, short localPort, const char*remoteAddr, 
	short remotePort, int codecType);

int rtp_packet_recv_264(struct pjmedia_vid_stream*stream, char* packetBuffer, int packetLen);
int rtp_packet_and_unpack_notnet_test(pjmedia_vid_stream*stream, char* frameBuffer, int frameLen);
void rtp_packet_and_unpack_notnet_test_264(pj_uint8_t *bits, pj_size_t bits_len);
void h264_package_test(pj_uint8_t *bits, pj_size_t bits_len);
void packet_and_send_test(pj_uint8_t *bits, pj_size_t bits_len);
#endif //__ANDROID__


#endif	/* __PJMEDIA_VID_STREAM_H__ */
