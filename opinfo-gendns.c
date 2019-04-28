/* 
   opinfo-gendns
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
// mysql
#include </usr/include/mysql/mysql.h>

#include "opinfo.h"

/* constants */
#define DNS_ZONE_FILE  "db.subdomain.domain.com"
#define KILLHUP_NAMED "/usr/bin/killall -HUP named"
#define LOG_FILE  "/var/log/opinfo-gendns.log"
#define GEN_TIME 30
#define SERIAL_FILE "/usr/local/opinfo/serial-file.txt"

/* Functions prototypes */
void writeLog(int type, char *msg);

int main()
{
   MYSQL *dbHandle;
   MYSQL_RES *mysqlResult;
   MYSQL_ROW row;
   pid_t processPid, sPid;
   char  dbString[256], sql[1024],
         temp[1024], soa[64],soa2[64], file[256];
   int i, j,
       nrTuples, changedIps,
       seqNumber, nDay, dbSelected;
   FILE *fp,*fSerial;
   time_t t;
   struct tm *system_date;
   
   sprintf(temp, "Inicializing OpInfo-gendns demon V.%s", VERSION);
   printf("\n-> %s\n", temp);
   writeLog(0, temp);
   
   time(&t);
   system_date=localtime(&t);
   sprintf(soa, "%d%2.2d%2.2d%2.2d", system_date->tm_year+1900, system_date->tm_mon+1, system_date->tm_mday, system_date->tm_sec);
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
   nDay=-1;
   seqNumber=0;
   while(1) {
      // Inicializes MySQL
      dbHandle=mysql_init(NULL);
      if (!dbHandle) {
         writeLog(1,"Could not allocated memory for mysql_init - retrying in 10 seconds");
         sleep(10);
         continue;
      }
      // Connects to MySQL
      dbSelected=1;
      mysql_real_connect(dbHandle, DBSERVER, DBUSER, DBPASS, DBNAME,0,NULL,0);
      if(mysql_errno(dbHandle)) {
         sprintf(temp, "MySql Connect to <%s> - %ld %s", DBSERVER, mysql_errno, mysql_error(dbHandle));
         writeLog(1,temp);
         continue;
         // Connects to MySQL Server 2
         //dbSelected=2;
      	/* mysql_real_connect(dbHandle, DBSERVER2, DBUSER, DBPASS, DBNAME,0,NULL,0);
      	if(mysql_errno(dbHandle)) {
         	sprintf(temp, "MySql Connect <%s> failed retrying in 10 seconds - %ld %s", DBSERVER2, mysql_errno, mysql_error(dbHandle));
         	writeLog(1,temp);
         	mysql_close(dbHandle);
         	sleep(10);
         	continue;
         }*/
      }
      if (mysql_ping(dbHandle)) {
         writeLog(1,"Connection to DB lost - retrying in 10 seconds");
         mysql_close(dbHandle);
         sleep(10);
         continue;
      }      
      // select
      strcpy(sql, "select hostname, ip from ophosts where gen_dns=1");
      if (mysql_query(dbHandle, sql)) {
         if (mysql_errno(dbHandle)==1065) {
            // no error - query empty
         }
         else { 
            sprintf(temp, "MySql Select retrying in 10 seconds %ld %s", mysql_errno(dbHandle), mysql_error(dbHandle));
            writeLog(1,temp);
            mysql_close(dbHandle);
            sleep(10);
            continue;
         }
      }
      // checks if any IP has changed
      mysqlResult=mysql_store_result(dbHandle);
      nrTuples=mysql_affected_rows(dbHandle);
      mysql_free_result(mysqlResult);
      changedIps=nrTuples;
      if(nrTuples>0) {
         time(&t);
         system_date=localtime(&t);
         if (nDay!=system_date->tm_mday) {
            nDay=system_date->tm_mday;
            seqNumber=0;
         }
         seqNumber++;
         if (seqNumber>99) seqNumber=1;
         sprintf(soa, "%04d%02d%02d%02d", system_date->tm_year+1900, system_date->tm_mon+1, system_date->tm_mday, seqNumber);
         if (access(SERIAL_FILE,F_OK)==0)  {
            // files exists
            fSerial=fopen(SERIAL_FILE,"r");
            fgets(soa2, sizeof(soa2), fSerial);
            fclose(fSerial);
            if ((atol(soa2)+1)>atol(soa)) {
               // file bigger the memory, uses file fo increment
               sprintf(soa2,"%010ld", atol(soa2)+1);
               fSerial=fopen(SERIAL_FILE,"w");
               fputs(soa2, fSerial);
               fflush(fSerial);
               fclose(fSerial);
               strcpy(soa, soa2);
            }
            else {
               fSerial=fopen(SERIAL_FILE,"w");
               fputs(soa, fSerial);
               fflush(fSerial);
               fclose(fSerial);
            }
         }
         else {
            // file do not exists
            fSerial=fopen(SERIAL_FILE,"w");
            fputs(soa, fSerial);
            fflush(fSerial);
            fclose(fSerial);
         }
         // select all that have dyn_dns set to 1
         strcpy(sql, "select hostname, ip from ophosts where dyn_dns=1 order by hostname");
         if (mysql_query(dbHandle, sql)) {
            sprintf(temp, "MySql Select retrying in 10 seconds %ld %s", mysql_errno(dbHandle), mysql_error(dbHandle));
            writeLog(1,temp);
            mysql_close(dbHandle);
            sleep(10);
            continue;
         }
         mysqlResult=mysql_store_result(dbHandle);
         // Clean DNS temp file
         sprintf(temp, "/bin/rm -f /tmp/%s", DNS_ZONE_FILE);
         system(temp);
         sprintf(file, "/tmp/%s", DNS_ZONE_FILE);
         fp=fopen(file, "w+");
         if(!fp) {
            writeLog(1, "Could not create DNS zone file - retrying in 10 seconds");
            mysql_free_result(mysqlResult);
            mysql_close(dbHandle);
            sleep(10);
            continue;
         }
         fputs("@\tIN SOA  subdmoin.domain.com. hostmaster.domain.com. (\n", fp);
         sprintf(temp, "\t\t%s ; Serial\n", soa);
         fputs(temp, fp);
         fputs("\t\t28800\t; Refresh\n", fp);
         fputs("\t\t14400\t; Retry\n", fp);
         fputs("\t\t3600000\t; Expire\n", fp);
         fputs("\t\t300)\t; Minimum\n", fp);
         fputs(";\n", fp);
         fputs("\tIN\tNS\tns1.domain.com.\n", fp);
         fputs("\tIN\tNS\tns2.domain.com.\n", fp);
         fputs("\tIN\tNS\tns3.domain.com.\n", fp);
         fputs("\tIN\tNS\tns4.domain.com.\n", fp);
         fputs(";\n", fp);
         fflush(fp);
         // get each host and generate its corresponding entry in the DNS file
         while((row = mysql_fetch_row(mysqlResult))) {
            sprintf(temp, "%s\t\tIN\tA\t%s\n", row[0], row[1]);
            fputs(temp, fp);
         }
         mysql_free_result(mysqlResult);         
         sprintf(sql,"update ophosts set gen_dns=0");
         if (mysql_query(dbHandle, sql)) {
            sprintf(temp, "MySql UPDATE retrying in 10 seconds %ld %s", mysql_errno(dbHandle), mysql_error(dbHandle));
            writeLog(1,temp);
            mysql_close(dbHandle);
            sleep(10);
            continue;
         }
         fflush(fp);
         fclose(fp);

         // copy DNS temp file over.
         sprintf(temp, "/bin/cp -f /tmp/%s /var/named/chroot/var/named/%s", DNS_ZONE_FILE, DNS_ZONE_FILE);
         system(temp);
         system(KILLHUP_NAMED);
         system(KILLHUP_NAMED);
      }
      if (dbSelected==1) {
      	sprintf(temp, "DNS zone generated - MainDB - %d IPs changed", changedIps);
      	writeLog(0, temp);
      }
      else if (dbSelected==2) {
			sprintf(temp, "DNS zone generated - BackupDB - %d IPs changed", changedIps);
      	writeLog(0, temp);
      }
      mysql_close(dbHandle);
      sleep(GEN_TIME);
   }
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
      strcat(line, "Info -> ");
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
   //printf("%s\n", line);
}
