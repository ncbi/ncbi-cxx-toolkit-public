#!/usr/bin/perl

use strict;

# simple SAX parser for OMSSA XML output
# sample usage:  "perl readOMSSA.pl test.xml"

# any standard SAX parser should work
# note that you need to install the perl SAX library for this to work

# use XML::LibXML::SAX;
# my $parser = XML::LibXML::SAX->new( Handler => $my_handler );

use XML::SAX::ParserFactory;
use OMSSA;

my $my_handler = OMSSA->new;
my $parser = XML::SAX::ParserFactory->parser( Handler => $my_handler );

foreach my $instance (@ARGV) {
        $parser->parse(Source => { SystemId => $instance });
}


