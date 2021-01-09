#include "glog.h"

#include <stdio.h>
#include <stdarg.h>

#if PJ_LOG_MAX_LEVEL >= 0
PJ_DEF(void) pj_log_0(const char *obj, const char *format, ...)
{
    va_list arg;
    va_start(arg, format);
    pj_log(obj, 0, format, arg);
    va_end(arg);
}
#endif

#if PJ_LOG_MAX_LEVEL >= 1
PJ_DEF(void) pj_log_1(const char *obj, const char *format, ...)
{
    va_list arg;
    va_start(arg, format);
    pj_log(obj, 1, format, arg);
    va_end(arg);
}
#endif	/* PJ_LOG_MAX_LEVEL >= 1 */

#if PJ_LOG_MAX_LEVEL >= 2
PJ_DEF(void) pj_log_2(const char *obj, const char *format, ...)
{
    va_list arg;
    va_start(arg, format);
    pj_log(obj, 2, format, arg);
    va_end(arg);
}
#endif

#if PJ_LOG_MAX_LEVEL >= 3
PJ_DEF(void) pj_log_3(const char *obj, const char *format, ...)
{
    va_list arg;
    va_start(arg, format);
    pj_log(obj, 3, format, arg);
    va_end(arg);
}
#endif

#if PJ_LOG_MAX_LEVEL >= 4
PJ_DEF(void) pj_log_4(const char *obj, const char *format, ...)
{
    va_list arg;
    va_start(arg, format);
    pj_log(obj, 4, format, arg);
    va_end(arg);
}
#endif

#if PJ_LOG_MAX_LEVEL >= 5
PJ_DEF(void) pj_log_5(const char *obj, const char *format, ...)
{
    va_list arg;
    va_start(arg, format);
    pj_log(obj, 5, format, arg);
    va_end(arg);
}
#endif

#if PJ_LOG_MAX_LEVEL >= 6
PJ_DEF(void) pj_log_6(const char *obj, const char *format, ...)
{
    va_list arg;
    va_start(arg, format);
    pj_log(obj, 6, format, arg);
    va_end(arg);
}
#endif


void pj_log( const char *obj, int level, const char *format, va_list marker)
{
	const char *ltexts[] = { "FATAL:", "ERROR:", " WARN:", " INFO:", "DEBUG:", "TRACE:", "DETRC:"};
    char log_buffer[LOG_MAX_SIZE]= {0};
	strcpy(log_buffer, ltexts[level]);
    //remember,destination buffer should not be overflow
	vsnprintf(log_buffer+6, LOG_MAX_SIZE-7, format, marker);
	printf("%s\n", log_buffer);
}

// void main()
// {
//     log_debug("%s %d", "mylog test", 123456);
// }
