#!/usr/bin/perl

($from, $mailTo, $subject)=@ARGV;

$body=<STDIN>;
# Gets hostname
#open (P, "/bin/hostname |");
#while (<P>) {
#   $desc=$_;
#   chop($desc);
#}
#$HOSTNAME=$desc;

# Gets System Date
#open (P, "/bin/date |");
#while (<P>) {
#   $desc=$_;
#   chop($desc);
#}
#$SysDate=$desc;

open(MAIL, '| /usr/lib/sendmail -t -oi');
print MAIL <<EOF;
To: $mailTo
Cc:
From: $from
Subject: $subject
$body
EOF
close MAIL;
