CC = g++
CFLAGS = -Wall -c -O2 -I/usr/include/mysql -I../framer
LIBS = -lpthread -lboost_system -lboost_thread-mt -lboost_program_options -lmysqlpp
SRCS = main.cpp IoHandler.cpp SerialHandler.cpp TcpHandler.cpp Message.cpp Database.cpp Options.cpp PidFile.cpp
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
	$(CC) $(LIBS) -o collectord $(OBJS)

%.o: %.cpp
	$(CC) $(CFLAGS) $<

