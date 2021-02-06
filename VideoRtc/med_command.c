

#include "med_command.h"
#include "utils.h"
#include "os.h"
#include "glog.h"
#include "rtcp.h"
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>

pjmedia_vid_command g_vid_command;

//thread
static void worker_comm_recvfrom(void *arg)
{
    struct pjmedia_vid_command *udp = (struct pjmedia_vid_command *) arg;
    struct trans_channel *chann = &udp->channel;
    pj_uint32_t  value = 0;
    if (ioctl(chann->sockfd, FIONBIO, &value))
    {
        //status  = pj_get_netos_error();
        return ;
    }
    
    ssize_t status = 0;
    pj_ssize_t  bytes_read = 0;
    socklen_t addr_len =sizeof(struct sockaddr_in);
    fd_set  recv_set; /* add by j33783 20190621 */
    struct timeval timeout, tv_start = {0, 0}, tv_end = {0, 0};

    on_comm_recv cb;//int (*cb)(void*, pj_ssize_t);
    cb = udp->comm_cb;
    while(chann->sockfd != PJ_INVALID_SOCKET && !chann->recv_tid.thread_quit)  /* modify by j33783 20190509 */
    {
        //special, use the struct pjmedia_vid_stream
        if(chann->is_rtcp)
        {
            gettimeofday(&tv_end, NULL);
            if(tv_end.tv_sec-tv_start.tv_sec>=1)
            {
                gettimeofday(&tv_start, NULL);
                char rtcp[100] = {0};
                pj_size_t size = 0;
                rtcp_build_rtcp_sr(rtcp, &size);
                status = sendto(chann->sockfd, rtcp, size, 0,
                            (const struct sockaddr *)&chann->remote_addr, sizeof(struct sockaddr_in));
                if(status < 0) {
                    log_error("rtcp sendto failed sockid:%d errno:%d", chann->sockfd, errno);
                }
                log_debug("rtcp send begin size:%d rtcp:%d addr:%s port:%d", size, chann->is_rtcp,
                          inet_ntoa(chann->remote_addr.sin_addr), htons(chann->remote_addr.sin_port));
                
            }
        }//rtcp sr heartbeat
        
        timeout.tv_sec  = 0;
        timeout.tv_usec = 200000;//100ms
        FD_ZERO(&recv_set);
        FD_SET(chann->sockfd, &recv_set);
        status = select(chann->sockfd + 1, &recv_set, NULL, NULL, &timeout);
        if(status>0)
        {
           if(FD_ISSET(chann->sockfd, &recv_set)) {
               bytes_read = RTP_LEN;
               status = recvfrom(chann->sockfd,  chann->recv_buf, bytes_read, 0,
                               (struct sockaddr *)&chann->client_addr, &addr_len);
               
               if (status>0 && cb)  /* 20190328 vid_stream.c on_rx_rtp */   /* add by j33783 20190509 */
               {
                   (*cb)(chann->recv_buf, (int)status);
                   //log_warn("recvfrom H264__ size:%d ", status);
               }
               else//EAGAIN
               {
                   //to do
               }
           }
        }
        else if(0 == status)//
        {
            //usleep(100);//10000->10ms
            continue;
        }
        else if(status < 0)
        {
            //log_error("thread will exit, pj_sock_select return:%d", status);
            break;
        }
        
    }//while
}


RTC_API
int med_command_create(const char*localAddr, unsigned short localPort, on_comm_recv comm_cb) {
    int status = 0;
    memset(&g_vid_command, 0, sizeof(pjmedia_vid_command));
    //set data callback to ui layer
    g_vid_command.comm_cb = comm_cb;
    
    struct trans_channel* chann = &g_vid_command.channel;
    chann->user_udp = &g_vid_command;
    chann->is_rtcp = PJ_TRUE;

    transport_channel_create(chann, localAddr, localPort, PJ_TRUE, NULL, worker_comm_recvfrom);
    
    return status;
}

RTC_API
int med_command_destroy() {
    int status = -1;
    status = transport_channel_destroy(&g_vid_command.channel);
    return status;
}

RTC_API
int med_command_start(const char* remoteAddr, unsigned short remotePort) {
    int status = -1;

    status = transport_channel_start(&g_vid_command.channel, remoteAddr, remotePort, "comm_thread");
    
    return status;
}

RTC_API
int med_command_stop() {
    int status = -1;
    status = transport_channel_stop(&g_vid_command.channel);
    return status;
}


RTC_API
int med_command_send(char* buffer, pj_uint32_t size) {
    ssize_t status = -1;
    struct trans_channel* chann = &g_vid_command.channel;
    status = sendto(chann->sockfd, buffer, size, 0,
                    (const struct sockaddr *)&chann->remote_addr, sizeof(struct sockaddr_in));
    if(status < 0) {
        log_error("comm sendto failed sockid:%d errno:%d", chann->sockfd, errno);
    }
    return (int)status;
}
