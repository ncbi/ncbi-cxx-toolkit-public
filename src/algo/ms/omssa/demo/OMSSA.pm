package OMSSA;

require 5.005_62;
use strict;
use XML::SAX::Base;
our @ISA = ('XML::SAX::Base');
our $VERSION = '0.01';

sub new {
        my ($type) = @_;
        return bless {}, $type;
}

my $current_element = '';

sub start_element {
        my ($self, $element) = @_;

    $current_element = $element->{Name};

    if ($element->{Name} eq 'MSHitSet') {
      print "Start of Hitset (hits to a particular spectrum)\n";
    }
    elsif ($element->{Name} eq 'MSHits') {
      print " Start of a hit\n";
    }
    elsif ($element->{Name} eq 'MSPepHit') {
      print "  Start of peptides matching a hit\n";
    }

}


sub end_element {
        my ($self, $element) = @_;

    if ($element->{Name} eq 'MSHitSet') {
      print "End of Hitset\n";
    }
    elsif ($element->{Name} eq 'MSHits') {
      print " End of a hit\n";
    }
    elsif ($element->{Name} eq 'MSPepHit') {
      print "  End of peptides\n";
    }

}


sub characters {
    my ($self, $characters) = @_;
    my $text = $characters->{Data};
    $text =~ s/^\s*//;
    $text =~ s/\s*$//;
    return '' unless ($text ne "");

    if ($current_element eq 'MSHitSet_number') {
      print "Hitset number: " . $text . "\n";
    }
    elsif ($current_element eq 'MSHits_evalue') {
      print " Hit E-value: " . $text . "\n";
    }
    elsif ($current_element eq 'MSHits_charge') {
      print " Hit charge: " . $text . "\n";
    }
    elsif ($current_element eq 'MSPepHit_start') {
      print "  Peptide start: " . $text . "\n";
    }
    elsif ($current_element eq 'MSPepHit_stop') {
      print "  Peptide stop: " . $text . "\n";
    }
    elsif ($current_element eq 'MSPepHit_gi') {
      print "  Peptide found in protein id: " . $text . "\n";
    }
    elsif ($current_element eq 'MSPepHit_defline') {
      print "  Peptide found in protein with defline: " . $text . "\n";
    }
    elsif ($current_element eq 'MSHits_pepstring') {
      print "  Peptide sequence: " . $text . "\n";
    }
    elsif ($current_element eq 'MSHitSet_ids_E') {
      print "Hitset id: " . $text . "\n";
    }
}


1;

