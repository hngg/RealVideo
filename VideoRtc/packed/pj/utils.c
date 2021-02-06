#include "utils.h"
#include <stdio.h>
#include "video_rtc_api.h"


long get_currenttime_us() {
	struct timeval tv_cur;
	gettimeofday(&tv_cur, NULL);
	return tv_cur.tv_sec*1000000 + tv_cur.tv_usec;
}

long get_timeofday_us(const struct timeval *tval) {
    if(tval==NULL)
        return 0;
    return  tval->tv_sec*1000000 + tval->tv_usec;
}


pj_ssize_t getCurrentTimeMs()
{
    struct timeval tv_cur;
    gettimeofday(&tv_cur, NULL);
    return tv_cur.tv_sec*1000 + tv_cur.tv_usec/1000;
}

/**
 * Fill the memory location with zero.
 *
 * @param dst        The destination buffer.
 * @param size        The number of bytes.
 */
void pj_bzero(void *dst, pj_size_t size)
{
#if defined(PJ_HAS_BZERO) && PJ_HAS_BZERO!=0
    bzero(dst, size);
#else
    memset(dst, 0, size);
#endif
}

void* pj_memcpy(void *dst, const void *src, pj_uint32_t size)
{
    return memcpy(dst, src, size);
}

/*
 * Convert 16-bit value from network byte order to host byte order.
 */
pj_uint16_t pj_ntohs(pj_uint16_t netshort)
{
    return ntohs(netshort);
}

pj_uint16_t pj_htons(pj_uint16_t netshort)
{
    return htons(netshort);
}


RTC_API
/*
    sample:src = "a=001"; char mainType[M][N] = {0};
    splitStr(src, "=", mainType);mainType[0] = "a"

    successful:return the split count, else return 0
*/
int splitStr(char *src, char *format, char (*substr)[COLUMN])
{
    char * begin;
    char * pos;
    int substrLen, count = 0;
    int formatLen = (int)strlen(format);

    for(count=0, begin=src; ( pos = (char*)strstr(begin, format) ) != NULL;)
    {
        substrLen = (int)(pos - begin);
        strncpy(substr[count], begin, substrLen);
        begin = pos + formatLen;
        count++;
        //should not out of memory
        if(count+1>=ROW) {
            printf("error:out of max array.\n");
            break;
        }
    }
    substrLen = (int)strlen(begin);
    if(substrLen > 0){
        strncpy(substr[count], begin, substrLen);
        count++;
    }
    return count;
}

RTC_API
/*
    sameple:
    src = "a=001&b=002&c=003"; char outValue[100] = {0};
    getSubkeyValue(src, "&", "=", "a", outValue); //then the outValue is 001
 
    successful:return 1 else return 0
*/
int getSubkeyValue(char *src, char*format, char*subform, char*inKey, char*outValue)
{
    int result = 0;
    char mainType[ROW][COLUMN] = {};
    
    int j = 0, i = splitStr(src, format, mainType);
    for(; j<i; j++)
    {
        //printf("%s\n", mainType[j]);
        char subType[ROW][COLUMN] = {};
        int m = splitStr(mainType[j], subform, subType);
        if(m==2)
        {
            if(!strcmp(subType[0], inKey))
            {
                strcpy(outValue, subType[1]);
                result = 1;
                break;
            }
            //printf("key:%s value:%s\n", subType[0], subType[1]);
        }
    }
    return result;
}

RTC_API
char* getVersion(void) {
    return VERSION;
}
