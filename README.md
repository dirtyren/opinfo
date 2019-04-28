## Welcome to opinfo ##

opinfo is a old project that implements and TCP client and server in C.
It also implements and message protocol between the server and client.
The original goal was for the client to send some linux details to a server,
and the server would store the details on a mysql database and keep a dynamic
DNS updated. The server would also monitor the hosts is a very simple way.

The main goal here is to keep it as a example on how to create a multiprocesc TCP
server / client, show a very simple message protocol implemented.

## Contents

 * opinfo-server: runs the server and listens for client data
 * opinfo-client: runs on the linux client and periodically sends data to the server
 * opinfo-gendns: keeps the DNS zone updated
 * opinfo-monitor: monitors if the clients are up

### Installation ###

Before installing the client and server, edit opinfo.h to set up your server
and client details and mysql database location, user and password.
Use the db.sql to create the opnetdb database on your mysql server.
You can also comment out all related functions to the mysql dababase if you
just want to try the TCP server / client.
This code used to run on CentOS 5 and CentOS 6 so it should compile on newer
linux versions with little effort.
Just run make and the code should compile clean.
