

#include "transport_udp.h"

#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "rtp.h"
#include "rtcp.h"
#include "glog.h"
#include "utils.h"

#ifdef __ANDROID__
#include "mediacodec.h"
#endif

void vid_sendto_exit_deal(int arg)
{	
    //log_debug("sendto thread quit:%p", pthread_self());
	pthread_exit(0);
}

void vid_recv_from_exit_deal(int arg)
{	
	//log_debug("recvfrom thread quit:%p", pthread_self());
	pthread_exit(0);
}

//thread safe
ssize_t channel_recv_loop(struct trans_channel*recv_channel, void *user_udp)
{
    ssize_t status = 0;
    pj_ssize_t  bytes_read = 0;
    socklen_t addr_len =sizeof(struct sockaddr_in);
    fd_set  recv_set; /* add by j33783 20190621 */
    struct timeval timeout, tv_start = {0, 0}, tv_end = {0, 0};

    transport_udp *udp = (transport_udp *)user_udp;
    on_recv_data cb;//void (*cb)(void*, void*, pj_ssize_t);
    cb = recv_channel->recv_cb;
    while(recv_channel->sockfd != PJ_INVALID_SOCKET && !recv_channel->recv_tid.thread_quit)  /* modify by j33783 20190509 */
    {
        //special, use the struct pjmedia_vid_stream
        if(recv_channel->is_rtcp)
        {
            gettimeofday(&tv_end, NULL);
            if(tv_end.tv_sec-tv_start.tv_sec>=1)
            {
                gettimeofday(&tv_start, NULL);
                if(user_udp)
                {
                    char rtcp[100] = {0};
                    pj_size_t size = 0;
                    trans_channel channel = udp->rtcp_chanel;
                    rtcp_build_rtcp_sr(rtcp, &size);
                    transport_send_rtcp(udp, rtcp, (pj_uint32_t)size);
                    log_debug("rtcp send begin size:%d rtcp:%d addr:%s port:%d",
                              size, channel.is_rtcp, inet_ntoa(channel.remote_addr.sin_addr), htons(channel.remote_addr.sin_port));
                }
            }
        }//rtcp sr heartbeat
        
        /* add by j33783 20190621 begin */
        timeout.tv_sec  = 0;
        timeout.tv_usec = 200000;//100ms
        FD_ZERO(&recv_set);
        FD_SET(recv_channel->sockfd, &recv_set);
        status = select(recv_channel->sockfd + 1, &recv_set, NULL, NULL, &timeout);

        if(status>0)
        {
           if(FD_ISSET(recv_channel->sockfd, &recv_set)) {
               bytes_read = RTP_LEN;
               status = recvfrom(recv_channel->sockfd,  recv_channel->recv_buf, bytes_read, 0,
                               (struct sockaddr *)&recv_channel->client_addr, &addr_len);
               
               if (status>0 && cb)  /* 20190328 vid_stream.c on_rx_rtp */   /* add by j33783 20190509 */
               {
                   (*cb)(udp->user_stream, recv_channel->recv_buf, status);
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
    
    return status;
}

static void worker_rtp_recvfrom(void *arg)
{
	    ssize_t status;

        struct sigaction new_act;
        memset(&new_act, 0, sizeof(new_act));
        new_act.sa_handler = vid_recv_from_exit_deal;
        sigemptyset( &new_act.sa_mask);
        sigaction(SIGUSR2, &new_act, NULL);
   
        log_debug("running rtp recv_from thread %ld begin.",pthread_self());  /* modify by j33783 20190509 */

        struct transport_udp *udp = (struct transport_udp *) arg;
        struct trans_channel *recv_channel = &udp->rtp_chanel;
        pj_uint32_t  value = 0;
        if (ioctl(recv_channel->sockfd, FIONBIO, &value))
        {
	    	//status  = pj_get_netos_error();
		  	return ;
        }
	   
        status = channel_recv_loop(recv_channel, udp);
    
		log_debug("running worker_rtp_recvfrom thread exit status:%d threadid:%ld", status, recv_channel->recv_tid.threadId);

	    return ;
}

static void worker_rtcp_recvfrom(void *arg)
{
     ssize_t status;
    
     struct transport_udp *udp = (struct transport_udp *) arg;
     struct trans_channel *recv_channel = &udp->rtcp_chanel;

     log_debug("running rtcp recv_from thread %ld begin.",pthread_self());  /* modify by j33783 20190509 */

     pj_uint32_t  value = 0;
     if (ioctl(recv_channel->sockfd, FIONBIO, &value))
     {
         //status  = pj_get_netos_error();
         return ;
     }
    
     status = channel_recv_loop(recv_channel, udp);
     log_debug("running worker_rtcp_recvfrom thread exit status:%d threadid:%ld", status, recv_channel->recv_tid.threadId);

	 return ;
}

pjmedia_vid_buf *create_resend_buf() {
	pjmedia_vid_buf *pVidbuf = (pjmedia_vid_buf*)malloc(sizeof(pjmedia_vid_buf));
	pVidbuf->buf = malloc(RESEND_BUFF_NUMBER*PJMEDIA_MAX_MTU);
	pVidbuf->pkt_len = malloc(RESEND_BUFF_NUMBER*sizeof(pj_uint16_t));
    
    pVidbuf->buf_size = RESEND_BUFF_NUMBER; //the number of resend package number

	return pVidbuf;
}

void release_resend_buf(pjmedia_vid_buf*resend_buf) {
	if(resend_buf) {
		free(resend_buf->pkt_len);
		resend_buf->pkt_len = NULL;

		free(resend_buf->buf);
		resend_buf->buf = NULL;

		free(resend_buf);
		resend_buf = NULL;
	}
}

void resend_save_rtp( pjmedia_vid_buf *vidBuf, pj_uint16_t extSeq, char*sendBuf, pj_uint16_t pkt_len){
	if(!vidBuf || !sendBuf)
		return ;
    
	pj_uint16_t index;
	index = extSeq % vidBuf->buf_size;
	pj_memcpy(vidBuf->buf + index * PJMEDIA_MAX_MTU, sendBuf, pkt_len);//pkt_len is smaller than PJMEDIA_MAX_MTU
	vidBuf->pkt_len[index] = pkt_len;
}

//for rtp package resend
pj_status_t resend_losted_package( struct transport_udp* udp, unsigned begin_seq, unsigned count)
{
    ssize_t status = -1;
    int i = 0, seq = begin_seq;
    if(!udp || count >= RESEND_BUFF_NUMBER)
        return -1;
    
    socklen_t addr_len =sizeof(struct sockaddr_in);
    
    struct trans_channel *rt_channel = &udp->rtp_chanel;
    pjmedia_vid_buf *resendBuf = rt_channel->resend;
    for(; i<count; i++)
    {
        pj_uint16_t index = seq++ % resendBuf->buf_size;
        void* sendPos = resendBuf->buf + index * PJMEDIA_MAX_MTU;
        int sendLen   =  resendBuf->pkt_len[index];
        
        status = sendto(rt_channel->sockfd, sendPos, sendLen, 0,
                        (const struct sockaddr *)&rt_channel->remote_addr, addr_len);
        if(status < 0) {
            switch(errno)
            {
                case EAGAIN:
                    if(packet_list_check_overflow(rt_channel->send_list.list_send_size,
                                                  rt_channel->send_list.list_write_size, RTP_LIST_MAX_SIZE))
                    {
                        packet_list_reset(&rt_channel->send_list);
                        log_debug("---reset---\n");
                    }
                    packet_list_node_add(&rt_channel->send_list, sendPos, sendLen);
                    break;
            }
            log_error("send_priority resend failed sockid:%d seq:%d errno:%d",
                      rt_channel->sockfd, seq-1, errno);
        }
        else
            log_debug("send_priority resend sockid:%d seq:%d", rt_channel->sockfd, seq-1);
    }

    return (int)status;
}

pj_status_t transport_channel_create( struct trans_channel*chan, const char *localAddr, unsigned short localPort,
                                     pj_bool_t is_rtcp, on_recv_data recv_cb, func_worker_recvfrom work_recv)
{
    pj_status_t status = 0;
    int rtp_sock, optVal = 1;
    
    char *sockTypt = (is_rtcp)?"rtcp":"rtp";
    log_debug("transport_channel_create %s addr:%s port:%d", sockTypt, localAddr, localPort);
    
    //create socket
    rtp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if(rtp_sock < 0) {
        log_error("create %s socket failed", sockTypt);
        return -1;
    }

    setsockopt(rtp_sock, SOL_SOCKET, SO_REUSEADDR, (const void *)&optVal, sizeof(optVal));

    //config for rtp
    if(!is_rtcp) {
        struct timeval tv = {3, 0};
        setsockopt(rtp_sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(struct timeval));
        setsockopt(rtp_sock, SOL_SOCKET, SO_SNDTIMEO, (char*)&tv, sizeof(struct timeval));
        
        unsigned nSendBuf = 2048*1024;
        unsigned nRecvBuf = 2048*1024;
        setsockopt(rtp_sock, SOL_SOCKET,SO_SNDBUF, (const char*)&nSendBuf, sizeof(int));
        setsockopt(rtp_sock, SOL_SOCKET,SO_RCVBUF, (const char*)&nRecvBuf, sizeof(int));
    }

    struct sockaddr_in *rtp_addr = &chan->local_addr;
    memset(rtp_addr, 0, sizeof(struct sockaddr_in));
    rtp_addr->sin_family = AF_INET;
    rtp_addr->sin_port   = htons(localPort);        //rtp_addr(sock->sin_port)
    rtp_addr->sin_addr.s_addr = inet_addr(localAddr);  //inet_ntoa(rtp_addr->sin_addr) get ip string
    
    status = bind(rtp_sock, (struct sockaddr *)rtp_addr, sizeof(struct sockaddr_in));
    if(status < 0) {
        close(rtp_sock);
        log_error("bind %s sock:%d failed.", sockTypt, rtp_sock);
        return status;
    }
    
    chan->sockfd      = rtp_sock;
    chan->recv_cb     = recv_cb;
    chan->work_recv   = work_recv;
    chan->is_rtcp     = is_rtcp;
    
    //create for rtp
    if(!is_rtcp) {
        //create packets list
        packet_list_create(&chan->send_list);
        chan->send_list.pack_type = H264_PACKET;
        
        //create resend
        chan->resend = create_resend_buf();
    }
    
    log_debug("transport_channel_create %s done sock:%d", sockTypt, rtp_sock);
    
    return status;
}

pj_status_t transport_udp_create( struct transport_udp* udpout, const char *localAddr, unsigned short localPort,
									on_recv_data rtp_cb, on_recv_data rtcp_cb)
{
    pj_status_t status = 0;
	//int rtp_sock,rtcp_sock;

    struct transport_udp* udp = (struct transport_udp*)udpout;
    memset(&udp->rtp_chanel, 0, sizeof(struct trans_channel));//set rtp_chanel zero
    memset(&udp->rtcp_chanel, 0, sizeof(struct trans_channel));//set rtcp_chanel zero
    
    //important set user_udp
    udp->rtp_chanel.is_rtcp  = PJ_FALSE;
    udp->rtp_chanel.user_udp = udp;
    udp->rtcp_chanel.is_rtcp  = PJ_TRUE;
    udp->rtcp_chanel.user_udp = udp;
    
    transport_channel_create(&udp->rtp_chanel, localAddr, localPort, PJ_FALSE, rtp_cb, worker_rtp_recvfrom);
    transport_channel_create(&udp->rtcp_chanel, localAddr, localPort+1, PJ_TRUE, rtcp_cb, worker_rtcp_recvfrom);
    

	//mutex lock initiate
    pthread_mutex_init(&udp->rtp_cache_mutex, NULL);
    pthread_mutex_init(&udp->udp_socket_mutex, NULL);


	//log_debug("transport_udp_create done rtp_sock:%d rtcp_sock:%d", rtp_sock, rtcp_sock);

    return status;
}

pj_status_t transport_channel_destroy( struct trans_channel*chan)
{
    pj_status_t status = 0;
    if(chan->sockfd != PJ_INVALID_SOCKET) {
        close(chan->sockfd);
        chan->sockfd = PJ_INVALID_SOCKET;
    }
    
    //for rtp
    if(!chan->is_rtcp) {
        packet_list_destroy(&chan->send_list);
        
        //release resend
        if(chan->resend)
            release_resend_buf(chan->resend);
    }

    return status;
}

pj_status_t transport_udp_destroy( struct transport_udp* udp) {
    pj_status_t status = 0;

    transport_channel_destroy(&udp->rtp_chanel);
    transport_channel_destroy(&udp->rtcp_chanel);

	pthread_mutex_destroy(&udp->rtp_cache_mutex);
    pthread_mutex_destroy(&udp->udp_socket_mutex);

	//free(udp);
	//udp = NULL;

    return status;
}


pj_status_t transport_channel_start( struct trans_channel*chan, const char*remoteAddr, unsigned short remoteRtpPort, char*name)
{
    pj_status_t status = 0;
    socklen_t addr_len = sizeof(struct sockaddr_in);
    struct sockaddr_in *sock_addr = &chan->remote_addr;
    memset(sock_addr, 0, addr_len);
    sock_addr->sin_family = AF_INET;
    sock_addr->sin_port   = htons(remoteRtpPort);        //rtp_addr(sock->sin_port)
    sock_addr->sin_addr.s_addr = inet_addr(remoteAddr);  //inet_ntoa(rtp_addr->sin_addr) get ip string
    
    memset(&(chan->recv_tid), 0, sizeof(pj_thread_t));
    
    pj_thread_create(name, (thread_proc *)chan->work_recv, chan->user_udp, 0, 0, &chan->recv_tid);
    if (status != 0) {
        log_error("pj_thread_create create failed name:%s", name);
        return status;
    }
    
    chan->attached = PJ_TRUE;
    
    return status;
}

pj_status_t transport_udp_start( struct transport_udp* udp, const char*remoteAddr, unsigned short remoteRtpPort) {
    pj_status_t status = 0;

    transport_channel_start(&udp->rtp_chanel, remoteAddr, remoteRtpPort, "rtp_thread");
    transport_channel_start(&udp->rtcp_chanel, remoteAddr, remoteRtpPort+1, "rtcp_thread");

    return status;
}

pj_status_t transport_channel_stop( struct trans_channel*chan)
{
    pj_status_t status = 0;
    chan->recv_tid.thread_quit     = 1;
    return status;
}

pj_status_t transport_udp_stop( struct transport_udp* udp) {
    pj_status_t status = 0;
    transport_channel_stop(&udp->rtp_chanel);
    transport_channel_stop(&udp->rtcp_chanel);
    return status;
}

ssize_t transport_channel_send(struct trans_channel *rt_channel, void *rtpPacket, pj_uint32_t size)
{
    ssize_t status        = 0;
    pjmedia_rtp_hdr *head = NULL;
    socklen_t addr_len =sizeof(struct sockaddr_in);
    
    //only rtp will save the package that hadnot been sent
    if(!rt_channel->is_rtcp) {
        rtp_sendto_thread_list_node *rtp_node = rt_channel->send_list.list_current_send;
        while(rtp_node && rt_channel->sockfd != PJ_INVALID_SOCKET)
        {
            size_t send_len = rtp_node->rtp_buf_size;
            if(send_len != 0)
            {
                //mutex_lock(udp->udp_socket_mutex);
                status = sendto(rt_channel->sockfd, rtp_node->rtp_buf, send_len, 0,
                                (const struct sockaddr *)&rt_channel->remote_addr , addr_len);
                head = (pjmedia_rtp_hdr*)(rtp_node->rtp_buf);
                if(status<0) {
                    log_error("sendto rtp packet:[%u] failure, send_len:%d rtp seq:[%u], errno:[%d] errstr:%s", rt_channel->send_list.list_send_size,  send_len, pj_ntohs(head->seq), errno, strerror(errno));
                }
                //else continue;
                log_debug("sendto list packet size:%d seq:%d", send_len, pj_ntohs(head->seq));
            }else
            {
                log_error("sendto packet send_len is:%d seq:%d", send_len, pj_ntohs(head->seq));
            }
            packet_list_node_offset(&rt_channel->send_list);//remove current sending packet
            
            usleep(500);
            rtp_node = rt_channel->send_list.list_current_send;//next sending packet
            
        }//check nodes done
    }

    
//    if(status == EAGAIN)
//        return (pj_status_t)status;
    
    //pthread_mutex_lock(&udp->udp_socket_mutex);
    
    head = (pjmedia_rtp_hdr*)(rtpPacket);
//    if(pj_ntohs(head->seq)!=0)
//    if((pj_ntohs(head->seq)%10)==0 || (pj_ntohs(head->seq)%10)==1 || (pj_ntohs(head->seq)%10)==2)
//        return size;
    
    status = sendto(rt_channel->sockfd, rtpPacket, size, 0,
                    (const struct sockaddr *)&rt_channel->remote_addr, addr_len);
    if(status < 0) {
        log_error("send_priority sendto failed sockid:%d seq:%d errno:%d", rt_channel->sockfd, pj_ntohs(head->seq), errno);
        if(!rt_channel->is_rtcp) {
            switch(errno)
            {
                case EAGAIN:
                    if(packet_list_check_overflow(rt_channel->send_list.list_send_size,
                                                  rt_channel->send_list.list_write_size, RTP_LIST_MAX_SIZE))
                    {
                        packet_list_reset(&rt_channel->send_list);
                        log_debug("---reset---\n");
                    }
                    
                    packet_list_node_add(&rt_channel->send_list, rtpPacket, size);
                    log_debug("save_packet ext:%d size:%d\n", head->x, rt_channel->send_list.list_write_size);
                    break;
            }
        }//if
        usleep(500);
    }
    else
    {
        //usleep(2);
        log_debug("send_priority sendto sockid:%d status:%d seq:%d",rt_channel->sockfd, status, pj_ntohs(head->seq));
    }
    
    return status;
}

pj_status_t transport_priority_send_rtp( transport_udp *udp,
							  const void *rtpPacket, pj_uint32_t size)
{
	ssize_t status        = 0;
    pjmedia_rtp_hdr *head = (pjmedia_rtp_hdr*)(rtpPacket);
    struct trans_channel *rt_channel = &udp->rtp_chanel;
    
    status = transport_channel_send(rt_channel, (char*)rtpPacket, size);
    
    resend_save_rtp(rt_channel->resend, pj_ntohs(head->seq), (char*)rtpPacket, size);//save packet to nack
    
	return (pj_status_t)status;
}

pj_status_t transport_send_rtcp(struct transport_udp*udp, const void *rtpPacket, pj_uint32_t size)
{
    ssize_t status = 0;
    struct trans_channel *rt_channel = &udp->rtcp_chanel;//rtcp channel

    status = transport_channel_send(rt_channel, (char*)rtpPacket, size);
    
    return (pj_status_t)status;
}

pj_status_t transport_save_packet( struct transport_udp*udp, const void *rtpPacket, pj_uint32_t size) {
    pj_status_t status = 0;
    
    struct trans_channel *rtp_channel = &udp->rtp_chanel;
    pjmedia_rtp_hdr *head = (pjmedia_rtp_hdr *)rtpPacket;
    if(packet_list_check_overflow(rtp_channel->send_list.list_send_size,
                                  rtp_channel->send_list.list_write_size, RTP_LIST_MAX_SIZE))
    {
        packet_list_reset(&rtp_channel->send_list);
        log_debug("---reset---\n");
    }
    log_debug("save_packet ext:%d size:%d\n", head->x, rtp_channel->send_list.list_write_size);

    //pthread_mutex_lock(&tp->rtp_cache_mutex);
    packet_list_node_add(&rtp_channel->send_list, rtpPacket, size);
    //pthread_mutex_unlock(&tp->rtp_cache_mutex);
    
    return status;
}

pj_status_t  transport_reset_socket(struct transport_udp*  tp) {
    pj_status_t status = 0;
    return status;
}

pj_status_t transport_reset_rtp_socket(struct transport_udp*  tp) {
    pj_status_t status = 0;
    return status;
}




