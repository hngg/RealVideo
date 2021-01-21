

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
                              size, channel.is_rtcp, inet_ntoa(channel.rem_addr.sin_addr), htons(channel.rem_addr.sin_port));
                }
            }
        }//rtcp sr heartbeat
        
        /* add by j33783 20190621 begin */
        timeout.tv_sec  = 0;
        timeout.tv_usec = 100000;//100ms
        FD_ZERO(&recv_set);
        FD_SET(recv_channel->sockfd, &recv_set);
        status = select(recv_channel->sockfd + 1, &recv_set, NULL, NULL, &timeout);

        if(status>0)
        {
           if(FD_ISSET(recv_channel->sockfd, &recv_set)) {
               bytes_read = RTP_LEN;
               status = recvfrom(recv_channel->sockfd,  recv_channel->recv_buf, bytes_read, 0,
                               (struct sockaddr *)&recv_channel->src_addr, &addr_len);
               
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
            usleep(10000);//10ms
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
    
    pVidbuf->buf_size = RESEND_BUFF_NUMBER;//the number of resend package number

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

<<<<<<< HEAD
pj_status_t resend_losted_package( struct transport_udp* udp, unsigned begin_seq, unsigned count)
{
    ssize_t status = -1;
    int i = 0, seq = begin_seq;
    if(!udp || count >= RESEND_BUFF_NUMBER)
        return -1;
    
    socklen_t addr_len =sizeof(struct sockaddr_in);
    pjmedia_vid_buf *resendBuf = udp->resend;
    struct trans_channel *rt_channel = &udp->rtp_chanel;
    for(; i<count; i++)
    {
        pj_uint16_t index = seq++ % resendBuf->buf_size;
        void* sendPos = resendBuf->buf + index * PJMEDIA_MAX_MTU;
        int sendLen   =  resendBuf->pkt_len[index];
        
        status = sendto(rt_channel->sockfd, sendPos, sendLen, 0,
                        (const struct sockaddr *)&rt_channel->rem_addr, addr_len);
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

=======
>>>>>>> d0e83e775b61c141acf0f986720c005b7d0f6a80
pj_status_t transport_udp_create( struct transport_udp** udpout, const char *localAddr, unsigned short rtpPort,
									void (*rtp_cb)(void*, void*, pj_ssize_t),
				                    void (*rtcp_cb)(void*, void*, pj_ssize_t))
{
    pj_status_t status = 0;
	int rtp_sock,rtcp_sock; 
	int optVal = 1;

    struct transport_udp* udp = (struct transport_udp*)malloc(sizeof(struct transport_udp));
    socklen_t addr_len = sizeof(struct sockaddr_in);
    memset(&udp->rtp_chanel, 0, sizeof(struct trans_channel));//set rtp_chanel zero
    memset(&udp->rtcp_chanel, 0, sizeof(struct trans_channel));//set rtcp_chanel zero

    
	//create rtp
    rtp_sock = socket(AF_INET, SOCK_DGRAM, 0);
	log_debug("create port:%d rtpsock:%d", rtpPort, rtp_sock);
    if(rtp_sock < 0)
        return -1;

	setsockopt(rtp_sock, SOL_SOCKET, SO_REUSEADDR, (const void *)&optVal, sizeof(optVal));

    struct timeval tv = {3, 0};
	setsockopt(rtp_sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(struct timeval));
	setsockopt(rtp_sock, SOL_SOCKET, SO_SNDTIMEO, (char*)&tv, sizeof(struct timeval));
    
    unsigned nSendBuf = 2048*1024;
    unsigned nRecvBuf = 2048*1024;
    setsockopt(rtp_sock, SOL_SOCKET,SO_SNDBUF, (const char*)&nSendBuf, sizeof(int));
    setsockopt(rtp_sock, SOL_SOCKET,SO_RCVBUF, (const char*)&nRecvBuf, sizeof(int));
    
    memset(&udp->local_rtp_addr, 0, addr_len);
    udp->local_rtp_addr.sin_family = AF_INET;
    udp->local_rtp_addr.sin_port   = htons(rtpPort);
    udp->local_rtp_addr.sin_addr.s_addr = inet_addr(localAddr);//htonl(INADDR_ANY);
	
    status = bind(rtp_sock, (struct sockaddr *)&udp->local_rtp_addr, sizeof(struct sockaddr_in));
	if(status < 0)
    	log_error("bind rtp failed.\n");
		
    
	//create rtcp 
    rtcp_sock = socket(AF_INET, SOCK_DGRAM, 0);
	log_debug("create rtcp_sock:%d", rtcp_sock);
    if(rtcp_sock<0)
        return -1;
	optVal = 1;
	setsockopt(rtcp_sock, SOL_SOCKET, SO_REUSEADDR, (const void *)&optVal, sizeof(optVal));

    memset(&udp->local_rtcp_addr, 0, addr_len);
    udp->local_rtcp_addr.sin_family = AF_INET;
    udp->local_rtcp_addr.sin_port 	= htons(rtpPort+1);
    udp->local_rtcp_addr.sin_addr.s_addr = inet_addr(localAddr);//htonl(INADDR_ANY);
    
    status = bind(rtcp_sock, (struct sockaddr *)&udp->local_rtcp_addr, sizeof(struct sockaddr_in));
	if(status < 0)
        log_error("bind rtcp failed.\n");

    udp->rtp_chanel.sockfd   = rtp_sock;
    udp->rtp_chanel.recv_cb  = rtp_cb;
    udp->rtcp_chanel.sockfd  = rtcp_sock;
    udp->rtcp_chanel.recv_cb = rtcp_cb;
    udp->rtcp_chanel.is_rtcp = PJ_TRUE;
    
    //create packets list
    packet_list_create(&udp->rtp_chanel.send_list);
    udp->rtp_chanel.send_list.pack_type = H264_PACKET;
    

	//create resend
	udp->resend = create_resend_buf();

	//mutex lock initiate
    pthread_mutex_init(&udp->rtp_cache_mutex, NULL);
    pthread_mutex_init(&udp->udp_socket_mutex, NULL);


    *udpout = udp;

	log_debug("transport_udp_create done rtp_sock:%d rtcp_sock:%d", rtp_sock, rtcp_sock);

    return status;
}

pj_status_t transport_udp_destroy( struct transport_udp* udp) {
    pj_status_t status = 0;


    if(udp->rtp_chanel.sockfd != PJ_INVALID_SOCKET) {
        close(udp->rtp_chanel.sockfd);
        udp->rtp_chanel.sockfd = PJ_INVALID_SOCKET;
    }

    if(udp->rtcp_chanel.sockfd != PJ_INVALID_SOCKET) {
        close(udp->rtcp_chanel.sockfd);
        udp->rtcp_chanel.sockfd = PJ_INVALID_SOCKET;
    }
    
    packet_list_destroy(&udp->rtp_chanel.send_list);
    
	//release resend
<<<<<<< HEAD
    if(udp->resend)
        release_resend_buf(udp->resend);
=======
	release_resend_buf(udp->resend);
>>>>>>> d0e83e775b61c141acf0f986720c005b7d0f6a80

	pthread_mutex_destroy(&udp->rtp_cache_mutex);
    pthread_mutex_destroy(&udp->udp_socket_mutex);

	free(udp);
	udp = NULL;

    return status;
}

pj_status_t transport_udp_start( struct transport_udp* udp, const char*remoteAddr, unsigned short remoteRtpPort) {
    pj_status_t status = 0;

    socklen_t addr_len = sizeof(struct sockaddr_in);
    struct sockaddr_in *rtp_addr = &udp->rtp_chanel.rem_addr;
    memset(rtp_addr, 0, addr_len);
    rtp_addr->sin_family = AF_INET;
    rtp_addr->sin_port   = htons(remoteRtpPort);        //rtp_addr(sock->sin_port)
    rtp_addr->sin_addr.s_addr = inet_addr(remoteAddr);  //inet_ntoa(rtp_addr->sin_addr) get ip string
    
    struct sockaddr_in *rtcp_addr = &udp->rtcp_chanel.rem_addr;
    memset(rtcp_addr, 0, addr_len);
    rtcp_addr->sin_family = AF_INET;
    rtcp_addr->sin_port   = htons(remoteRtpPort+1);
    rtcp_addr->sin_addr.s_addr = inet_addr(remoteAddr);

    udp->rtp_chanel.user_udp  = udp;
    udp->rtcp_chanel.user_udp = udp;
    
    memset(&(udp->rtp_chanel.recv_tid), 0, sizeof(pj_thread_t));
    memset(&(udp->rtcp_chanel.recv_tid), 0, sizeof(pj_thread_t));
<<<<<<< HEAD
=======

    pj_thread_create("rtp_recv", (thread_proc *)&worker_rtp_recvfrom, udp, 0, 0, &udp->rtp_chanel.recv_tid);
    pj_thread_create("rtcp_recv", (thread_proc *)&worker_rtcp_recvfrom, udp, 0, 0, &udp->rtcp_chanel.recv_tid);
    if (status != 0) {
        return status;
    }
    
	udp->attached = PJ_TRUE;
>>>>>>> d0e83e775b61c141acf0f986720c005b7d0f6a80

    pj_thread_create("rtp_recv", (thread_proc *)&worker_rtp_recvfrom, udp, 0, 0, &udp->rtp_chanel.recv_tid);
    pj_thread_create("rtcp_recv", (thread_proc *)&worker_rtcp_recvfrom, udp, 0, 0, &udp->rtcp_chanel.recv_tid);
    if (status != 0) {
        return status;
    }
    
	udp->attached = PJ_TRUE;

<<<<<<< HEAD
    return status;
}

pj_status_t transport_udp_stop( struct transport_udp* udp) {
    pj_status_t status = 0;
	udp->rtp_chanel.recv_tid.thread_quit 	= 1;
	udp->rtcp_chanel.recv_tid.thread_quit 	= 1;
=======
pj_status_t transport_udp_stop( struct transport_udp* udp) {
    pj_status_t status = 0;
	udp->rtp_chanel.recv_tid.thread_quit 	= 1;
	udp->rtcp_chanel.recv_tid.thread_quit 	= 1;
    return status;
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
    
>>>>>>> d0e83e775b61c141acf0f986720c005b7d0f6a80
    return status;
}

ssize_t transport_channel_send(struct trans_channel *rt_channel, void *rtpPacket, pj_uint32_t size)
{
    ssize_t status        = 0;
    pjmedia_rtp_hdr *head = NULL;
    socklen_t addr_len =sizeof(struct sockaddr_in);
    rtp_sendto_thread_list_node *rtp_node = rt_channel->send_list.list_current_send;
    while(rtp_node && rt_channel->sockfd != PJ_INVALID_SOCKET)
    {
        size_t send_len = rtp_node->rtp_buf_size;
        if(send_len != 0)
        {
            //mutex_lock(udp->udp_socket_mutex);
            status = sendto(rt_channel->sockfd, rtp_node->rtp_buf, send_len, 0,
                            (const struct sockaddr *)&rt_channel->rem_addr , addr_len);
<<<<<<< HEAD
            head = (pjmedia_rtp_hdr*)(rtp_node->rtp_buf);
            if(status<0) {
=======
            if(status<0) {
                head = (pjmedia_rtp_hdr*)(rtp_node->rtp_buf);
>>>>>>> d0e83e775b61c141acf0f986720c005b7d0f6a80
                log_error("sendto rtp packet:[%u] failure, send_len:%d rtp seq:[%u], errno:[%d] errstr:%s", rt_channel->send_list.list_send_size,  send_len, pj_ntohs(head->seq), errno, strerror(errno));
            }
            //else continue;
            log_debug("sendto list packet size:%d seq:%d", send_len, pj_ntohs(head->seq));
        }else
        {
            log_error("sendto packet send_len is:%d seq:%d", send_len, pj_ntohs(head->seq));
        }
        packet_list_node_offset(&rt_channel->send_list);//remove current sending packet
<<<<<<< HEAD
        
        usleep(500);
        rtp_node = rt_channel->send_list.list_current_send;//next sending packet
        
=======
        
        usleep(1000);
        rtp_node = rt_channel->send_list.list_current_send;//next sending packet
        
>>>>>>> d0e83e775b61c141acf0f986720c005b7d0f6a80
    }//check nodes done
    
//    if(status == EAGAIN)
//        return (pj_status_t)status;
    
    //pthread_mutex_lock(&udp->udp_socket_mutex);
    
<<<<<<< HEAD
    head = (pjmedia_rtp_hdr*)(rtpPacket);
//    if(pj_ntohs(head->seq)!=0)
//    if((pj_ntohs(head->seq)%10)==0 || (pj_ntohs(head->seq)%10)==1 || (pj_ntohs(head->seq)%10)==2)
//        return size;
    
    status = sendto(rt_channel->sockfd, rtpPacket, size, 0,
                    (const struct sockaddr *)&rt_channel->rem_addr, addr_len);
    if(status < 0) {
        log_error("send_priority sendto failed sockid:%d seq:%d errno:%d", rt_channel->sockfd, pj_ntohs(head->seq), errno);
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
        usleep(500);
=======
    //pthread_mutex_lock(&udp->udp_socket_mutex);
    head = (pjmedia_rtp_hdr*)(rtpPacket);
    status = sendto(rt_channel->sockfd, rtpPacket, size, 0,
                    (const struct sockaddr *)&rt_channel->rem_addr, addr_len);
    if(status == EAGAIN) {
        if(packet_list_check_overflow(rt_channel->send_list.list_send_size,
                                      rt_channel->send_list.list_write_size, RTP_LIST_MAX_SIZE))
        {
            packet_list_reset(&rt_channel->send_list);
            log_debug("---reset---\n");
        }
        log_debug("save_packet ext:%d size:%d\n", head->x, rt_channel->send_list.list_write_size);
        packet_list_node_add(&rt_channel->send_list, rtpPacket, size);
        log_error("sendto rtp packet failed status:%d", status);
>>>>>>> d0e83e775b61c141acf0f986720c005b7d0f6a80
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
    
    return status;
}

pj_status_t transport_priority_send_rtp( transport_udp *udp,
							  const void *rtpPacket, pj_uint32_t size)
{
	ssize_t status        = 0;
    pjmedia_rtp_hdr *head = (pjmedia_rtp_hdr*)(rtpPacket);
    struct trans_channel *rt_channel = &udp->rtp_chanel;
    
    status = transport_channel_send(rt_channel, (char*)rtpPacket, size);
    
    resend_save_rtp(udp->resend, pj_ntohs(head->seq), (char*)rtpPacket, size);//save packet to nack
    
	return (pj_status_t)status;
}

pj_status_t transport_send_rtcp(struct transport_udp*udp, const void *rtpPacket, pj_uint32_t size)
{
    ssize_t status = 0;
    struct trans_channel *rt_channel = &udp->rtcp_chanel;//rtcp channel

    status = transport_channel_send(rt_channel, (char*)rtpPacket, size);
    
    return (pj_status_t)status;
}

<<<<<<< HEAD

//

=======
>>>>>>> d0e83e775b61c141acf0f986720c005b7d0f6a80
pj_status_t transport_send_rtp_seq(struct transport_udp*udp, const void *rtpPacket, pj_size_t size, unsigned short extSeq) {
    pj_status_t status = 0;
    
    //pjmedia_rtp_hdr *head = (pjmedia_rtp_hdr *)rtpPacket;
//    if(packet_list_check_overflow(udp->rtp_thread_list_header.list_send_size, udp->rtp_thread_list_header.list_write_size, RTP_LIST_MAX_SIZE))
//    {
//        packet_list_reset(&udp->rtp_thread_list_header);
//        log_debug("---reset---\n");
//    }
//    pthread_mutex_lock(&udp->rtp_cache_mutex);
//    packet_list_node_add(&udp->rtp_thread_list_header, rtpPacket, (pj_uint32_t)size);
//    resend_save_rtp( udp->resend, extSeq, (char*)rtpPacket, size);
//    pthread_mutex_unlock(&udp->rtp_cache_mutex);

    return status;
}

<<<<<<< HEAD
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

=======
>>>>>>> d0e83e775b61c141acf0f986720c005b7d0f6a80
pj_status_t  transport_reset_socket(struct transport_udp*  tp) {
    pj_status_t status = 0;
    return status;
}

pj_status_t transport_reset_rtp_socket(struct transport_udp*  tp) {
    pj_status_t status = 0;
    return status;
}




