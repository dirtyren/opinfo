/* 
   opinfo-monitor
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
#include </usr/include/mysql/mysql.h>

#include "opinfo.h"

/* constants */
#define LOGFILE "/var/log/opinfo-monitor.log"
#define PING_SCRIPT "/usr/local/opinfo/ping.pl"
#define MAIL_SCRIPT "/usr/local/opinfo/sendMail.pl"
#define POOL_TIME 30

/* Functions prototypes */
void writeLog(int type, char *msg);
int ping(char *ip);
int pingTCP(char *ip, int port);
//int updateHistory(char *hostname, int laststatus, int status);
int updateStatus(char *hostname, int laststatus);
int connected(char *ipAdress, int port);
int connect_timeout(int nSocket, struct sockaddr *sin, int nSize, int timeout);
void sendEmail(char *hostname, int status, char *descr);

int main()
{
   pid_t processPid;
   int   ok, i, nrTuplas, nrFields,
         laststatus, hosts_up, hosts_down,
         nRows, nOpNet;
   long  nMinutes;
   char  temp[1024],
   		sql[1024],
         hostname[201];
   //time_t t, nMinutesLastUpDate;
   //struct tm *system_date, *db_date;
   MYSQL *dbHandle;
   MYSQL_RES *mysqlResult;
   MYSQL_ROW row;
   
   sprintf(temp, "Inicializing opinfo-monitor demon V.%s", VERSION);
   printf("\n->%s\n", temp);
   writeLog(0, temp);
   sprintf(temp, "Pooling time set to <%d>", POOL_TIME);
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
   
   while (1) {
      /* Connects to the database */
	   // Inicializes MySQL
	   dbHandle=mysql_init(NULL);
	   if (!dbHandle) {
	      writeLog(1,"Could not allocated memory for mysql_init - retrying in 10 seconds");
	      sleep(10);
	      continue;
	   }
	   // Connects to MySQL
	   mysql_real_connect(dbHandle, DBSERVER, DBUSER, DBPASS, DBNAME,0,NULL,0);
	   if(mysql_errno(dbHandle)) {
	      sprintf(temp, "MySql Connect - retrying in 10 seconds - %ld %s", mysql_errno, mysql_error(dbHandle));
	      writeLog(1,temp);
	      mysql_close(dbHandle);
	      sleep(10);
	      continue;
	   }
	   if (mysql_ping(dbHandle)) {
	      writeLog(1,"Connection to DB lost - retrying in 10 seconds");
	      mysql_close(dbHandle);
	      sleep(10);
	      continue;
	   }
	   sprintf(sql, "select hostname, laststatus, UNIX_TIMESTAMP(now())-UNIX_TIMESTAMP(date_last_update),opnet from ophosts order by hostname");
	   if (mysql_query(dbHandle, sql)) {
	      if (mysql_errno(dbHandle)==1065) {
	         // no error - query empty
	      }
	      else { 
	         sprintf(temp, "MySql Select - retrying in 10 seconds %ld %s", mysql_errno(dbHandle), mysql_error(dbHandle));
	         writeLog(1,temp);
	         mysql_close(dbHandle);
	         sleep(10);
	      }
	   }
	   mysqlResult = mysql_store_result(dbHandle);
      nrTuplas=mysql_affected_rows(dbHandle);
      sprintf(temp, "Start pooling - <%d> hosts", nrTuplas);
      writeLog(0, temp);
      hosts_up=0;
      hosts_down=0;
      for(i=0;i<nrTuplas;i++) {
      	row=mysql_fetch_row(mysqlResult);
      	strcpy(hostname, row[0]);      	
         laststatus=atoi(row[1]);
         nMinutes=atoi(row[2]);
         nOpNet=atoi(row[3]);
         
         // No OpInfo/ behind firewall
         if (nOpNet==3) {
				ok=1;
	      }
	      else {
	         nMinutes=nMinutes/60;
	         if (nMinutes>6) {
	            ok=0;
	            sprintf(temp,"%s DOWN - No info for %d", hostname, nMinutes);
	            writeLog(0, temp);
	         }
	         else {
	            ok=1;
	         }
	      }
         //updateHistory(hostname, laststatus, ok);
         if (ok) { // UP
            hosts_up++;
            if (laststatus==0) {
            	updateStatus(hostname, ok);
               // sendmail
               // sendEmail(hostname, ok, descr);
            }
         }
         else { // DOWN
            hosts_down++;
            //sprintf(temp, "Host <%s> is down.", hostname);
            //writeLog(1,temp);
            if (laststatus==1) {
            	updateStatus(hostname, ok);
               // sendmail
               // sendEmail(hostname, ok, descr);
            }

         }
      }
      mysql_free_result(mysqlResult);
		mysql_close(dbHandle);      
      sprintf(temp, "Pooling finished - <%d> UP / <%d> DOWN - Sleeping <%d> seconds", hosts_up, hosts_down, POOL_TIME);
      writeLog(0, temp);
      sleep(POOL_TIME);
   }
   return(0);
}

