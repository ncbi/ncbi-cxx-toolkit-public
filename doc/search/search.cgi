#!/usr/bin/perl

##########################################################################
# COPYRIGHT NOTICE:
# 
# Copyright FocalMedia.Net 2001
# 
# This program is free to use for commercial and non commercial use. 
# This script may be used and modified by anyone, so long as this copyright 
# notice and the header above remain intact. Selling the code for this 
# program without prior written consent from FocalMedia.Net
# is expressly forbidden.
# 
# This program is distributed "as is" and without warranty of any
# kind, either express or implied.
#
##########################################################################

##########################################################################
# Configuration - Edit Below
##########################################################################

## This is the url location of fmsearch.cgi
$fmsearch = "./search.cgi";

## This is the url location of SSI filter
##$ssifilter = "./perlssi.pl";

## What directories on your server/hosting account would you like to search
## If there is more than one directory, seperate them with a comma

@searchdirs = (
               "",
##               "docxx",
               "libs",
               "libs/gui",
               "libs/om",
               "lxr",
               "programming_manual",
               "programming_manual/CARdemo",
               "tools",
               "tools/datatool",
               "tools/dispatcher",
               "tools/hello",
               "tools/id1_fetch"
              );

## If there is more than one directory, seperate them with a comma

if ($ENV{'SCRIPT_FILENAME'} =~ m"/private/htdocs") {
    $webbase    = "http://intranet.ncbi.nlm.nih.gov/ieb/ToolBox/CPP_DOC";
    $searchbase = "/web/private/htdocs/intranet/ieb/ToolBox/CPP_DOC";
    @searchdirs_private = ("internal", "internal/idx");
    push(@searchdirs, @searchdirs_private);
} else {
    $webbase    = "http://www.ncbi.nlm.nih.gov/IEB/ToolBox/CPP_DOC";
    $searchbase = "/web/public/htdocs/IEB/ToolBox/CPP_DOC";
}

## What is the extensions of the files to be searched
## If you want to use more than one extension seperate them via comma
@searchext = (".html", ".htm");

## How many items to list per page
$listperpage = "20";

## How many symbols aproximately must be showns from each side from
## founded keyword in a headline
$searchgap = 200;


##########################################################################
# Don't change anything below this line unless you know what you are doing.
##########################################################################

&get_env;

print "Content-type: text/html\n\n";

$fsize = (-s "./main.html");
open (RVF, "./main.html");
read(RVF, $mainlayout, $fsize);
close(RVF);

$fsize = (-s "./listings.html");
open (RVF, "./listings.html");
read(RVF, $listings, $fsize);
close(RVF);


# Preprocess keywords

$x_keywords = $fields{'keywords'};
$x_keywords =~ s/^\s+//;
$x_keywords =~ s/\s+$//;

if ($x_keywords eq "") {
   $x_keywords = "No-Keywords-Specified";
} else {
   $x_keywords = substr(&html_quote($x_keywords), 0, 70);
}


### GET FILES

$itec = 0;

foreach $item (@searchdirs) {
    $itempath = "$searchbase/$item";
    if (-e "$itempath") {
       opendir(DIR, "$itempath");
       @files = readdir(DIR);
       closedir(DIR);
       $fnc = 0;
       @sfiles = ();
       foreach $fn (@files) {

          ### SORT FILES

          foreach $ext (@searchext) {
             if ($ext eq substr($fn,length($fn)-length($ext),length($ext))) {
                if ($item eq "") {
                   $sfiles[$fnc] = "$fn";
                } else {
                   $sfiles[$fnc] = "$item/$fn";
                }
                $fnc++;
             }
          }
       }
    } else {
###       print "Error - $item does not exist.";
    }
    
    $itec++;
    $tmpc = push(@allfiles, @sfiles);
}


### PREPARE KEYWORDS

if ($x_keywords =~ / /) {
   @keywords = split (/ /,$x_keywords);
} else {
   $keywords[0] = $x_keywords;
}

### SEARCH FILES

$srcntr = 0;
$highsr = 0;

