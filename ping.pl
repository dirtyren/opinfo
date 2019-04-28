#!/usr/bin/perl -w
use Net::Ping;

local ($ip)=@ARGV;

my $p=Net::Ping->new("icmp");

local $number_times=0;
while($number_times<10) {
   if($p->ping($ip)) {
      exit(1);
   }
   $number_times++;
}
exit(0);
