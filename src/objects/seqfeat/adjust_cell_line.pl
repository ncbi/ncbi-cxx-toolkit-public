#!/usr/bin/perl
#
#This script is intended to reformat the text exported from the first
#and second active worksheets of the Excel table provided by ICLAC 
#listing bad cell lines for use by the validator library and BioSample 
#validation
#

use strict;

my $filename;
my $thisline;
my $next_line;
my $start = 0;
my @tokens;
my @new_table_tokens;
my $cell_line;
my $claimed_organism;
my $item;

$filename = shift || die "Must supply filename!\n";
open (IN, $filename) || die "Unable to open $filename!\n";
while ($thisline = <IN>) {
  $thisline =~ s/\n//g;
  $thisline =~ s/\r//g;
  $thisline =~ s/^\s+//;
  $thisline =~ s/\s+$//;

  if ($thisline =~ /^Table (1|2).  Misidentified cell lines/) {
    $start = 1;
  } elsif ($start) {
    $thisline = &get_rest_of_line($thisline);
    if ($thisline =~ /^Misidentified Cell Line/) {
      #skip header
    } else {
      @tokens = split(/\t/, $thisline);
      if (@tokens > 4) {
        @new_table_tokens = ();
        push (@new_table_tokens, $tokens[0]);
        push (@new_table_tokens, $tokens[1]);
        push (@new_table_tokens, $tokens[3]);
        push (@new_table_tokens, $tokens[4]);

        for ($item = 0; $item < @new_table_tokens; $item++) {
          $new_table_tokens[$item] =~ s/\"//g;
        }
        $new_table_tokens[1] = &fix_species($new_table_tokens[1]);
        $thisline = join("\t", @new_table_tokens);

        if ($thisline) {
          print "$thisline\n";
        }
      }
    }
  }
}
close(IN);

sub get_rest_of_line
{
  my $thisline = shift(@_);
  my $num_quote = 0;
  my $nextline;

  $num_quote = &count_chars($thisline, '"');
  while ($num_quote % 2 != 0 && ($nextline = <IN>)) {
    $nextline =~ s/\n//g;
    $nextline =~ s/\r//g;
    $num_quote += &count_chars($nextline, '"');
    $thisline .= " $nextline";
  }
  return $thisline;
}

sub count_chars
{
  my $thisline = shift(@_);
  my $char_to_count = shift(@_);
  my $count = 0;
  my $pos;

  $pos = index($thisline, $char_to_count);
  while ($pos != -1) {
    $count++;
    $thisline = substr($thisline, $pos + 1);
    $pos = index($thisline, $char_to_count);
  }
  return $count;
}

sub fix_species
{
  my $species = shift(@_);
  my %known_taxnames = (
    "Human", "Homo sapiens",
    "Cow", "Bos taurus",
    "Guinea pig", "Cavia porcellus",
    "Horse", "Equus caballus",
    "Rabbit", "Oryctolagus cuniculus");

  my %species_fixes = (
    "Canis familiaris", "Canis lupus familiaris"); 

  if ($species =~ /\((.*)\)/) {
    #scientific name in parentheses
    $species = $1;
  } elsif ($species =~ /.*\,\s*(\S+\s\S+)\s*$/) {
    #scientific name after comma
    $species = $1;
  } elsif ($known_taxnames{$species}) {
    $species = $known_taxnames{$species};
  }
  if ($species_fixes{$species}) {
    $species = $species_fixes{$species};
  }
  return $species;
}

