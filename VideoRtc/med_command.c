

#include "med_command.h"
#include "utils.h"
#include "os.h"
#include "glog.h"

pjmedia_vid_command g_vid_command;


RTC_API
int med_command_create(const char*localAddr, unsigned short localPort, on_comm_recv comm_cb) {
    int status = 0;
    memset(&g_vid_command, 0, sizeof(pjmedia_vid_command));
    
    return status;
}

RTC_API
int med_command_destroy() {
    int result = -1;

    return result;
}

RTC_API
int med_command_connect(const char* remoteAddr, unsigned short remotePort) {
    int result = -1;

    return result;
}

RTC_API
int med_command_send(char* buffer, int size) {
    int result = -1;

    return result;
}
