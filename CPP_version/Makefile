CC = g++ -g  
SERVER_OBJS =  backupRecordService.o SocketUtil.o  Util.o  mp4aac/H264Parser.o ./mp4aac/convert.o ./SQL.o  ./ThreadPool.o  ./Alarm.o
XMLPCLIB = ../xmlrpc/libXmlRpc.a
LIB := -lpthread -Llib -lmxml $(XMLPCLIB) -L./mp4aac/lib -lfdk-aac -laudec_g726 -lmp4v2 -L/usr/lib -lmysqlpp
INCLUDE := -I../xmlrpc/src -Imp4aac/include -I/usr/include/mysql  -I/usr/include/mysql++   -I/usr/include
all: $(SERVER_OBJS) backupRecord
backupRecord:	$(SERVER_OBJS)
	$(CC) -o $@ $^ $(LIB)

%.o:	%.cpp
	@cd mp4aac && make
	$(CC) -c $< $(INCLUDE)

clean:
	@cd mp4aac && make clean
	rm -f *.o backupRecord

install:
	cp backupRecord mp4aac/bsr2mp4 /opt/bstar/backupRecord/ -f
	cp -drf mp4aac/lib/* /opt/bstar/backupRecord/lib/

copy:
	\cp backupRecord /opt/bstar/backupRecord/
