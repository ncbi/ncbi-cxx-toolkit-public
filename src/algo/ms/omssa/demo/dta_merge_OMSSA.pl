#!/usr/bin/perl -w
use Sys::Hostname;
$host=hostname();
print  "Host Machine name => $host\n";  
print "Operating System => $^O\n";
print "Perl Version", "=> ", $],"\n";
print "perl exe", "=> ", $0,"\n\n";
#foreach $key (sort keys %ENV)
#{   print $key, '=', $ENV{$key}, "\n";}

	
package OMSSA;
#########################################################################################
#  											#
#	dta_merge2.pl: merging .dta files for OMSSA batching				#
#											#
#       $DATE: 07/15/2003								#
#	$AUTHOR: Ming Xu								#
#	$VERSION: 1.0.0									#
#       $usage: perl dta_merge2.pl -i <input path> -o <output path> -s <batching size>  #
#				   -n <output file nameROOT>				#
#               <output path>: an absolute path to a dir where t oread .dta files	#
# 	        <output path>: an absolute path to a dir where to wrtie concatnated     #
#			       dta file(s) for OMSSA batching processing                #	               
#	        <batching size>: max. No. of .dta files to be concatnated into a file   #
#	        <output file nameROOT> : concatnated file's name root			#
#	       										#
#	$NOTE: concatenating up to $batch_maxFiles *.dta files                          #
#              formatting:								#
#              $inFile => full name of an original dta file				#
#              $outFile => full name of batching output file				#
#	       $iteration => indexing of concatenated dta files in $outFile		#
#											#
#              <dta id=1 name=$inFile BatchName=$outFile>  	        		#
#	       ...									#
#              ... #original dta file content goes here 				# 
#	       ...   									#
#	       </dta>						                        #
#	             ...  empty line goes here  ...					#
#              <dta id=$iteration name=$inFile BatchName=$outFile> 		        #
#	       ...									#
#              ... #original dta file content goes here 				# 
#	       ...   									#
#	       </dta>						                        #
#	             ...  empty line goes here  ...					#
#              <dta id=1 name=$inFile BatchName=$outFile>  	        		#
#	       ...									#
#              ... #original dta file content goes here 				# 
#	       ...   									#
#	       </dta>						                        #
#	             ...  empty line goes here  ...					#
#              <dta id=up to <batching size>||100 name=$inFile BatchName=$outFile>      #
#	       ...									#
#              ... #original dta file content goes here 				# 
#	       ...   									#
#	       </dta>						                        ##     	       										#
#########################################################################################

use  strict;
use File::Glob qw(bsd_glob);
use Getopt::Std;
use vars qw ($opt_i $opt_o $opt_s $opt_n);
getopts('i:o:s:n:');
#print "input path => $opt_i\noutput path => $opt_o\nMax. No. of files to be concatnated => $opt_s\noutput file nameROOT => $opt_n\n";

  my $maxNo_Files = $opt_s ||= 2000;     # number of dta files to be concatnated into a batching file, default 10000


  my $batch_index=1;      		# indexing for batching file(s)
  my $iteration=0;        		# loop index for a batching file
  
  my $inputDir = $opt_i ||= "./";  	# directory to read dta files, default ./
  my $inputFilter ||="*";		# input file filtering
  my $inputPath="$inputDir"."$inputFilter";  # input file(dta files) full name

  my $outPutDir = $opt_o ||="./";   	# batching file dirtectory, default ./

  my $batch_out = $opt_n;
     $batch_out||="dtaBatch";    	 # batching file name, default dtaBatch + $$No_batchedFile + ".txt"      
  my $outputPath="$outPutDir"."$batch_out";  # output path
  

  my $inFile;				# input dta file name holder
  my $No_batchedFile;          	        # int

print "input dir => $inputDir\noutput dir => $outPutDir\n" ; 
print "Max. No. of files to be concatnated into a file => $maxNo_Files\noutput file nameROOT => $batch_out\n\n";

  my $outFile = "$outputPath"."$batch_index".".txt"; # output file full name
  open(FileOut,">$outFile") || die "cannot create output file"."$outFile";
                
foreach my $inFile (bsd_glob("$inputPath/*"))
  {
        if ($inFile =~ /\.dta$/i && -s $inFile)
	{
             
 	     if($iteration >= $maxNo_Files)
             {
                print "$outputPath"."$batch_index".".txt contains  $iteration dta files\n" ;
                $iteration = 0;
                $batch_index +=1;
                close FileOut;
                $outFile = "$outputPath"."$batch_index".".txt";
                open(FileOut,">$outFile") || die "cannot create output file"."$outFile";
	     }

	     $iteration +=1;
             open(FileIn,"<$inFile");	     
	     print FileOut "<dta id=\"$iteration\" name=\"$inFile\" batchFile=\"$outFile\">\n";
	     
	     # modify the code to allow this program take mascot format files as input 
	     # to generated merged dta file --- Wenyao Shi.
	     my $pepmass = 0;				#initialize the variable for PEPMASS in mascot format file header
	     my $charge = "";				#initialize the variable for CHARGE in mascot format file header
	     my $first_line_printed = 0;	#flag for printing the first line when convert from mascot format file
            while (<FileIn>) 
 	     	{
 	     		if ($_ =~ /^PEPMASS/){
 	     			$pepmass = (split(/=/,$_))[1];	# get pepmass from mascot formated header
 	     			chomp($pepmass);
 	     			$pepmass =~ s/^\s+//;         # remove space at beginning of line
    				$pepmass =~ s/\s+$//;         # remove space at end of line
 	     		}
 	     		if ($_ =~ /^CHARGE/){
 	     			$charge = (split(/=/,$_))[1];	# get charge from mascot formated header
 	     			chomp($charge); 			
 	     			$charge =~ s/^\s+//;         # remove space at beginning of line
    				$charge =~ s/\s+$//;         # remove space at end of line	
 	     		}
 	     		
 	     		if ($pepmass != 0 && $charge ne "" && $first_line_printed != 1){
 	     			$charge = (split(/\+|\-/, $charge))[0];		# remove the "+" or "-" sign after the charge number
 	     			print FileOut "$pepmass $charge\n";			# print "mass/charge" pair as the first line to make it a regular dta format
 	     			$first_line_printed = 1;
 	     		}
  		      	print FileOut $_ if ($_ =~ /^\d+/);
             }
	     print FileOut "</dta>\n\n";
             close FileIn;
	      
        }
        
  }

  if( $iteration != 0)
  {   print "$outputPath"."$batch_index".".txt contains $iteration dta files\n" ; }
  else
  {    print "no dta file(s) found in the input directory \'$inputDir\'\n" ;}  
 
  close FileOut;  
 
	
 
