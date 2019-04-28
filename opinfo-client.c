/* 
   opinfo-client
*/
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h> 
#include <sys/socket.h> 
#include <sys/errno.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netinet/tcp.h> 
#include <netdb.h>
#include <stdlib.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/time.h>
#include <errno.h>
#include <zlib.h>
#include <signal.h>

#include "opinfo.h"

extern int h_errno;

#define OPINFO_LOCK_FILE "/var/log/opinfo-client-reboot.lock"
#define OPINFO_RESOLVER_FILE "/tmp/opinfo-resolver-file.txt"

/* constants */
#define	LOG_FILE	"/var/log/opinfo-client.log"

/* Functions prototypes */
int nameResolver(char *hostname, char *ipAddress);
int sendData(char *opInfoServer, int port, char *data, int dataSize);
int connect_timeout(int nSocket, struct sockaddr *sin, int nSize, int timeout);
void getInformation_v10(struct opinfo_proto_v10 *opinfo_proto, int reboot);
void writeLog(int type, char *msg);

int main()
{
   pid_t processPid;
   int   ok,i,nrTimes,timeToSleep,reboot;
   char  temp[1024];
   struct opinfo_proto_v10 opinfo_proto;
   FILE *fp;
   
   sprintf(temp, "Inicializing OpInfo Client V.%s - Server %s", VERSION, SERVER);
   printf("\n-> %s\n",temp);
   writeLog(0, temp);
   
   /* Forks to background*/
   /* Detachs process from terminal */
   setpgrp();
   /* signal to tell the child not to wait for the father to exit */
   signal(SIGCLD, SIG_IGN);
   if ((processPid=fork()) < 0) {
      sprintf(temp, "Could not fork - %s", strerror(errno));
      writeLog(1, temp);
      exit(1);
   }
   if (processPid > 0) {
      exit(0);
   }
   
   /* check for the lcok file */
   if ((access(OPINFO_LOCK_FILE, F_OK))==0) {
      /* the lock file is present, the ISG has rebooted */
      reboot=1;
   }
   else {
      /* There is no lock file, create it */
      fp=fopen(OPINFO_LOCK_FILE, "w");
      fclose(fp);
      reboot=0;
   }
   
   srand((unsigned int)time((time_t *)NULL));
   while(1) {
      getInformation_v10(&opinfo_proto, reboot);
      /* waits for switch acknowledge */
      sleep(5);
      ok=sendData(SERVER, PORT, (char *)&opinfo_proto, sizeof(opinfo_proto));
      if (ok) {
         // Counld not send, tries the backup server
         sprintf(temp, "Could not send to %s, trying %s",SERVER,SERVER2);
         writeLog(0, temp);
         ok=sendData(SERVER2, PORT, (char *)&opinfo_proto, sizeof(opinfo_proto));
         if (ok) {
            timeToSleep=1;
         }
         else {
            timeToSleep=(rand()%300)+1;
         }
      }
      else {
         timeToSleep=(rand()%300)+1;
      }
      sprintf(temp, "Sleeping for %ld seconds",timeToSleep);
      writeLog(0,temp);
      /* After it finds out if the OpNet has rebooted, it clens the flag */
      reboot=0;
      sleep(timeToSleep);
   }
   return(0);
}

/* Name resolver 0 - sucessfull /  1 - error */
int nameResolver(char *hostname, char *ipAddress)
{
	struct hostent *hostinfo;
   pid_t childPid;
	char  temp[1024], tempIP[256];
	FILE	*fp;

   /* deletes resolver file */
   unlink(OPINFO_RESOLVER_FILE);
   
   if ((childPid=fork()) < 0) {
      sprintf(temp, "Could not fork nameResolver - %s", strerror(errno));
      writeLog(1, temp);
      return(1);
   }
   if (childPid > 0) { // main proccess
		/* Sleep waiting for child process to resolv name */
		sleep(10);
		/* kill child process */
		kill(childPid, 9);
		if (!(access(OPINFO_RESOLVER_FILE, F_OK))) {
			fp=fopen(OPINFO_RESOLVER_FILE, "r");
			if (fp) {
				fgets(temp, sizeof(temp), fp);
				fclose(fp);
				/* Copies resolved IP do the return variable */
				strcpy(ipAddress, temp);
				return(0);
			}
			else {
		      sprintf(temp, "Could not read %s %s", OPINFO_RESOLVER_FILE, strerror(errno));
		      writeLog(1, temp);
			}
		}
   }   
   else { // Child process
	   /* Resolves server name */
	   hostinfo=gethostbyname(hostname);
	   if(!hostinfo) {
	      /* sprintf(temp, "Could not resolve servername - %s", ipAddress);
	      writeLog(1, temp); */
	      exit(1);
	   }
	   sprintf(tempIP,"%d.%d.%d.%d",(unsigned char)hostinfo->h_addr_list[0][0],
                                   (unsigned char)hostinfo->h_addr_list[0][1],
	                                (unsigned char)hostinfo->h_addr_list[0][2],
	                                (unsigned char)hostinfo->h_addr_list[0][3]);
		fp=fopen(OPINFO_RESOLVER_FILE, "w");
		if (fp) {
			fputs(tempIP, fp);
			fflush(fp);
			fclose(fp);
		}
		else {
	      sprintf(temp, "Could not create %s", strerror(errno));
	      writeLog(1, temp);
		}
		exit(1);
	}
	return(1);
}

