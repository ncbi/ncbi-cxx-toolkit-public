#! /usr/bin/perl -w
use strict;

my $allow_common = 1;

sub readln() {
  my $l = <>;
  chomp($l);
  return $l;
}

sub has_comma() {
  if ( !defined $1 ) {
    return 0;
  }
  $1 eq ',' or die "Bad comma: \"$1\"";
  return 1;
}

sub checkl($$) {
  $_[0] eq $_[1] or die "\"$_[0]\" != \"$_[1]\"\n";
}

my $match;

sub checkl_re($$) {
  $_[0] =~ m{^$_[1]$} or die "\"$_[0]\" doesn't match to \"$_[1]\"\n";
  $match = $1;
}

sub expect($) {
  checkl(readln(), $_[0]);
}

sub expect_re($) {
  checkl_re(readln(), $_[0]);
}

sub with_comma($) {
  my $l = readln();
  if ( $l eq $_[0] ) {
    return 0;
  }
  checkl($l, $_[0].',');
  return 1;
}

my $last_column = 0;

sub print_column_header($$) {
  my ( $id, $name ) = @_;
  print "            header {";
  print "\n              field-id $id" if $id ne '';
  print ',' if $id ne '' && $name ne '';
  print "\n              field-name \"$name\"" if $name ne '';
  print "\n            },\n";
}

sub start_column(@) {
  print "          {\n";
  print_column_header($_[0], $_[1]);
}

sub end_column() {
  print "\n          }";
  print ',' if !$last_column;
  print "\n";
}

sub print_int_data(@) {
  print "            data int {";
  for my $i (0..$#_) {
    print ',' if $i;
    print "\n              $_[$i]";
  }
  print "\n            }";
}

sub print_string_data(@) {
  if ( $allow_common ) {
    my %index;
    for my $i (0..$#_) {
      $index{$_[$i]} = 1;
    }
    my @common = keys(%index);
    if ( $#common < $#_/2-4 ) {
      @common = sort(@common);
      for my $i (0..$#common) {
        $index{$common[$i]} = $i;
      }
      print "            data common-string {\n";
      print "              strings {";
      for my $i (0..$#common) {
        print ',' if $i;
        print "\n                \"$common[$i]\"";
      }
      print "\n              },\n";
      print "              indexes {";
      for my $i (0..$#_) {
        print ',' if $i;
        print "\n                $index{$_[$i]}";
      }
      print "\n              }\n";
      print "            }";
      return;
    }
  }
  print "            data string {";
  for my $i (0..$#_) {
    print ',' if $i;
    print "\n              \"$_[$i]\"";
  }
  print "\n            }";
}

sub print_bytes_data(@) {
  if ( $allow_common ) {
    my %index;
    for my $i (0..$#_) {
      $index{$_[$i]} = 1;
    }
    my @common = keys(%index);
    if ( $#common < $#_/2-4 ) {
      @common = sort(@common);
      for my $i (0..$#common) {
        $index{$common[$i]} = $i;
      }
      print "            data common-bytes {\n";
      print "              bytes {";
      for my $i (0..$#common) {
        print ',' if $i;
        print "\n                '$common[$i]'H";
      }
      print "\n              },\n";
      print "              indexes {";
      for my $i (0..$#_) {
        print ',' if $i;
        print "\n                $index{$_[$i]}";
      }
      print "\n              }\n";
      print "            }";
      return;
    }
  }
  print "            data bytes {";
  for my $i (0..$#_) {
    print ',' if $i;
    print "\n              '$_[$i]'H";
  }
  print "\n            }";
}

sub print_sparse_index(@) {
  print ",\n            sparse indexes {";
  for my $i (0..$#_) {
    print ',' if $i;
    print "\n              $_[$i]";
  }
  print "\n            }";
}

sub print_int_column($$$@) {
  my ( $id, $name, $defval, @values ) = @_;
  start_column($id, $name);
  if ( @values ) {
    print_int_data(@values);
    print ",\n" if $defval ne '';
  }
  if ( $defval ne '' ) {
    print "            default int $defval";
  }
  end_column();
}

sub print_string_column($$$@) {
  my ( $id, $name, $defval, @values ) = @_;
  start_column($id, $name);
  if ( @values ) {
    print_string_data(@values);
    print ",\n" if $defval ne '';
  }
  if ( $defval ne '' ) {
    print "            default string \"$defval\"";
  }
  end_column();
}

sub print_bytes_column($$$@) {
  my ( $id, $name, $defval, @values ) = @_;
  start_column($id, $name);
  if ( @values ) {
    print_bytes_data(@values);
    print ",\n" if $defval ne '';
  }
  if ( $defval ne '' ) {
    print "            default bytes \'$defval\'H";
  }
  end_column();
}