int updateStatus(char *hostname, int laststatus)
{
   char temp[1024],sql[1024];
   MYSQL *dbHandle;
   
   /* Connects to the database */
   /* Connects to the database */
   // Inicializes MySQL
   dbHandle=mysql_init(NULL);
   if (!dbHandle) {
      writeLog(1,"UPDATE STATUS - Could not allocated memory for mysql_init");
      sleep(10);
      return(1);
   }
   // Connects to MySQL
   mysql_real_connect(dbHandle, DBSERVER, DBUSER, DBPASS, DBNAME,0,NULL,0);
   if(mysql_errno(dbHandle)) {
      sprintf(temp, "UPDATE STATUS  - MySql Connect - %ld %s", mysql_errno, mysql_error(dbHandle));
      writeLog(1,temp);
      mysql_close(dbHandle);
      return(1);
   }
   if (mysql_ping(dbHandle)) {
      writeLog(1,"UPDATE STATUS  - Connection to DB lost");
      mysql_close(dbHandle);
      return(1);
   }
   sprintf(sql, "update ophosts set laststatus=%d where hostname='%s'",laststatus, hostname);
   if (mysql_query(dbHandle, sql)) {
		sprintf(temp, "UPDATE STATUS - %ld %s", mysql_errno(dbHandle), mysql_error(dbHandle));
      writeLog(1,temp);
		mysql_close(dbHandle);         
   	return(1);      
   }
	mysql_close(dbHandle);         
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
   sprintf(line, "%d-%02d-%02d %02d:%02d:%02d - ", system_date->tm_year+1900, system_date->tm_mon+1, system_date->tm_mday, system_date->tm_hour, system_date->tm_min, system_date->tm_sec);

   if (type==0) {
      strcat(line, "Info  -> ");
   }
   else {
      strcat(line, "Error -> ");
   }
   strcat(line, msg);
   strcat(line, "\n");
   
   fp=fopen(LOGFILE, "a+");
   if(!fp) return;
   fputs(line, fp);
   fflush(fp);
   fclose(fp);
   //printf("%s\n", line);
}

// 0=Down >0=UP
int ping(char *ip)
{
   int  ok;
   char temp[1024];
   // Pings the host
   sprintf(temp,"%s %s", PING_SCRIPT, ip);
   ok=system(temp);
   return(ok);
}


int pingTCP(char *ip, int port)
{
   int ok;
   ok=connected(ip, port);
   return(ok);   
}

/*int updateHistory(char *hostname, int laststatus, int status)
{
   int  ok;
   char temp[1024];
   PGconn *dbHandle;
   PGresult *sqlResult;
   ConnStatusType conStatus;*/
   
   /* Connects to the database */
   /*sprintf(temp, "host=%s port=%s dbname=%s user=%s password=%s", DBHOST, DBPORT, DBNAME, DBUSER, DNPASS);
   dbHandle=PQconnectdb(temp);
   if (!dbHandle) {
      writeLog(1,"could not connect to the database");
      return(0);
   }
   conStatus=PQstatus(dbHandle);
   if (conStatus!=CONNECTION_OK) {
      writeLog(1,"Conenction status unstable");
      return(0);
   }
   
   ok=status;
   // Based on laststatus and the ping result, takes action
   if (ok) { // UP
      if (laststatus==0) {
         // write history
         sprintf(temp, "insert into history values ('%s', %d);", hostname, 1);
         sqlResult=PQexec(dbHandle, temp);
         if (!sqlResult) {
            writeLog(1,"Could not insert into history");
         }         
      }
   }
   else { // DOWN
      if (laststatus==1) {
         // write history
         sprintf(temp, "insert into history values ('%s', %d);", hostname, 0);
         sqlResult=PQexec(dbHandle, temp);
         if (!sqlResult) {
            writeLog(1,"Could not insert into history");
         }                  
      }   
   }
   PQfinish(dbHandle);
   return(ok);
} */

