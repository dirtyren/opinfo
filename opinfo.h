
#define VERSION "1.8"
#define ERROR_STRING "ERROR"
#define SERVER "opinfo-server"
#define SERVER_IP "1.1.1.1"
#define SERVER2 "opinfo-server2"
#define SERVER_IP2 "2.2.2.2"
#define PORT 54979
#define DBSERVER "localhost"
#define DBNAME "opnetdb"
#define DBUSER "root"
#define DBPASS "dbpass"
#define OPINFO_PROT_VERSION "010"
#define MAIL_SCRIPT "/usr/local/opinfo/sendMail.pl"

struct opinfo_proto_v10 {
   char hostname[101];
   char mac_address[13];
   char reboot[2];
   char kernel_version[15];
   char load[10];
};