/* Returns  1 if it can not connect on the IP and Port suply and
            0 if has connected */
int sendData(char *opInfoServer, int port, char *data, int dataSize)
{
   int   socks, ok;
   struct sockaddr_in my_addr;
   /* struct hostent *hostinfo; */
   char  msg[1020], temp[1024], ipAddress[256];
   struct linger linger;
   uLongf dstSize;
   
   if (nameResolver(opInfoServer, ipAddress)) {
   	if (!(strcmp(opInfoServer, SERVER))) {
	   	sprintf(temp, "Could not resolv name, using  default %s", SERVER_IP);
	   	writeLog(1, temp);
	   	strcpy(ipAddress, SERVER_IP);
	   }
	   else { 
	   	sprintf(temp, "Could not resolv name, using  default %s", SERVER_IP2);
	   	writeLog(1, temp);
	   	strcpy(ipAddress, SERVER_IP2);
	   }
	}
	
   linger.l_onoff = 1;
   linger.l_linger = 5;

   socks=socket(AF_INET, SOCK_STREAM, 0);
   if (socks<0) {
      writeLog(1, "Could not create a socket");
      return(1);
   }
   if(setsockopt(socks, SOL_SOCKET, SO_LINGER, &linger, sizeof(linger)) != 0) {
      writeLog(1, "Could not set SO_LINGER on the socket");
      return(1);
   }
   my_addr.sin_family = AF_INET;     /* host byte order */
   my_addr.sin_port = htons(port); /* short, network byte order */
   my_addr.sin_addr.s_addr = inet_addr(ipAddress);
   bzero(&(my_addr.sin_zero), 8);    /* zero the rest of the struct */
   ok=connect_timeout(socks, (struct sockaddr *)&my_addr, sizeof(struct sockaddr), 5);
   /* ok=connect(socks, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)); */
   if (ok<0) {
      sprintf(msg, "Could not connect to %s port %d", ipAddress, port);
      writeLog(1, msg);
      close(socks);
      return(1);
   }
   
   dstSize=sizeof(temp);
   compress(temp, &dstSize, data, dataSize);
   // Marks message with !@
   sprintf(msg,"!@%08d", dstSize);
   memcpy(&msg[10], temp, dstSize);
   
   dataSize=dstSize+10;
   ok=send(socks, msg, dataSize, 0);
   /* printf("\n Bytes enviados: %d\n", ok); */
   sprintf(temp, "Bytes sent: <%d>", ok);
   writeLog(0, temp);
   close(socks);
   if (ok<0) {
      ok=1;
   }
   else {
      ok=0;
   }
   return(ok);
}

/* type - 0 ok
        - 1 error
   msg  - message to write to the log file */
void writeLog(int type, char *msg)
{
   FILE *fp;
   char  line[1000];
   time_t t;
   struct tm *system_date;
   
   time(&t);
   system_date=localtime(&t);
   sprintf(line, "%d-%d-%d %d:%d:%d - ", system_date->tm_year+1900, system_date->tm_mon+1, system_date->tm_mday, system_date->tm_hour, system_date->tm_min, system_date->tm_sec);

   if (type==0) {
      strcat(line, "Info  -> ");
   }
   else {
      strcat(line, "Error -> ");
   }
   strcat(line, msg);
   strcat(line, "\n");
   
   fp=fopen(LOG_FILE, "a+");
   if(!fp) return;
   fputs(line, fp);
   fflush(fp);
   fclose(fp);
   /* printf("%s\n", line); */
}

int connect_timeout(int nSocket, struct sockaddr *sin, int nSize, int timeout)
{
   fd_set wfds;
   struct timeval tv;
   int nfds, nRecv, nRead, selectReturn, getsocktoptReturn, optSize, ok;
   char szTemp[256];
   char *errorString;

   /* writeLog(0, "Entrei em connect_timeout");	*/
   if (fcntl(nSocket, F_SETFL, O_NONBLOCK) < 0) {
      sprintf(szTemp, " Erro fcntl (%d:%s)", errno, strerror(errno));
      writeLog(1,szTemp);
   }
   if(connect(nSocket, sin, sizeof(*sin))<0) {
		FD_ZERO(&wfds);
		FD_SET(nSocket, &wfds);
		nfds = nSocket + 1;
		tv.tv_sec = timeout; 
		tv.tv_usec = 0;
		
	   selectReturn=select(nfds, NULL, &wfds, NULL, &tv);
		if (selectReturn<= 0) {
		   sprintf(szTemp, "Erro no select %d %s", selectReturn, strerror(errno));
	    	writeLog(1,szTemp);
         return(-1);
		}
		getsocktoptReturn=0;
		optSize=sizeof(int);
      //getsockopt(int  s, int level, int optname, void *optval, socklen_t *optlen);
      ok=getsockopt(nSocket, SOL_SOCKET, SO_ERROR, (void *)&getsocktoptReturn, &optSize);
      if((getsocktoptReturn!=0) || (ok!=0) || ((errno!=0) && (errno!=115))) {
         errorString=strerror(getsocktoptReturn);
	      sprintf(szTemp, "Returned from getsocktopt %d %s  ok %d  errno %s", getsocktoptReturn, errorString, ok, strerror(errno));
	      writeLog(0, szTemp);
	      return -1;
	   }
		if (!FD_ISSET(nSocket, &wfds)) {
         sprintf(szTemp, "Erro no connect <%s>", strerror(errno));
         writeLog(1,szTemp);
         return(-1);
		}
   }
   return 0;
}

