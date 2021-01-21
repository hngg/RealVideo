
#ifndef VIDEO_RTC_API_H_
#define VIDEO_RTC_API_H_


typedef int (*on_rtp_frame)(char* frameBuffer, int frameLen);
typedef void (*on_network_status)(int rtt, int byte_count, long lost_rate);

#define RTC_API

RTC_API
char* getVersion(void);

RTC_API //for ios
int vid_stream_create_ios(const char*localAddr, unsigned short localRtpPort, on_rtp_frame frame_cb, int codecType);

RTC_API
int vid_stream_create(const char*localAddr, unsigned short localRtpPort, void*surfacem, int codecType);
RTC_API
int vid_stream_destroy(void);

RTC_API
int vid_stream_network_callback(on_network_status net_cb);

RTC_API
int vid_stream_start(const char*remoteAddr, unsigned short remoteRtpPort);
RTC_API
int vid_stream_stop(void);

RTC_API
int packet_and_send(char* frameBuffer, int frameLen);

#endif
