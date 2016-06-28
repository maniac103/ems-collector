CC = g++
CFLAGS = -Wall -c -O2 -std=c++0x -DHAVE_DAEMONIZE

LIBS = -lpthread -lboost_system -lboost_program_options
SRCS = main.cpp IoHandler.cpp SerialHandler.cpp SendingSerialHandler.cpp \
       TcpHandler.cpp CommandHandler.cpp ApiCommandParser.cpp \
       CommandScheduler.cpp DataHandler.cpp EmsMessage.cpp \
       ValueApi.cpp ValueCache.cpp Options.cpp PidFile.cpp
OBJS = $(SRCS:%.cpp=%.o)
DEPFILE = .depend

# Uncomment the following lines to build the collector with MySQL database
# support. You'll need to have the development package of libmysql++ installed.
# SRCS += Database.cpp
# CFLAGS += -DHAVE_MYSQL -I/usr/include/mysql
# LIBS += -lmysqlpp

# Uncomment the following line in order to build the collector with support
# for the 'raw read' and 'raw write' commands.
# CFLAGS += -DHAVE_RAW_READWRITE_COMMAND

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

