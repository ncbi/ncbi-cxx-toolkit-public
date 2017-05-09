#!/usr/bin/perl
#
use strict;
my $file;
my @fatal;
my @critical;
my @error;
my @warning;
my @info;
my @unknown;
my $text;
my $count;

opendir (DIR, ".") or die $!;
while (my $file = readdir(DIR)) {
    if ($file =~ /cpp/) {
        &process_file($file);
    }
}
closedir(DIR);

$count = @fatal;
print "FATAL: $count\n";
$count = @critical;
print "REJECT: $count\n";
$count = @error;
print "ERROR: $count\n";
$count = @warning;
print "WARNING: $count\n";
$count = @info;
print "INFO: $count\n";
$count = @unknown;
print "Unknown: $count\n";
print "\n";

$count = @fatal;
print "\nFATAL: $count\n";
foreach $text (@fatal) {
  print "$text\n";
}

$count = @critical;
print "\nREJECT: $count\n";
foreach $text (@critical) {
  print "$text\n";
}

$count = @error;
print "\nERROR: $count\n";
foreach $text (@error) {
  print "$text\n";
}

$count = @warning;
print "\nWARNING: $count\n";
foreach $text (@warning) {
  print "$text\n";
}

$count = @info;
print "\nINFO: $count\n";
foreach $text (@info) {
  print "$text\n";
}

$count = @unknown;
print "\nUnknown: $count\n";
foreach $text (@unknown) {
  print "$text\n";
}


sub process_file
{
  my $file = shift(@_);
  my $thisline;
  my $line_num = 1;
  my $in_err = 0;
  my $err_text = "";

  open (IN, $file) || die "Unable to open $file\n";
  while($thisline=<IN>) {
    $thisline =~ s/\r//;
    $thisline =~ s/\n//;
    if ($thisline =~ /PostErr/) {
      $in_err = 1;
    }
    if ($in_err) {
      $err_text .= $thisline;
      if ($thisline =~ /\)/) {
        $in_err = 0;
      }
    } elsif ($err_text) {
      $err_text =~ s/\s+/ /g;
      if ($err_text =~ /eDiag_Critical/) {
        $err_text =~ s/\s*PostErr\s*\(\s*eDiag_Critical\s*\,\s*eErr_//;
        push @critical, $err_text;
      } elsif($err_text =~ /eDiag_Error/) {
        $err_text =~ s/\s*PostErr\s*\(\s*eDiag_Error\s*\,\s*eErr_//;
        push @error, $err_text;
      } elsif($err_text =~ /eDiag_Warning/) {
        $err_text =~ s/\s*PostErr\s*\(\s*eDiag_Warning\s*\,\s*eErr_//;
        push @warning, $err_text;
      } elsif($err_text =~ /eDiag_Info/) {
        $err_text =~ s/\s*PostErr\s*\(\s*eDiag_Info\s*\,\s*eErr_//;
        push @info, $err_text;
      } elsif ($err_text =~ /eDiag_Fatal/) {
        $err_text =~ s/\s*PostErr\s*\(\s*eDiag_Fatal\s*\,\s*eErr_//;
        push @fatal, $err_text;
      } elsif(!&exclude_err($err_text)) {
        push @unknown, "$file:$line_num $err_text";
      }
      $err_text = "";
    }
    $line_num++; 
  }
}


sub exclude_err
{
  my $err = shift(@_);

  if ($err =~ /PostErr\s*\(sv, et, msg/) {
    return 1;
  } elsif ($err =~ /CValidError_base/) {
    return 1;
  } elsif ($err =~ /CValidError_imp/) {
    return 1;
  } elsif ($err =~ /^\s*\/\//) {
    return 1;
  }

  return 0;
}

