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

if ($ENV{'SCRIPT_FILENAME'} =~ m"/private/htdocs") {
    $webbase    = "http://intranet.ncbi.nlm.nih.gov/ieb/ToolBox/CPP_DOC";
    $searchbase = "/web/private/htdocs/intranet/ieb/ToolBox/CPP_DOC";
} else {
    $webbase    = "http://www.ncbi.nlm.nih.gov/IEB/ToolBox/CPP_DOC";
    $searchbase = "/web/public/htdocs/IEB/ToolBox/CPP_DOC";
}

@searchdirs = (
               "",
##               "docxx",
               "internal",
               "internal/idx",
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

## What is the extensions of the files to be searched
## If you want to use more than one extension seperate them via comma
@searchext = (".html", ".htm");

## How many items to list per page
$listperpage = "20";


##########################################################################
# Don't change anything below this line unless you know what you are doing.
##########################################################################

&get_env;

print "Content-type: text/html\n\n";

$fsize = (-s "./main.html");
open (RVF, "./main.html");
read(RVF, $mainlayout, $fsize);
close(RVF);

##$mainlayout = readpipe("$ssifilter main.html");

$fsize = (-s "./listings.html");
open (RVF, "./listings.html");
read(RVF, $listings, $fsize);
close(RVF);

if ($fields{'keywords'} eq "") {$fields{'keywords'} = "No-Keywords-Specified";}


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
             if ($ext eq substr($fn, length($fn) - length($ext), length($ext))) {
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
       print "Error - $item does not exist.";
    }
    
    $itec++;
    $tmpc = push(@allfiles, @sfiles);
}

### PREPARE KEYWORDS

if ($fields{'keywords'} =~ / /) {
   @keywords = split (/ /,$fields{'keywords'});
} else {
   $keywords[0] = $fields{'keywords'};
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
        
   ### GET RELEVANCE
    
   foreach $kw (@keywords) {

     $lrelevance = 0;
      # Filename matching
      if ($url =~ /$kw/i) { 
         $lrelevance++;
      }

      # File data matching
      if ($rdata =~ /$kw/i) {
         @filelines = split(/\n/,$rdata);
         foreach $oln (@filelines) {
            if ($oln =~ /$kw/i) { 
               $lrelevance++;
            }
            if (($oln =~ /$fields{'keywords'}/i) and 
                ($fields{'keywords'} =~ " ")) {
                $lrelevance = $lrelevance + 15;
            }             
         }
       } ### FOUND MATCH

       # Keyword not found -- AND search method: relevance -> 0
       if ($lrelevance == 0) {
          $mrelevance = 0;
          goto ("donesearch")[0];
       }
       $mrelevance = $mrelevance + $lrelevance;

   } ### KEYWORD LOOP

donesearch:
   if ($mrelevance > $highsr) {$highsr = $mrelevance;}
   if ($mrelevance > 0) {
      ($ptitle) = &get_page_details($item);
       ###print "FOUND MATCH IN $item - Relevance = $mrelevance -> $ptitle <br><b></b>";
       $mrelevance = &convertnr($mrelevance);
       $sresults[$srcntr] = $mrelevance . "\t" . 
       $item . "\t" . $url . "\t" . $ptitle;
       $srcntr++;
   }
	
} ### FILE LOOP

@sresults = sort(@sresults);
$allresults = push(@sresults);
$tmr = $allresults;

foreach $item (@sresults) {
   $tmr = $tmr - 1;
   $sresults2[$tmr] = $item;
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
$mainlayout =~ s/!!Keywords!!/$fields{'keywords'}/gi;

foreach $item (@sresults2) {
   if (($ippc > $fields{'st'}) and ($ippc <= $fields{'nd'})) {
        
		($relevance, $sp, $url, $ptitle) =split(/\t/,$item);
		if ($ptitle eq "") {$ptitle = "Untitled";}
   
		$relevance = int($relevance / $highsr * 100);
		
		$wls = $listings;
		$wls =~ s/!!Index!!/$ippc/gi;
		$wls =~ s/!!Title!!/$ptitle/gi;
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
   local ($filename) = @_;
   local ($title, $begin_title, $end_title, $fsize, $rdata);

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

sub convertnr
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

