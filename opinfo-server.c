/* 
   opinfo-server
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
#include <signal.h>
#include </usr/include/mysql/mysql.h>
#include <zlib.h>

#include "opinfo.h"

/* constants */
#define LOG_FILE "/var/log/opinfo-server.log"

/* Functions prototypes */
int setServer(int port, int qlen);
int tcpServer(int socket, struct sockaddr_in sin);
void writeLog(int type, char *msg);
int dbWrite_mysql(struct opinfo_proto_v10 *opinfo_proto, char *ipFrom);
int verifySocketStatus(int socket);
void sendEmail(char *hostname, char *ip, int msgType);

int main()
{
   pid_t processPid, sPid;
   int   ok,i,sock, socket, slen;
   char  temp[1024];
   struct sockaddr_in sin;
   
   sprintf(temp, "Inicializing OpInfo-server demon V.%s", VERSION);
   printf("\n-> %s\n", temp);
   writeLog(0, temp);
   
   /* Forks to background*/
   /* Detachs process from terminal */
   setpgrp();
   if ((processPid=fork()) < 0) {
      sprintf(temp, "Could not fork - %s", strerror(errno));
      writeLog(1, temp);
      exit(1);
   }
   if (processPid > 0) {
      exit(0);
   }
   sock=setServer(PORT, 10);
   while(1) {
      slen=sizeof(sin);
      socket=accept(sock, (struct sockaddr *)&sin, &slen);
      sPid=fork();
      switch(sPid) {
         case 0: {/* child */
            close(sock);
            if (setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, "\x1", sizeof(int)) != 0) {
               writeLog(1, "Could not set SO_REUSEADDR on the socket");
               close(socket);
               exit(1);
            }
            ok=tcpServer(socket, sin);
            close(socket);
            exit(ok);
          }
         
         default: {/* parent */
            signal(SIGCLD, SIG_IGN);
            close(socket);
            break;
         }
         case -1: {/* could not fork */
            writeLog(1, "Could not fork");
         }
      }   
   }
   return(0);
}

/* Allocate and bind a server socket using TCP */
int setServer(int port, int qlen)
{
   struct sockaddr_in sin;
   int sock,ok;
   
   bzero((char *)&sin, sizeof(sin));
   sin.sin_family = AF_INET;     /* host byte order */
   sin.sin_port = htons(port); /* short, network byte order */
   sin.sin_addr.s_addr = INADDR_ANY;

   sock=socket(PF_INET, SOCK_STREAM, 0);
   if (sock<0) {
      writeLog(1, "Could not create a socket");
      exit(1);
   }
   if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, "\x1", sizeof(int)) != 0) {
      writeLog(1, "Could not set SO_REUSEADDR on the socket");
      exit(1);
   }
   ok=bind(sock, (struct sockaddr *)&sin, sizeof(sin));
   if (ok<0) {
      writeLog(1, "Could not bind the socket");
      exit(1);
   }
   ok=listen(sock, qlen);
   if (ok!=0) {
      writeLog(1, "Could not listen the socket");
      exit(1);
   }
   return(sock);
}