// Sends email when status change
void sendEmail(char *hostname, int status, char *descr)
{
   char  statusDescr[100],
         date_time[100],
         temp[1000],
         body[1000];
   time_t t;
   struct tm *system_date;
   
   time(&t);
   system_date=localtime(&t);
   sprintf(date_time, "%d-%02d-%02d %02d:%02d:%02d ", system_date->tm_year+1900, system_date->tm_mon+1, system_date->tm_mday, system_date->tm_hour, system_date->tm_min, system_date->tm_sec);

   if (status) {
      strcpy(statusDescr, "UP");
   }
   else {
      strcpy(statusDescr, "DOWN");
   }
   sprintf(body,"\"%s | %s | %s | %s\"", hostname, statusDescr, date_time, descr);
   //sprintf(temp, "/bin/echo \"%s - %s - %s - %s\" | %s callcenter@opservices.com.br alerts@opservices.com.br \"Alerta da Monitoração\"", hostname, statusDescr, date_time, descr, MAIL_SCRIPT);
   sprintf(temp, "/bin/echo %s | %s callcenter@opservices.com.br opnet@opservices.com.br %s", body, MAIL_SCRIPT, body);
   //writeLog(0,temp);
   system(temp);
}


/* Returns  0 if it can not connect on the IP and Port suply and
            1 if has connected */
int connected(char *ipAddress, int port)
{
   int   socks, ok;
   struct sockaddr_in my_addr;
   char  msg[1000];
   
   socks=socket(AF_INET, SOCK_STREAM, 0);
   if (socks<0) {
      writeLog(1, "Could not create a socket");
      return(0);
   }
   my_addr.sin_family = AF_INET;     /* host byte order */
   my_addr.sin_port = htons(port); /* short, network byte order */
   my_addr.sin_addr.s_addr = inet_addr(ipAddress);
   bzero(&(my_addr.sin_zero), 8);    /* zero the rest of the struct */
   ok=connect_timeout(socks, (struct sockaddr *)&my_addr, sizeof(struct sockaddr), 10);
   /* ok=connect(socks, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)); */
   if (ok<0) {
      /*sprintf(msg, "Could not connect to %s port %d", ipAddress, port);
      writeLog(1, msg); */
      close(socks);
      return(0);
   }
   close(socks);
   return(1);
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
	   /*sprintf(szTemp, "Returned from select %d", selectReturn);
	   writeLog(0, szTemp);*/
		if (selectReturn<= 0) {
	    	/* writeLog(1," Erro no select"); */
         return(-1);
		}
		getsocktoptReturn=0;
		optSize=sizeof(int);
      //getsockopt(int  s, int level, int optname, void *optval, socklen_t *optlen);
      ok=getsockopt(nSocket, SOL_SOCKET, SO_ERROR, (void *)&getsocktoptReturn, &optSize);
      if((getsocktoptReturn!=0) || (ok!=0) || ((errno!=0) && (errno!=115))) {
         /* errorString=strerror(getsocktoptReturn);
	      sprintf(szTemp, "Returned from getsocktopt %d %s  ok %d  errno %s", getsocktoptReturn, errorString, ok, strerror(errno));
	      writeLog(0, szTemp); */
	      return(-1);
	   }
		if (!FD_ISSET(nSocket, &wfds)) {
         /* writeLog(1,"Erro no connect"); */
         return(-1);
		}
   }
   return(0);
}
