
#ifndef VIDEO_RTC_API_H_
#define VIDEO_RTC_API_H_


typedef int (*rtp_frame_cb)(char* frameBuffer, int frameLen);


#define RTC_API

RTC_API
char* getVersion(void);

RTC_API //for ios
int vid_stream_create_ios(const char*localAddr, unsigned short localRtpPort, rtp_frame_cb frame_cb, int codecType);

RTC_API
int vid_stream_create(const char*localAddr, unsigned short localRtpPort, void*surfacem, int codecType);
RTC_API
int vid_stream_destroy(void);

RTC_API
int vid_stream_start(const char*remoteAddr, unsigned short remoteRtpPort);
RTC_API
int vid_stream_stop(void);

RTC_API
int packet_and_send(char* frameBuffer, int frameLen);

#endif