int tcpServer(int socket, struct sockaddr_in sin)
{
   fd_set rfds;
   struct timeval tv;
   int nfds, nRecv, nRead, ok,
       nBytesProt, posString, selectReturn,
       getsocktoptReturn, optSize,
       returnCode, nTurns, maxTurns,
       nZippedBytes;
   uLongf   dstSize;
   char szTemp[512], buffer[512], tempBuffer[256],
      *errorString, ipFrom[30], temp[1024];
   struct opinfo_proto_v10 opinfo_proto;
   
   // sets IpFrom.
   strcpy(ipFrom, (char *)inet_ntoa(sin.sin_addr));

   // Gets protocol header - number of bytes comming 
   nBytesProt=10;
   maxTurns=30;
   nTurns=0;
   posString=0;
   while (nBytesProt>0) {
      /* Verifies if everything is still ok with the socket */
      if(verifySocketStatus(socket)) {
	      return(1);
	   }
      FD_ZERO(&rfds);
      FD_SET(socket, &rfds);
      nfds = socket + 1;
      tv.tv_sec = 60;
      tv.tv_usec = 0;
      selectReturn=select(nfds, &rfds, NULL, NULL, &tv);
      switch(selectReturn) {
         case 0: /* timeout*/
            sprintf(szTemp, "Timeout no select from %-15.15s", inet_ntoa(sin.sin_addr));
            writeLog(1, szTemp);
            return(1);
         case -1: /* error on select */
            sprintf(szTemp, "Select error %-15.15s", inet_ntoa(sin.sin_addr));
            writeLog(1, szTemp);
            return(1);
         default: /* select ok - reads data from socket */
            nRecv=recv(socket, (void *)tempBuffer, nBytesProt, 0);
            nBytesProt-=nRecv;
            /* strncpy(&buffer[posString], tempBuffer, nRecv); */
            memcpy(&buffer[posString], tempBuffer, nRecv);
            posString+=nRecv;
      }
      nTurns++;
      if (nTurns>maxTurns) {
         sprintf(szTemp, "Error receiving - Max number of Turns %-15.15s", inet_ntoa(sin.sin_addr));
         writeLog(1, szTemp);
         return(1);
      }         
   }
   buffer[10]=0;
   // Verifies if protocol complies
   if ( (buffer[0]!='!') || (buffer[1]!='@') ) {
      sprintf(temp, "Reject packet from %s", ipFrom);
      writeLog(1, temp);
      return(1);
   }
   strncpy(temp, &buffer[2], 8);
   temp[8]=0;
   nZippedBytes=atoi(temp);
   nBytesProt=atoi(temp);
   if (nBytesProt>512) {
      sprintf(temp, "Reject packet from %s - too big <%ld>", ipFrom, nBytesProt);
      writeLog(1, temp);
      return(1);
   }
   maxTurns=nBytesProt+20;
   nTurns=0;
   posString=0;
   while (nBytesProt>0) {
      /* Verifies if everything is still ok with the socket */
      /* if(verifySocketStatus(socket)) {
	      return(1);
	   }*/
      FD_ZERO(&rfds);
      FD_SET(socket, &rfds);
      nfds = socket + 1;
      tv.tv_sec = 60;
      tv.tv_usec = 0;
      selectReturn=select(nfds, &rfds, NULL, NULL, &tv);
      switch(selectReturn) {
         case 0: /* timeout*/
            sprintf(szTemp, "Timeout no select from %-15.15s", inet_ntoa(sin.sin_addr));
            writeLog(1, szTemp);
            return(1);
         case -1: /* error on select */
            sprintf(szTemp, "Select error %-15.15s", inet_ntoa(sin.sin_addr));
            writeLog(1, szTemp);
            return(1);
         default: /* select ok - reads data from socket */
            nRecv=recv(socket, (void *)tempBuffer, nBytesProt, 0);
            nBytesProt-=nRecv;
            /* strncpy(&buffer[posString], tempBuffer, nRecv); */
            memcpy(&buffer[posString], tempBuffer, nRecv);
            posString+=nRecv;
      }
      nTurns++;
      if (nTurns>maxTurns) {
         sprintf(szTemp, "Error receiving - Max number of Turns %-15.15s", inet_ntoa(sin.sin_addr));
         writeLog(1, szTemp);
         return(1);
      }         
   }
   
   dstSize=sizeof(temp);
   uncompress (temp, &dstSize, buffer, nZippedBytes);
   memcpy((void *)&opinfo_proto, temp, sizeof(opinfo_proto));
   dbWrite_mysql(&opinfo_proto, ipFrom);
   for(ok=5;ok<sizeof(opinfo_proto.hostname);ok++) {
      if(opinfo_proto.hostname[ok]==' ') {
         opinfo_proto.hostname[ok]=0;
         break;
      }
   }
   sprintf(szTemp, "Packet received <%s> <%s>", opinfo_proto.hostname, ipFrom);
   writeLog(0, szTemp);
   return(0);
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
   /* printf("%s", line);*/
}

