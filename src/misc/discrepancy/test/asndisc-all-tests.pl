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
my $quiet;
my $dev_tests;

my $usage = <<"<<END>>";
USAGE:
      $exe [-c <c-version-exe>] [-cpp <cpp-version-exe>] [-keep] [-quiet] [-dev]
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
  if ($ARGV[$n] eq '-quiet')
  { $quiet = 1;
    next;
  }
  if ($ARGV[$n] eq '-keep')
  { $keep_output = 1;
    next;
  }
  if ($ARGV[$n] eq '-dev')
  { $dev_tests = 1;
    next;
  }
}

die $usage if $exe_c eq '' || $exe_cpp eq '';

my $testdata = File::Spec->catfile($path, 'test-data.txt');
my $script = File::Spec->catfile($path, 'asndisc-test.pl');

open DATA, "<$testdata" or die "Cannot open $testdata\n";
my @lines = <DATA>; close DATA;
foreach my $line (@lines)
{ chomp $line;
  $line=~s/#.*$//;
  next unless $line=~/(\S+)\s+(\S+)\s+(\S+)\s+(\S+)(\s+-X)?(?:\s+(\S+))?/;
  die "Test name conflict: $1\n" if $tests{$1};
  $tests{$1}{arg} = $2;
  $tests{$1}{arg0} = $3;
  $tests{$1}{data} = $4;
  $tests{$1}{ext} = $5;
  $tests{$1}{gold} = $6;
}

die "$exe_c does not exist!\n" unless -e $exe_c;
die "$exe_cpp does not exist!\n" unless -e $exe_cpp;

###
###   Get the test list
###

my %all_cases;

my $reading = 0;
foreach my $line (`$exe_cpp -help`)
{ chomp $line;
  if ($line eq 'TESTS'){ $reading = 1; next;}
  next unless $reading;
  $line=~s/\/.*$//; $line=~s/^\s*//; $line=~s/\s*$//;
  $all_cases{$line} = 0 if $line ne '';
}

###
###   Run all tests
###

my $pass = 0;
my $fail = 0;

foreach my $test (sort keys %tests)
{ next if $test=~/^_/ && !$dev_tests;
  $all_cases{$tests{$test}{arg}}++;
  my $cmd = "perl $script $test -c $exe_c -cpp $exe_cpp";
  $cmd = "$cmd -keep" if $keep_output;
  $cmd = "$cmd -quiet" if $quiet;
  print "##teamcity[testStarted name='$test' captureStandardOutput='true']\n";
  open(OLD_STDOUT, '>&STDOUT') if $quiet;
  open(STDOUT, '>/dev/null') if $quiet;
  my $result = system($cmd);
  open(STDOUT, '>&OLD_STDOUT') if $quiet;
  
  print "##teamcity[testFailed name='$test' message='$result']\n" if ($result);
  print "##teamcity[testFinished name='$test']\n";

  if ($result)
  { print "$test: FAIL!\n\n";
    $fail++;
  }
  else
  { print "$test: PASS!\n\n";
    $pass++;
  }
}

my $covered = 0;

foreach my $key (sort keys %all_cases)
{ $covered++ if $all_cases{$key};
}

my $cases = scalar keys %all_cases;
print "\nCOVERED: $covered of $cases\n";
if ($covered != $cases)
{ print "MISSING:\n";
  foreach my $key (sort keys %all_cases)
  { print "\t$key\n" unless $all_cases{$key};
  }
}

my $total = $pass + $fail;
print "\nPASS: $pass of $total\n";
print "FAIL: $fail\n" if $fail;

die print "FAIL: $fail of $total\n" if $fail;