foreach $item (@allfiles) {

   $url = "$webbase/$item";
   $item = "$searchbase/$item";

   $mrelevance = 0;

   $fsize = (-s "$item");
   open(RVF, "$item");
   read(RVF, $rdata, $fsize);
   close(RVF);
  
   $rdata = &strip_html($rdata);  
   $headline = "";
   $headline_have = 0;

   ### GET RELEVANCE
    
   foreach $kw (@keywords) {

     $lrelevance = 0;

      # Filename matching
      if ($url =~ /$kw/i) { 
         $lrelevance += 10;
      }

      # File data matching

      if ($rdata =~ /$kw/i) {
         @filelines = split(/\n/,$rdata);
         foreach $oln (@filelines) {
            if (($oln =~ /$x_keywords/i) and 
               ($x_keywords =~ " ")) {
               $lrelevance += 15;
            }
            if ($oln =~ /$kw/i) { 
               $lrelevance++;
            }             
         }
         # Get headline
         if ($lrelevance != 0) {
            if ($headline_have != 1) {
               $hl = &get_headline($rdata,$x_keywords);
               if ($hl ne "") {
                  $headline = $hl;
                  $headline_have = 1;
               }
            }
            if (($headline_have != 1) and !($headline =~ /$kw/i)) {
               $headline .= " ";
               $headline .= &get_headline($rdata,$kw);
            }
         }

       } ### FOUND MATCH

       # Keyword not found -- AND search method: relevance -> 0
       if ($lrelevance == 0) {
          $mrelevance = 0;
          goto ("donesearch")[0];
       }
       $mrelevance += $lrelevance;

   } ### KEYWORD LOOP

donesearch:
   if ($mrelevance > $highsr) {$highsr = $mrelevance;}
   if ($mrelevance > 0) {
       $ptitle = &get_page_details($item);
       ###print "FOUND MATCH IN $item - Relevance = $mrelevance -> $ptitle <br><b></b>";
       $mrelevance = &convert_rel2str($mrelevance);
       $sresults[$srcntr] = $mrelevance . "\t" . $srcntr;
       if ($headline ne "") {
          foreach $kw (@keywords) {
             $headline = &mark_headline($headline, $kw);
          }
          $headline .= "<br>";
       }
       $sresults_data[$srcntr] = $item . "\t" . $url . "\t" . $ptitle . "\t" . $headline;
       $srcntr++;
   }
	
} ### FILE LOOP

@sresults = sort(@sresults);
$allresults = push(@sresults);
$tmr = $allresults;

foreach $item (@sresults) {
   $tmr = $tmr - 1;
   $sresults_inverted[$tmr] = $item;
}


##########################################################################

$modp = ($allresults % $listperpage );
$pages = ($allresults - $modp) / $listperpage;
if ($modp != 0) {$pages++;}

if ($fields{'st'} eq "") {$fields{'st'} = 0;}
if ($fields{'nd'} eq "") {$fields{'nd'} = $listperpage;}

$pitem = "";
$ippc = 1;

$mainlayout =~ s/!!Matches!!/$allresults/gi;
$mainlayout =~ s/!!Keywords!!/$x_keywords/gi;

foreach $item (@sresults_inverted) {
   if (($ippc > $fields{'st'}) and ($ippc <= $fields{'nd'})) {
        
		($relevance, $index) = split(/\t/,$item);
		($sp, $url, $ptitle, $headline) =split(/\t/,$sresults_data[$index]);

		if ($ptitle eq "") {$ptitle = "Untitled";}
   
		$relevance = int($relevance / $highsr * 100);
		
		$wls = $listings;
		$wls =~ s/!!Index!!/$ippc/gi;
		$wls =~ s/!!Title!!/$ptitle/gi;
		$wls =~ s/!!Headline!!/$headline/gi;
		$wls =~ s/!!Url!!/$url/gi;
		$wls =~ s/!!Relevance!!/$relevance%/gi;
		
		$totalresults = $totalresults . $wls;
   }
   $ippc++;  
}

$kw = $fields{'keywords'};
$kw =~ s/ /+/g;

for ($ms = 0; $ms < $pages; $ms++) {
   $pg = $ms + 1;
   if ($fields{'nd'} == ($pg * $listperpage)) {
      $pgstring = $pgstring . " [$pg] ";
   } else {
      $st = ($pg * $listperpage) - $listperpage;
      $nd = ($pg * $listperpage);
      $pgstring = $pgstring ."<a href=\"$fsearch?keywords=$kw&st=$st&nd=$nd\">$pg</a> ";
   }
}

if ($pages > 1) {
   $mainlayout =~ s/!!Pages!!/Pages: $pgstring/gi;
} else {
   $mainlayout =~ s/!!Pages!!//gi;
}

# PREV NEXT

$spls = $modp;
if ($spls == 0) {$spls++;}

if ($fields{'nd'} <= ($allresults - $spls)) {
   $st = $fields{'st'} + $listperpage;
   $nd = $fields{'nd'} + $listperpage;
   $nextt = "<a href=\"$fmsearch?keywords=$kw&st=$st&nd=$nd\">Next Page</a> ";
}

if ($fields{'st'} > 0) {
   $st = $fields{'st'} - $listperpage;
   $nd = $fields{'nd'} - $listperpage;
   $prev = "<a href=\"$fmsearch?keywords=$kw&st=$st&nd=$nd\">Prev Page</a> ";
}

if (($prev ne "") and ($nextt ne "")) {
   $spcer = " | ";
} else {
   $spcer = " ";
}

$mainlayout =~ s/!!SearchResults!!/$totalresults/gi;
$mainlayout =~ s/!!PagesNavigate!!/$prev $spcer $nextt/gi;

print $mainlayout;

###

#print "<br><hr><br>";
#foreach (keys %ENV) {
#  print "$_  =  ".$ENV{$_}."<br>";
#}


##########################################################################

