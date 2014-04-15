CC = g++
CFLAGS = -Wall -c -O2 -I/usr/include/mysql -std=c++0x
#CFLAGS += -DHAVE_RAW_READWRITE_COMMAND
LIBS = -lpthread -lboost_system -lboost_thread -lboost_program_options -lmysqlpp
SRCS = main.cpp IoHandler.cpp SerialHandler.cpp TcpHandler.cpp CommandHandler.cpp \
       DataHandler.cpp EmsMessage.cpp Database.cpp ValueApi.cpp ValueCache.cpp \
       Options.cpp PidFile.cpp
OBJS = $(SRCS:%.cpp=%.o)
DEPFILE = .depend

all: collectord

clean:
	rm -f collectord
	rm -f *.o
	rm -f $(DEPFILE)

$(DEPFILE): $(SRCS)
	$(CC) $(CFLAGS) -MM $(SRCS) > $(DEPFILE)

-include $(DEPFILE)

collectord: $(OBJS) $(DEPFILE) Makefile
	$(CC) -o collectord $(OBJS) $(LIBS)

%.o: %.cpp
	$(CC) $(CFLAGS) $<

