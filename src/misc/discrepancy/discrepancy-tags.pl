#!/usr/bin/perl
use Cwd;
use strict;

###
### Synchronise the tags in DISCREPANCY_CASE macros with the discrepancy-tags.txt
###

my $dir = cwd;
die "Run this script in the discrepancy source folder!\n" unless $dir=~/\/discrepancy$/;
opendir(my $d, $dir) or die "Cannot open directory!";
my @files = readdir($d); closedir($d);

my %ttt;
open FILE, "<discrepancy-tags.txt" or die "Cannot open discrepancy-tags.txt\n";
my @lines = <FILE>; close FILE;

foreach my $line(@lines)
{ chomp $line;
  $line=~s/#.*//;
  next unless $line=~/^(\S+)\s+(\S+)\s+(\S+)/;
  $ttt{$2} = $1;
}

foreach my $file(@files)
{ next unless $file=~/\.cpp/;
  process($file);
}

sub process
{ my $file = shift;
  print "$file\n";
  open FILE, "<$file" or die "Cannot read $file\n";
  my @lines = <FILE>; close FILE;
  open FILE, ">$file" or die "Cannot write $file\n";
  my $count = 0;
  my $changed = 0;
  foreach my $line(@lines)
  { if ($line=~/DISCREPANCY_CASE\(\s*(\w+)\s*,\s*(\w+)\s*,\s*([^,]+)\s*,\s*"(.*)"\s*\)/)
    { my $test = $1;
      my $type = $2;
      my $group = $3;
      my $comm = $4;
      my $ggg = '';
      $ggg = 'eDisc' if $ttt{$test}=~/D/;
      $ggg = $ggg eq '' ? 'eOncaller' : "$ggg | eOncaller" if $ttt{$test}=~/O/;
      $ggg = $ggg eq '' ? 'eSubmitter' : "$ggg | eSubmitter" if $ttt{$test}=~/S/;
      $ggg = $ggg eq '' ? 'eSmart' : "$ggg | eSmart" if $ttt{$test}=~/T/;
      $ggg = 0 if $ggg eq '';
      if ($ggg ne $group)
      { print "$test:  $group  =>  $ggg\n";
        $changed++;
      }
      $line = "DISCREPANCY_CASE($test, $type, $ggg, \"$comm\")\n";
      $count++;
    }
    elsif ($line=~/DISCREPANCY_CASE/)
    { print "##### Please change to a single line:\n$line";
    }
    print FILE $line;
  }
  close FILE;
  print "$changed of $count changed\n";
}
