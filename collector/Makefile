CC = g++
CFLAGS = -Wall -c -O2 -I/usr/include/mysql
LIBS = -lboost_system -lboost_thread-mt -lboost_program_options -lmysqlpp
SRCS = main.cpp SerialHandler.cpp Message.cpp Database.cpp Options.cpp
OBJS = $(SRCS:%.cpp=%.o)
DEPFILE = .depend

all: collectord

clean:
	rm -f collectord
	rm -f *.o
	rm -f $(DEPFILE)

$(DEPFILE): $(SRCS)
	$(CC) -MM $(SRCS) > $(DEPFILE)

-include $(DEPFILE)

collectord: $(OBJS) $(DEPFILE) Makefile
	$(CC) $(LIBS) -o collectord $(OBJS)

%.o: %.cpp
	$(CC) $(CFLAGS) $<