int dbWrite_mysql(struct opinfo_proto_v10 *opinfo_proto, char *ipFrom)
{
   MYSQL *dbHandle;
   MYSQL_RES *mysqlResult;
   MYSQL_ROW row;
   char  dbString[256], sql[1024],
         dateTime[128], szTime[512],
         temp[2048], hostname[150],
         mac_address[20], reboot[20],
         hw_change[20], ipFromAux[20],
         gen_dns[5];
   int i, j, nrBoots, genDNS,nRows;
   time_t t;
   struct tm *system_date;
   
   time(&t);
   system_date=localtime(&t);
   sprintf(dateTime, "%d-%2.2d-%2.2d %2.2d:%2.2d:%2.2d", system_date->tm_year+1900, system_date->tm_mon+1, system_date->tm_mday, system_date->tm_hour, system_date->tm_min, system_date->tm_sec);
  
   /* Takes out all spaces at the end of the word */
   for(i=0;i<strlen(opinfo_proto->hostname);i++) {
      if(opinfo_proto->hostname[i]==' ') {
         opinfo_proto->hostname[i]=0;
         break;
      }
   }
   /* Check for error on the hostname sent by the ISG */
   /*if(strcmp(opinfo_proto->hostname, "ERRO")==0) {
      //system(sql);
      return(1);
   }*/
   for(i=0;i<strlen(ipFrom);i++) {
      if(ipFrom[i]==' ') {
         ipFrom[i]=0;
         break;
      }
   }
   for(i=0;i<strlen(opinfo_proto->kernel_version);i++) {
      if(opinfo_proto->kernel_version[i]==' ') {
         opinfo_proto->kernel_version[i]=0;
         break;
      }
   }
   for(i=0;i<strlen(opinfo_proto->load );i++) {
      if(opinfo_proto->load[i]==' ') {
         opinfo_proto->load[i]=0;
         break;
      }
   }
   for(i=0;i<strlen(opinfo_proto->mac_address);i++) {
      if(opinfo_proto->mac_address[i]==' ') {
         opinfo_proto->mac_address[i]=0;
         break;
      }
   }
   // Inicializes MySQL
   dbHandle=mysql_init(NULL);
   if (!dbHandle) {
      writeLog(1,"Could not allocated memory for mysql_init");
      return(1);      
   }
   // Connects to MySQL
   mysql_real_connect(dbHandle, DBSERVER, DBUSER, DBPASS, DBNAME,0,NULL,0);
   if(mysql_errno(dbHandle)) {
      sprintf(temp, "MySql Connect - %ld %s", mysql_errno, mysql_error(dbHandle));
      writeLog(1,temp);
      mysql_close(dbHandle);
      return(1);
   }
   if (mysql_ping(dbHandle)) {
      writeLog(1,"Connection to DB lost");
      mysql_close(dbHandle);
      return(1);
   }
   // Select the table to see if hostname already registered.
   sprintf(sql, "select hostname, mac_address, reboot, hw_change, ip, gen_dns from ophosts where hostname='%s'",opinfo_proto->hostname);
   if (mysql_query(dbHandle, sql)) {
      if (mysql_errno(dbHandle)==1065) {
         // no error - query empty
      }
      else { 
         sprintf(temp, "MySql Select %ld %s", mysql_errno(dbHandle), mysql_error(dbHandle));
         writeLog(1,temp);
         mysql_close(dbHandle);
         return(1);
      }
   }
   mysqlResult = mysql_store_result(dbHandle);
   nRows=mysql_affected_rows(dbHandle);
   if (nRows<=0) { // Insert into de database
      // insert
      mysql_free_result(mysqlResult);
      sprintf(sql, "insert into ophosts (hostname, mac_address, kernel_version, load, ip, gen_dns,date_registered,date_last_update) values ('%s', '%s', '%s', '%s','%s',%d,now(),now())",opinfo_proto->hostname, opinfo_proto->mac_address, opinfo_proto->kernel_version, opinfo_proto->load, ipFrom, 1);
      if (mysql_query(dbHandle, sql)) {
         sprintf(temp, "MySql Insert %ld %s\n", mysql_errno(dbHandle), mysql_error(dbHandle));
         writeLog(1,temp);
         mysql_close(dbHandle);
         return(1);
      }
      sprintf(temp, "New Server On-line %s %s", opinfo_proto->hostname, ipFrom);
      writeLog(0, temp);
      sendEmail(opinfo_proto->hostname, ipFrom, 2);
   }
   else { // checks for reboot, hardware change and update de DB.
      row=mysql_fetch_row(mysqlResult);
      strcpy(hostname, row[0]);
      strcpy(mac_address, row[1]);
      strcpy(reboot, row[2]);
      strcpy(hw_change, row[3]);
      strcpy(ipFromAux, row[4]);
      strcpy(gen_dns, row[5]);
      genDNS=atoi(gen_dns);
      mysql_free_result(mysqlResult);

      // Verifies if Server has rebooted
      if (strcmp(opinfo_proto->reboot, "1")==0) {
         nrBoots=atoi(reboot)+1;
         sprintf(reboot, "%d", nrBoots);
         // Logs status 3 if machine has rebooted
         sprintf(sql, "insert into history (hostname, status, date) values ('%s', 3, now())", hostname);
         if (mysql_query(dbHandle, sql)) {
            if (mysql_errno(dbHandle)) {
               sprintf(temp, "MySql Log Reboot History - %ld %s\n", mysql_errno(dbHandle), mysql_error(dbHandle));
               writeLog(1,temp);
            }
         }
      }
      // Verifies if Server hardware has been changed 
      if (strcmp(opinfo_proto->mac_address, mac_address)!=0) {
         nrBoots=atoi(hw_change)+1;
         sprintf(hw_change, "%d", nrBoots);
         sprintf(sql, "insert into history (hostname, status, date) values ('%s', 4, now())", hostname);
         if (mysql_query(dbHandle, sql)) {
            if (mysql_errno(dbHandle)) {
               sprintf(temp, "MySql Log HWCHANGE History - %ld %s\n", mysql_errno(dbHandle), mysql_error(dbHandle));
               writeLog(1,temp);
            }
         }         
         // Sends email
         sendEmail(hostname, ipFrom, 1);
      }
      // Verifies if ipAddress has changed
      if (strcmp(ipFromAux, ipFrom)!=0) {
         genDNS=1;
         sprintf(sql, "insert into history (hostname, status, date) values ('%s', 2, now())", hostname);
         if (mysql_query(dbHandle, sql)) {
            if (mysql_errno(dbHandle)) {
               sprintf(temp, "MySql Log IP History - %ld %s\n", mysql_errno(dbHandle), mysql_error(dbHandle));
               writeLog(1,temp);
            }
         }
      }
      sprintf(sql, "update ophosts set mac_address='%s', kernel_version='%s', reboot=%s, hw_change=%s, ip='%s', gen_dns=%d, date_last_update=now(), dyn_dns=1, load='%s' where hostname='%s'",opinfo_proto->mac_address, opinfo_proto->kernel_version, reboot, hw_change, ipFrom, genDNS, opinfo_proto->load, hostname);
      if (mysql_query(dbHandle, sql)) {
         if (mysql_errno(dbHandle)) {
            sprintf(temp, "MySql UPDATE - %ld %s\n", mysql_errno(dbHandle), mysql_error(dbHandle));
            writeLog(1,temp);
         }
      }
   }
   mysql_close(dbHandle);
   return(0);
}

