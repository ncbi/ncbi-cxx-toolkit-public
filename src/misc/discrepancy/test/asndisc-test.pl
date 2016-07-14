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
my $keep_output;
my $quiet;
my $nogold;

my $usage = <<"<<END>>";
USAGE:
      $exe <TEST_NAME> [-c <c-version-exe>] [-cpp <cpp-version-exe>] [-keep] [-quiet] [-nogold]
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
  if ($ARGV[$n] eq '-quiet')
  { $quiet = 1;
    next;
  }
  if ($ARGV[$n] eq '-nogold')
  { $nogold = 1;
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

die "Test $test not found\n" unless exists $tests{$test};

###
###   Run test
###

my $arg = $tests{$test}{arg};
my $arg0 = $tests{$test}{arg0};
my $autofix = 1 if $arg0 eq '__AUTOFIX__';
my $input = File::Spec->catfile($path, 'test-data', $tests{$test}{data});
my $out_c = "$test.c.txt";
my $out_cpp = $autofix ? "$test.cpp.asn" : "$test.cpp.txt";
my $gold = File::Spec->catfile($path, 'test-data', $tests{$test}{gold}) if $tests{$test}{gold} ne '' && !$nogold;
my $data = $tests{$test}{data};
my $rx = join('|', split(',', $arg.','.$arg0));
my $ip = -d $input ? 'p' : 'i';
my $cmd;
if ($autofix)
{ $cmd = "$exe_cpp -e $arg -i $input -o $out_cpp -r . -F";
}
else
{ $cmd = "$exe_cpp -e $arg -$ip $input -o $out_cpp";
  $cmd .= " -X ALL" if $tests{$test}{ext};
}

open(OLD_STDOUT, '>&STDOUT') if $quiet;
open(STDOUT, '>/dev/null') if $quiet;
my $result = system($cmd);
open(STDOUT, '>&OLD_STDOUT') if $quiet;
die "Error running $exe_cpp\n" if $result;

if ($autofix)
{ my $name = $tests{$test}{data};
  $name=~s/\.asn$|\.sqn$/.autofix.asn/;
  rename $name, $out_cpp;
}
elsif ($gold eq '')
{
  my $ip = -d $input ? 'p' : 'i';
  $cmd = "$exe_c -e $arg0 -$ip $input -o $out_c";
  $cmd .= " -X ALL" if $tests{$test}{ext};
  open(OLD_STDOUT, '>&STDOUT') if $quiet;
  open(STDOUT, '>/dev/null') if $quiet;
  my $result = system($cmd);
  open(STDOUT, '>&OLD_STDOUT') if $quiet;
  die "Error running $exe_c\n" if $result;
  $gold = $out_c;
}


###
###   Compare output
###

if ($autofix)
{ match_files($out_cpp, $gold) if $gold ne '';
  unlink $out_cpp if !$keep_output;
}
else
{ my %x = read_output($gold);
  my %y = read_output($out_cpp);
  compare_output(\%x, \%y);
  if (!$keep_output)
  { unlink $out_cpp;
    unlink $out_c if $gold eq $out_c;
  }
}
print "SUCCESS!\n";


sub read_output
{ my %obj;
  my $fname = shift;
  open FILE, "<$fname" or die "Cannot open $fname\n";
  my @lines = <FILE>; close FILE;
  my $state = 0;
  my $current;
  foreach my $line (@lines)
  { chomp $line;
    $line=~s/^\s+//; $line=~s/\s+$//;
    next if $line eq '';
    if ($line eq 'Summary'){ $state = 1; next; }
    if ($line eq 'Detailed Report'){ $state = 2; next; }
    next unless $state;
    if ($state == 1)
    { my $msg = $line;
      $msg = $1 if $line=~/(?:$rx):\s*(.*)/;
      $msg = normalize($msg);
      $msg = "$arg: $msg";
      $obj{summary}{$msg}++;
      next;
    }
    if ($state == 2)
    { my $msg = $line;
      if ($line=~/(?:$rx)::?\s*(.*)/)
      { $msg = normalize($1);
        $msg = "$arg: $msg";
        $current = $msg;
        next;
      }
      if ($line=~/$data:\s*(.*)/)
      { $msg = normalize_detail($1);
        $obj{details}{$current}{$msg}++;
        next;
      }
    }
  }
  return %obj;
}

# Fix descriptions and headings
sub normalize
{ my $str = shift;

  ### add special cases here!
  $str=~s/sequences in record$/sequences are present/;
  $str=~s/>/more than/g;
  $str=~s/\.$//;

  my @words = split(' ', $str);
  for (my $i = 0; $i < scalar @words; $i++){ $words[$i] = normalize_word($words[$i]); }
  return join(' ', @words);
}

sub normalize_word
{ my $word = shift;
  return '[is]'   if $word eq 'is'  || $word eq 'are';
  return '[does]' if $word eq 'do'  || $word eq 'does';
  return '[has]'  if $word eq 'has' || $word eq 'have';
  return $1 if $word=~/(.+)s$/;
  return $word;
}

# Fix data lines, like coordinates of features.
sub normalize_detail
{ my $str = shift;
  $str=~s/(\(\S+,\s)(\S+:)/$1/g;
  $str=~s/^lcl\|//g;
  $str=~s/misc_RNA/RNA/g;
  $str=~s/\/inference=//g;
  # C seems to prefer "()" for seq-loc-mix but C++ seems to prefer "[]"
  $str=~s/\s*\(\d+ components?\)//g;
  $str=~s/[][)(]//g;
  #print "<< $str\n";
  return $str;
}

sub compare_output
{ my $xx = shift;
  my $yy = shift;
  my %x = %$xx;
  my %y = %$yy;

  my %kkk;
  foreach my $k (sort keys %{$x{summary}}){ $kkk{$k} = 0; }
  foreach my $k (sort keys %{$y{summary}}){ $kkk{$k} = 0; }
  foreach my $k (sort keys %kkk){ die "Mismatching summary line:\n$k\n" if $x{summary}{$k} != $y{summary}{$k};}
  %kkk = ();
  foreach my $k (sort keys %{$x{details}}){ $kkk{$k} = 0; }
  foreach my $k (sort keys %{$y{details}}){ $kkk{$k} = 0; }
  foreach my $k (sort keys %kkk)
  { my %mmm;
    foreach my $m (sort keys %{$x{details}{$k}}){ $mmm{$m} = 0; }
    foreach my $m (sort keys %{$y{details}{$k}}){ $mmm{$m} = 0; }
    print "> $k\n";
    foreach my $m (sort keys %mmm)
    { die "Mismatching item:\n$k\n$m\n" if $x{details}{$k}{$m} != $y{details}{$k}{$m};
      print ">> $m\n";
    }
  }
}

sub match_files
{ my $f0 = shift;
  my $f1 = shift;
  open FILE, "<$f0" or die "Cannot open $f0\n";
  my @ff0 = <FILE>; close FILE;
  open FILE, "<$f1" or die "Cannot open $f1\n";
  my @ff1 = <FILE>; close FILE;
  my $len = scalar @ff0;
  $len = scalar @ff1 if $len < scalar @ff1;
  for (my $i = 0; $i < $len; $i++)
  { my $a = $ff0[$i]; chomp $a; $a=~s/^\s+//; $a=~s/\s+$//;
    my $b = $ff1[$i]; chomp $b; $b=~s/^\s+//; $b=~s/\s+$//;
    die "Mismatching line $i: $a / $b\n" if $a ne $b;
  }
}