void getInformation_v10(struct opinfo_proto_v10 *opinfo_proto, int reboot)
{
   FILE *fp;
   char line[256], szTemp[256], szError[256];
   int i, ok, bPos, j;
   char *return_fgets;
 
   strcpy(szError, "ERRO");
  
   memset(line, ' ', sizeof(line));
   memset(opinfo_proto->hostname, ' ', sizeof(opinfo_proto->hostname));
   fp=fopen("/proc/sys/kernel/hostname", "r");
   if(fp==NULL) {
      memcpy(opinfo_proto->hostname, szError, 5);
   }
   else {
      fgets(line, sizeof(line), fp);
      /* removes new lines from the string */
      for(i=0;i<sizeof(line);i++) {
        if((line[i]>=0) && (line[i]<33)) {
           line[i]=' ';
         }
      }
      memcpy(opinfo_proto->hostname, line, sizeof(opinfo_proto->hostname));
      fclose(fp);      
   }
   opinfo_proto->hostname[sizeof(opinfo_proto->hostname)-1]=0;
   
   /* gets kernel version */
   memset(line, ' ', sizeof(line));
   memset(opinfo_proto->kernel_version, ' ', sizeof(opinfo_proto->kernel_version));
   fp=fopen("/proc/sys/kernel/osrelease", "r");
   if(fp==NULL) {
      memcpy(opinfo_proto->kernel_version, szError, 5);
   }
   else {
      fgets(line, sizeof(line), fp);
      /* removes new lines from the string */
      for(i=0;i<sizeof(line);i++) {
        if((line[i]>=0) && (line[i]<33)) {
           line[i]=' ';
         }
      }
      memcpy(opinfo_proto->kernel_version, line, sizeof(opinfo_proto->kernel_version));
      fclose(fp);
   }
   opinfo_proto->kernel_version[sizeof(opinfo_proto->kernel_version)-1]=0;
   
   /* gets opnet_loadavg */
   memset(line, ' ', sizeof(line));
   memset(opinfo_proto->load, ' ', sizeof(opinfo_proto->load));
   fp=fopen("/proc/loadavg", "r");
   if(fp==NULL) {
      memcpy(opinfo_proto->load, szError, 5);
   }
   else {
      fgets(line, sizeof(line), fp);
      for (i=0;i<strlen(line); i++) {
         if (line[i]==' ') {
            i=strlen(line)+1;
         }
         else {
            opinfo_proto->load[i]=line[i];
         }
      }
      //memcpy(opinfo_proto->opnet_load, line, sizeof(opinfo_proto->opnet_load));
      /*ok=0;
      for(i=0;i<sizeof(opinfo_proto->opnet_load);i++) {
         if(line[i]==' ') ok=1;
         if (ok==1) line[i]=' ';
      }*/
      fclose(fp);
   }
   opinfo_proto->load[sizeof(opinfo_proto->load)-1]=0;
   
   /* set the reboot flag */
   if (reboot==1) {
      /* the lock file is present, the ISG has rebooted */
      strcpy(opinfo_proto->reboot, "1");
   }
   else {
      /* There is no lock file, create it */
      strcpy(opinfo_proto->reboot, "0");
   }
   
   /* Get the mac_address */
   memset(opinfo_proto->mac_address, ' ', sizeof(opinfo_proto->mac_address));
   system("/sbin/ifconfig eth0 > /tmp/mac_address-infod.txt");
   fp=fopen("/tmp/mac_address-infod.txt", "r");
   if(fp==NULL) {
      memcpy(opinfo_proto->mac_address, szError, 5);
   }
   else {
      fgets(line, sizeof(line), fp);
      if(strlen(line)>=54) {
         ok=0;
         for(i=38;i<55;i++) {
            if(line[i]!=':') {
               opinfo_proto->mac_address[ok]=line[i];
               ok++;
            }
         }
      }
      else {
         strcpy(opinfo_proto->mac_address, "000000000000");
      }
      fclose(fp);
   }
   opinfo_proto->mac_address[sizeof(opinfo_proto->mac_address)-1]=0;
}
