#!/usr/bin/perl
use strict;
use File::Spec;
use File::Basename;

my $exe = basename($0);
my $path = dirname($0);
my $test;
my $exe_c = 'asndisc';
my $exe_cpp;
my %tests;

my $usage = <<"<<END>>";
USAGE:
      $exe <TEST_NAME> [-c <c-version-exe>] [-cpp <cpp-version-exe>]
<<END>>

for (my $n = 0; $n < scalar @ARGV; $n++)
{ if ($ARGV[$n] eq '-c')
  { $n++;
    $exe_c = $ARGV[$n];
    next;
  }
  if ($ARGV[$n] eq '-cpp')
  { $n++;
    $exe_cpp = $ARGV[$n];
    next;
  }
  if ($test eq '')
  { $test = $ARGV[$n];
    next;
  }
}

die $usage if $test eq '' || $exe_c eq '' || $exe_cpp eq '';

my $testdata = File::Spec->catfile($path, 'test-data.txt');

open DATA, "<$testdata" or die "Cannot open $testdata\n";
my @lines = <DATA>;
foreach my $line (@lines)
{ chomp $line;
  $line=~s/#.*$//;
  next unless $line=~/(\S+)\s+(\S+)\s+(\S+)/;
  $tests{$1}{cmd} = $2;
  $tests{$1}{data} = $3;
}

die "Test $test not found" unless exists $tests{$test};

my $input = File::Spec->catfile($path, 'test-data', $tests{$test}{data});
my $out_c = "$test.c.txt";
my $out_cpp = "$test.cpp.txt";

my $cmd = "$exe_c -e $test -i $input -o $out_c";
print STDERR "running: $cmd\n";
die "Error running $exe_c\n" if system($cmd);

my $cmd = "$exe_cpp -e $test -i $input -o $out_cpp";
print STDERR "running: $cmd\n";
die "Error running $exe_cpp\n" if system($cmd);


