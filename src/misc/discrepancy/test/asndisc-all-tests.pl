#!/usr/bin/perl
use strict;
use File::Spec;
use File::Basename;

my $exe = basename($0);
my $path = dirname($0);
my $exe_c = 'asndisc';
my $exe_cpp;
my %tests;
my $keep_output;
my $script = 'asndisc-test.pl';

my $usage = <<"<<END>>";
USAGE:
      $exe [-c <c-version-exe>] [-cpp <cpp-version-exe>] [-keep]
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
  if ($ARGV[$n] eq '-keep')
  { $keep_output = 1;
    next;
  }
}

die $usage if $exe_c eq '' || $exe_cpp eq '';

my $testdata = File::Spec->catfile($path, 'test-data.txt');

open DATA, "<$testdata" or die "Cannot open $testdata\n";
my @lines = <DATA>; close DATA;
foreach my $line (@lines)
{ chomp $line;
  $line=~s/#.*$//;
  next unless $line=~/(\S+)\s+(\S+)\s+(\S+)\s+(\S+)(?:\s+(\S+))?/;
  $tests{$1}{arg} = $2;
  $tests{$1}{arg0} = $3;
  $tests{$1}{data} = $4;
  $tests{$1}{gold} = $5;
}

die "$exe_c does not exist!\n" unless -e $exe_c;
die "$exe_cpp does not exist!\n" unless -e $exe_cpp;


###
###   Run all tests
###

my $pass = 0;
my $fail = 0;

foreach my $test (sort keys %tests)
{ my $cmd = "$script $test -c $exe_c -cpp $exe_cpp";
  #print STDERR "running: $cmd\n";
  open(OLD_STDOUT, '>&STDOUT');
  open(STDOUT, '>/dev/null');
  my $result = system($cmd);
  open(STDOUT, '>&OLD_STDOUT');

  if ($result)
  { print "$test: FAIL!\n";
    $fail++;
  }
  else
  { print "$test: PASS!\n";
    $pass++;
  }
}

my $total = $pass + $fail;
print "\nPASS: $pass of $total\n";
print "FAIL: $fail\n" if $fail;
