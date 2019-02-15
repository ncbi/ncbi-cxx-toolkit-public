#!/usr/bin/perl  -w


use strict;
my $inputfile=shift (@ARGV);

open(in_handle, $inputfile);

while(my $line=<in_handle>){
 #print ("line = $line\n");
  if ($line =~ /^>.*\|(TR.+)\|.*\|.*\|.*\|.*\|.*\|.*\|.*\|.*\|.*\|.*\|.*\|.*\|.*\|/){
    print(">$1\n");
  } elsif ($line =~ /^>.*\|(IG.+)\|(.*)\|.*\|.*\|.*\|.*\|.*\|.*\|.*\|.*\|.*\|.*\|.*\|.*\|/){
    my $species = $2;
    my $id = $1;
    if ($species =~ /Mus\s+spretus/i) { #deal with mixed mouse species
      $id = $id."_Mus_spretus";
    }
    
    print(">$id\n");
  } else {
    $line =~ s/\.+//g;
    #get rid of dot
    print("$line");
  }
}

close (in_handle);
