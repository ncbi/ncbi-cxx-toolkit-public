#!/usr/bin/perl -w

use strict;



my $new_program = "./igblastn";
my $old_program = "/export/home/jianye/igblast/trunk/c++/src/app/blast/igblastn";
my $testcasefile = "igblastntestcase";
my $parameters = " -germline_db_V Human_gl_V -germline_db_J Human_gl_J -germline_db_D Human_gl_D -query $testcasefile"; 


system ("rm *.out");
system ("rm *.diff");

print ("$parameters\n");
print ("old\n");

system("time $old_program $parameters > $testcasefile.old.out");
print ("new\n");
system("time $new_program $parameters > $testcasefile.new.out");
system("diff  $testcasefile.old.out $testcasefile.new.out > $testcasefile.diff");