sub print_sparse_int_column($$$@@) {
  my ( $id, $name, $defval, @values ) = @_;
  start_column($id, $name);
  if ( @values ) {
    my $n = int(($#values+1)/2);
    print_int_data(@values[$n..$#values]);
    print_sparse_index(@values[0..$n-1]);
    print ",\n" if $defval ne '';
  }
  if ( $defval ne '' ) {
    print "            default int \"$defval\"";
  }
  end_column();
}

sub print_sparse_string_column($$$@@) {
  my ( $id, $name, $defval, @values ) = @_;
  start_column($id, $name);
  if ( @values ) {
    my $n = int(($#values+1)/2);
    print_string_data(@values[$n..$#values]);
    print_sparse_index(@values[0..$n-1]);
    print ",\n" if $defval ne '';
  }
  if ( $defval ne '' ) {
    print "            default string \"$defval\"";
  }
  end_column();
}

my $gi;
my $gi_line;
my $point_line = '          location pnt {';
my @point;
my @QualityCodes;
my @dbSNP;
my @replace1;
my @replace2;

my @fuzz_lim;
my @fuzz_lim_index;
my @replace3;
my @replace3_index;
my @comment;
my @comment_index;

while (<>) {
  if ( $_ ne "      data ftable {\n" ) {
    if ( m|      tag id (\d+)| ) {
      $gi = $1;
      $gi_line = "            id gi $gi";
    }
    print;
    next;
  }
  for (;;) {
    expect('        {');
    expect('          data imp {');
    expect('            key "variation"');
    expect('          },');
    my $n = $#point+1;
    my $l = readln();
    if ( $l ne $point_line ) {
      checkl_re($l, '          comment "(.*)",');
      push @comment, $match;
      push @comment_index, $n;
      expect($point_line);
    }
    expect_re('            point ([0-9]+),');
    push @point, $match;
    expect('            strand plus,');
    if ( with_comma($gi_line) ) {
      expect('            fuzz lim tr');
      push @fuzz_lim, 3;
      push @fuzz_lim_index, $n;
    }
    expect('          },');
    expect('          qual {');
    expect('            {');
    expect('              qual "replace",');
    expect_re('              val "(.*)"');
    push @replace1, $match;
    expect('            },');
    expect('            {');
    expect('              qual "replace",');
    expect_re('              val "(.*)"');
    push @replace2, $match;
    if ( with_comma('            }') ) {
      expect('            {');
      expect('              qual "replace",');
      expect_re('              val "(.*)"');
      push @replace3, $match;
      push @replace3_index, $n;
      expect('            }');
    }
    expect('          },');
    expect('          ext {');
    expect('            type str "dbSnpQAdata",');
    expect('            data {');
    expect('              {');
    expect('                label str "QualityCodes",');
    expect_re("                data os '(.*)'H");
    push @QualityCodes, $match;
    expect('              }');
    expect('            }');
    expect('          },');
    expect('          dbxref {');
    expect('            {');
    expect('              db "dbSNP",');
    expect_re('              tag id (\d+)');
    push @dbSNP, $match;
    expect('            }');
    expect('          }');
    if ( !with_comma('        }') ) {
      last;
    }
  }
  print "      data seq-table {\n";
  my $n = $#point+1;
  print "        feat-type 8,\n";
  print "        feat-subtype 71,\n";
  print "        num-rows $n,\n";
  print "        columns {\n";
  $last_column = 0;
  print_string_column('data-imp-key', '', 'variation');
  print_int_column('location-gi', '', $gi);
  print_int_column('location-from', '', '', @point);
  print_int_column('location-strand', '', 1);
  if ( @fuzz_lim >= 0 ) {
    print_sparse_int_column('location-fuzz-from-lim', '', '',
                            @fuzz_lim_index, @fuzz_lim);
  }
  if ( @comment ) {
    print_sparse_string_column('comment', '', '',
                               @comment_index, @comment);
  }
  print_string_column('qual', 'Q.replace', '', @replace1);
  print_string_column('qual', 'Q.replace', '', @replace2);
  if ( @replace3 >= 0 ) {
    print_sparse_string_column('qual', 'Q.replace', '',
                               @replace3_index, @replace3);
  }
  print_string_column('ext-type', '', 'dbSnpQAdata');
  print_bytes_column('ext', 'E.QualityCodes', '', @QualityCodes);
  $last_column = 1;
  print_int_column('dbxref', 'D.dbSNP', '', @dbSNP);
  print "        }\n";
}
