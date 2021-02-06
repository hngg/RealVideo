CXXFLAGS += -std=c++11
 
CC 	= gcc
CPP = g++
AR  = ar

COMPLIE_INCL = -I./ -I./include -I./packed -I./packed/pj

COMPLIE_LIB  = -L./ -lpthread -lm
CFLAGS 		= -Wall -DHAVE_CONFIG_H -DSENDTEST -g -fPIC #-DHAVE_SYS_TIME_H
CXXFLAGS += $(COMPLIE_INCL) 
LDFLAGS += $(COMPLIE_LIB)

CFLAGS  += -I./ -I./packed
CFLAGS	+= $(COMPLIE_INCL)

TARGET:SEND RECV UNITTest

OBJECTS = vid_codec.o vid_port.o transport_udp.o vid_stream.o med_command.o \
		  packed/packets_list.o packed/jitter_buffer.o packed/callCplus.o packed/dealrtpunpack.o \
		  packed/h264_packetizer.o packed/rtp.o packed/rtcp.o \
		  packed/pj/os_core_unix.o packed/pj/utils.o packed/pj/glog.o


RTCLIB=RTCLIB
$(RTCLIB):$(OBJECTS)
	$(AR) rcs librtc.a $(OBJECTS)

##############################test##############################
TESTDIR = test

SEND_OBJ = $(OBJECTS) \
			$(TESTDIR)/NALDecoder.o $(TESTDIR)/vid_stream_test.o $(TESTDIR)/rtc_send.o

RECV_OBJ = $(OBJECTS) \
			$(TESTDIR)/NALDecoder.o $(TESTDIR)/vid_stream_test.o $(TESTDIR)/rtc_recv.o

UNIT_OBJ = $(OBJECTS) \
			$(TESTDIR)/NALDecoder.o $(TESTDIR)/vid_stream_test.o $(TESTDIR)/UnitTest.o

SEND=SEND
RECV=RECV
UNITTest=UNITTest

$(SEND):$(SEND_OBJ) 
	$(CC) -o $(TESTDIR)/$(SEND) $(SEND_OBJ) $(LDFLAGS)

$(RECV):$(RECV_OBJ) 
	$(CC) -o $(TESTDIR)/$(RECV) $(RECV_OBJ) $(LDFLAGS)

$(UNITTest):$(UNIT_OBJ) 
	$(CC) -o $(TESTDIR)/$(UNITTest) $(UNIT_OBJ) $(LDFLAGS)



.PHONY:clean
clean:
	rm  *.o packed/*.o packed/pj/*.o $(TESTDIR)/*.o
