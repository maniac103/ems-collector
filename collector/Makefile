CC = g++
CFLAGS = -Wall -c -O2 -I/usr/include/mysql -std=c++0x -DHAVE_MYSQL -DHAVE_DAEMONIZE
#CFLAGS += -DHAVE_RAW_READWRITE_COMMAND

LIBS = -lpthread -lboost_system -lboost_program_options -lmysqlpp
SRCS = main.cpp IoHandler.cpp SerialHandler.cpp TcpHandler.cpp CommandHandler.cpp \
       CommandScheduler.cpp DataHandler.cpp EmsMessage.cpp Database.cpp \
       ValueApi.cpp ValueCache.cpp Options.cpp PidFile.cpp
OBJS = $(SRCS:%.cpp=%.o)
DEPFILE = .depend

# Uncomment the following lines to build the collector with MQTT support.
# If you want this, you need to clone the following repo to some directory
# of your choice: git://github.com/redboltz/mqtt_client_cpp
#
# Then, adapt the include path below to point towards the MQTT client
# include path. The default assumes that both ems-collector and
# mqtt_client_cpp are cloned into the same top level path.
#
# CFLAGS += -I../../mqtt_client_cpp/include -DMQTT_NO_TLS -DHAVE_MQTT
# SRCS += MqttAdapter.cpp

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