/*
   Test socket and returns
      1 - Error
      0 - Ok
 */
int verifySocketStatus(int socket)
{
	return(0);
   int ok, getsocktoptReturn, optSize;
   char szTemp[512], *errorString;
      
   /* Verifies if everuthing is still ok with the socket */
   ok=getsockopt(socket, SOL_SOCKET, SO_ERROR, (void *)&getsocktoptReturn, &optSize);
   if((getsocktoptReturn!=0) || (ok!=0) || ((errno!=0) && (errno!=115)) || ((errno!=0) && (errno!=115))) {
       errorString=strerror(getsocktoptReturn);
	    sprintf(szTemp, "Socket <%ld> Error %d %s  ok %d  errno %s", socket, getsocktoptReturn, errorString, ok, strerror(errno));
	    writeLog(1, szTemp);
	    return(1);
   }
   return(0);
}

/*
   Sends email 
   1 - hardware change
   2- New host included
*/
void sendEmail(char *hostname, char *ip, int msgType)
{
   char  date_time[100],
         temp[1000],
         body[1000];
   time_t t;
   struct tm *system_date;
 
   time(&t);
   system_date=localtime(&t);
   sprintf(date_time, "%d-%02d-%02d %02d:%02d:%02d ", system_date->tm_year+1900, system_date->tm_mon+1, system_date->tm_mday, system_date->tm_hour, system_date->tm_min, system_date->tm_sec);

   if (msgType==1) {
      sprintf(body,"\"HW_CHANGE %s | %s | %s \"", hostname, ip, date_time);
   }
   else if (msgType==2) {
      sprintf(body,"\"New Server On-line %s | %s | %s \"", hostname, ip, date_time);
   }
   sprintf(temp, "/bin/echo %s | %s email@email.com email@email.com %s", body, MAIL_SCRIPT, body);
   system(temp);
}
