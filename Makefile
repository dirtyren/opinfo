#CCFlags para HPUX
#CCFLAGS = -Wall -g -D_GNUC_ -D_REENTRANT -I/usr/local/pgsql/include

#CCFlags para Linux
# debug
#CCFLAGS = -Wall -g -D_LINUX_ -O2 -m486 -DCPU=686 -D__DEBUG__ -D_GNUC_ -D_REENTRANT -I/usr/local/pgsql/include
# Producao
CCFLAGS = -Wall -g -D_LINUX_ -D_GNUC_ -D_REENTRANT
LIBS = -lz -lmysqlclient -L/usr/lib/mysql
LIBS2 = -lz


all: opinfo-client opinfo-server opinfo-gendns opinfo-monitor

# Compile O files into executables
opinfo-server: opinfo-server.o
	gcc $(CCFLAGS) $(LIBS) -o opinfo-server opinfo-server.o

opinfo-client: opinfo-client.o
	gcc $(CCFLAGS) $(LIBS2) -o opinfo-client opinfo-client.o

opinfo-gendns: opinfo-gendns.o
	gcc $(CCFLAGS) $(LIBS) -o opinfo-gendns opinfo-gendns.o

opinfo-monitor: opinfo-monitor.o
	gcc $(CCFLAGS) $(LIBS) -o opinfo-monitor opinfo-monitor.o
	
opinfo-server.o: opinfo-server.c
	gcc -c opinfo-server.c

opinfo-client.o: opinfo-client.c
	gcc -c opinfo-client.c

opinfo-gendns.o: opinfo-gendns.c
	gcc -c opinfo-gendns.c

opinfo-monitor.o: opinfo-monitor.c
	gcc -c opinfo-monitor.c

install: opinfo-client
	cp -f opinfo-server /usr/local/opinfo/.
	cp -f opinfo-client /usr/local/opinfo/.
	cp -f opinfo-gendns /usr/local/opinfo/.
	cp -f opinfo-monitor /usr/local/opinfo/.
	cp -f sendMail.pl /usr/local/opinfo/.

clean:
	rm -f opinfo-client opinfo-server opinfo-gendns opinfo-monitor *.o *.DBG *.ERR *.OLD *.COM core *.txt *.log *.lock