sub get_page_details { 
   my ($filename) = @_;
   my ($title, $begin_title, $end_title, $fsize, $rdata);

   $fsize = (-s "$filename");
   open (RVF, "$filename");
   read(RVF, $rdata, $fsize);
   close(RVF);

   if ($rdata =~ m/#set var="TITLE" value="/gi) {
      ### SSI Title

      $begin_title = pos($rdata);
      if ($rdata =~ m/\"/gi) {
         $end_title = pos($rdata) - 1 - $begin_title;
      }

   } else {
      ### HTML Title

      if ($rdata =~ m/<title>/gi) {
         $begin_title = pos($rdata);
      }
      if ($rdata =~ m/<\/title>/gi) {
         $end_title = pos($rdata) - 8 - $begin_title;
      }
   }
   $title = substr($rdata, $begin_title, $end_title);
   $title =~ s/<//g;
   $title =~ s/>//g;
   if (length($title) > 70) {$title = substr($title, 0, 70) . "...";} 
   return ($title);
}


##########################################################################

sub get_headline
{
   my ($data) = @_; shift;
   my ($word) = @_;
   my ($hl)   = "";
   my ($dots) = "...";
   my ($gap)  = $searchgap;
   my ($begin, $beg, $ldot, $rdot, $hl, $orig);

   $data =~ s/\n/ /g;
   $data =~ s/\r/ /g;

   if ($data =~ /$word/gi) { 

      $begin = pos($data);
      $beg = $begin - length($word);

      # left part

      $ldot = rindex($data, '.', $beg);
      if ($beg <= $gap) {
         $ldot = 0;
      } else {
         if ($ldot == -1) {
            $ldot = index($data, ' ', $beg - $gap);
            if ($ldot == -1) {
               $ldot = $beg - $gap;
            }
          } else {
            $ldot++;
            if ($beg - $ldot > $gap) {
               $ldot = index($data, ' ', $beg - $gap);
               if ($ldot == -1) {
                  $ldot = $beg - $gap;
                }
            }
         }
      }
      if ($ldot > $beg) { $ldot = $beg };

      # right part

      $rdot = index($data, '.', $begin);
      if ($rdot == -1) { 
         $rdot = $begin + $gap;
         $rdot = rindex($data, ' ', $begin + $gap);
         if ($rdot == -1) {
            $rdot = $begin + $gap;
         }
      } else {
         $rdot++;
         if ($rdot - $begin > $gap) {
            $rdot = rindex($data, ' ', $begin + $gap);
            if ($rdot == -1) {
               $rdot = $begin + $gap;
             }
         }
      };
     if ($rdot < $begin) { $rdot = $begin };

      # get a sentence
      $hl = substr($data, $ldot, $rdot-$ldot);
      $hl =~ s/^\s*(.*)\s*$/$1/;
      $hl .= $dots;
   }
   return $hl;
}


##########################################################################

sub mark_headline
{
   my ($data) = @_; shift;
   my ($word) = @_;

   $data =~ s/($word)/<b>$1<\/b>/gi;

   return $data;
}


##########################################################################

sub convert_rel2str
{
   my ($cno) = @_;
   if (length($cno) == 1) {$cno = "000000000000" . $cno;}
   if (length($cno) == 2) {$cno = "00000000000" . $cno;}
   if (length($cno) == 3) {$cno = "0000000000" . $cno;}
   if (length($cno) == 4) {$cno = "000000000" . $cno;}
   if (length($cno) == 5) {$cno = "00000000" . $cno;}
   if (length($cno) == 6) {$cno = "0000000" . $cno;}
   if (length($cno) == 7) {$cno = "000000" . $cno;}
   if (length($cno) == 8) {$cno = "00000" . $cno;}
   if (length($cno) == 9) {$cno = "0000" . $cno;}
   if (length($cno) == 10){$cno = "000" . $cno;}
   if (length($cno) == 11){$cno = "00" . $cno;}
   if (length($cno) == 12){$cno = "0" . $cno;}	
   return ($cno);
}


##########################################################################

sub get_env
{
   if ($ENV{'QUERY_STRING'} ne "") {
      $temp = $ENV{'QUERY_STRING'};
   } else {
      read(STDIN, $temp, $ENV{'CONTENT_LENGTH'});
   }
   @pairs=split(/&/,$temp);
    
   foreach $item(@pairs) {
      ($key,$content)=split (/=/,$item,2);
      $content=~tr/+/ /;
      $content=~ s/%(..)/pack("c",hex($1))/ge;
      $fields{$key}=$content;
   }
}


##########################################################################

# Replace real symbols with special HTML symbols

sub html_quote {
    $_ = shift;

    s/\&/&amp;/g;
    s/\</&lt;/g;
    s/\>/&gt;/g;

	return $_;
}


##########################################################################

# Strip HTML tags from a string

sub strip_html
{
   my ($data) = @_;

   $data =~ s/<[^\s>][^>]*>//g;
   $data =~ s/\t/ /g;

   return $data;
}


##########################################################################
