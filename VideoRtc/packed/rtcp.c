

#include "rtcp.h"
#include "utils.h"
#include "os.h"
#include "glog.h"

pj_status_t rtcp_build_rtcp_sr(void *buf, pj_size_t *length)
{
    pjmedia_rtcp_sr_pkt *sr_pkt = (pjmedia_rtcp_sr_pkt*)buf;
    /* Init common RTCP SR header */
    sr_pkt->common.version  = 2;
    sr_pkt->common.count    = 1;
    sr_pkt->common.pt       = RTCP_SR;
    sr_pkt->common.length   = pj_htons(12);
    sr_pkt->rr.lsr = (pj_uint32_t)get_currenttime_us();
    
    *length = sizeof(struct pjmedia_rtcp_sr_pkt);
    log_debug("rtcp sr genaner lsr:%d", sr_pkt->rr.lsr);
    return PJ_SUCCESS;
}

pj_status_t rtcp_build_rtcp_rr(void *buf, pj_size_t *length, int lsr, int dlsr)
{
    pjmedia_rtcp_rr_pkt *rr_pkt = (pjmedia_rtcp_rr_pkt*)buf;
    /* Init common RTCP SR header */
    rr_pkt->common.version  = 2;
    rr_pkt->common.count    = 1;
    rr_pkt->common.pt       = RTCP_RR;
    rr_pkt->common.length   = pj_htons(12);
    rr_pkt->rr.lsr  = lsr;
    rr_pkt->rr.dlsr = dlsr;
    
    *length = sizeof(struct pjmedia_rtcp_rr_pkt);
    
    return PJ_SUCCESS;
}

pj_status_t rtcp_build_rtcp_nack_(  void *buf, pj_size_t *length, unsigned begin_seq, unsigned seq_num)
{
    pjmedia_rtcp_nack_pkg *pkg = (pjmedia_rtcp_nack_pkg *)buf;
    pkg->common.pt  = RTCP_NACK;
    pkg->common.fmt = 1;
    pkg->common.version = 2;
    pkg->common.length  = sizeof(pjmedia_rtcp_nack_pkg);
    pkg->nack.base_seq  = begin_seq;
    pkg->nack.flag      = seq_num;
    
    return PJ_SUCCESS;
}

pj_status_t rtcp_build_rtcp_nack( //pjmedia_rtcp_session *session, 
					    void *buf,
					    pj_size_t *length,
					    const pjmedia_rtcp_nack *nack)
{
    pjmedia_rtcp_nack_common *hdr;
    pj_uint8_t *p;
	pj_uint32_t *p32;
	pj_uint16_t *p16;
    pj_size_t len;

    //PJ_ASSERT_RETURN(session && buf && length && nack, PJ_EINVAL);

    /* Verify buffer length */
    len = sizeof(*hdr);
	len += 8;

    if (len > *length)
	    return -1;//PJ_ETOOSMALL;

    /* Build RTCP SDES header */
    hdr = (pjmedia_rtcp_nack_common*)buf;
    //pj_memcpy(hdr, (pjmedia_rtcp_nack_common *)&session->rtcp_sr_pkt.common,  sizeof(*hdr));
    hdr->pt = RTCP_NACK;
    hdr->length = pj_htons((pj_uint16_t)(len/4 - 1));
	hdr->fmt = 1;

    /* Build RTCP nack items */
    p = (pj_uint8_t*)hdr + sizeof(*hdr);
    p32 = (pj_uint32_t*)p;
	*p32++ = nack->ssrc;
	p16 = (pj_uint16_t*)p32;
	*p16++ = nack->base_seq;
	*p16 = nack->flag;

    /* Null termination */
	p = p + 8;

    /* Finally */
    //pj_assert((int)len == p-(pj_uint8_t*)buf);

    *length = len;

    return PJ_SUCCESS;
}

pj_status_t pjmedia_rtcp_build_fir( void *buf, pj_size_t *length)
{
    pjmedia_rtcp_nack_common *hdr;
    pj_size_t len;
    
    //PJ_ASSERT_RETURN(session && buf && length, PJ_EINVAL);

       /* Verify buffer length */
       len = sizeof(*hdr);
     if (len > *length)
         return -1;

    /* Build RTCP FIR header */
      hdr = (pjmedia_rtcp_nack_common*)buf;
      //pj_memcpy(hdr, &session->rtcp_sr_pkt.common,  sizeof(*hdr));
      hdr->pt = RTCP_FIR;
      hdr->length = pj_htons((pj_uint16_t)(len/4 - 1));

      *length = len;
      return PJ_SUCCESS;
}
