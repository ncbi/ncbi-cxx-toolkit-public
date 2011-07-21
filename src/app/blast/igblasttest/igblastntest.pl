#!/usr/bin/perl -w

use strict;



my $new_program = "./igblastn";
my $old_program = "/export/home/jianye/igblast/trunk/c++/src/app/blast/igblastn";
my $testcasefile = "igblastntestcase";
my $base_parameters = " -germline_db_V Human_gl_V -germline_db_J Human_gl_J -germline_db_D Human_gl_D -query $testcasefile -show_translation -outfmt "; 
my @format = ("3", "4", "6", "7");


system ("rm *.out");
system ("rm *.diff");

foreach my $fmt (@format){
  my $parameters = $base_parameters." ".$fmt;
  print ("$parameters\n");
  print ("old\n");
  
  system("time $old_program $parameters > $testcasefile.$fmt.old.out");
  print ("new\n");
  system("time $new_program $parameters > $testcasefile.$fmt.new.out");
  system("diff  $testcasefile.$fmt.old.out $testcasefile.$fmt.new.out > $testcasefile.$fmt.diff");
}




#mouse
$testcasefile = "igblastntestcase.mouse";
$base_parameters = " -germline_db_V Mouse_gl_V -germline_db_J Mouse_gl_J -germline_db_D Mouse_gl_D -query $testcasefile -show_translation -origin Mouse -outfmt "; 



foreach my $fmt (@format){
  my $parameters = $base_parameters." ".$fmt;
  print ("$parameters\n");
  print ("old\n");
  
  system("time $old_program $parameters > $testcasefile.$fmt.old.out");
  print ("new\n");
  system("time $new_program $parameters > $testcasefile.$fmt.new.out");
  system("diff  $testcasefile.$fmt.old.out $testcasefile.$fmt.new.out > $testcasefile.$fmt.diff");
}

#test seqid list
$testcasefile = "igblastntestcase.idlist";
$base_parameters = " -germline_db_V Human_gl_V -germline_db_J Human_gl_J -germline_db_D Human_gl_D -query $testcasefile -show_translation  -germline_db_seqidlist Human_gl_V.n.functional.seqid -outfmt "; 



foreach my $fmt (@format){
  my $parameters = $base_parameters." ".$fmt;
  print ("$parameters\n");
  print ("old\n");
  
  system("time $old_program $parameters > $testcasefile.$fmt.old.out");
  print ("new\n");
  system("time $new_program $parameters > $testcasefile.$fmt.new.out");
  system("diff  $testcasefile.$fmt.old.out $testcasefile.$fmt.new.out > $testcasefile.$fmt.diff");
}


#test v focus nt db 
$testcasefile = "igblastntestcase.vfocusnt";
$base_parameters = "  -germline_db_V Human_gl_V -germline_db_J Human_gl_J -germline_db_D Human_gl_D -query $testcasefile -show_translation -db nt -remote -focus_on_V_segment -num_alignments 60  -outfmt "; 



foreach my $fmt (@format){
  my $parameters = $base_parameters." ".$fmt;
  print ("$parameters\n");
  print ("old\n");
  
  system("time $old_program $parameters > $testcasefile.$fmt.old.out");
  print ("new\n");
  system("time $new_program $parameters > $testcasefile.$fmt.new.out");
  system("diff  $testcasefile.$fmt.old.out $testcasefile.$fmt.new.out > $testcasefile.$fmt.diff");
}



#test v focus igSeqNT db 
$testcasefile = "igblastntestcase.vfocusigseqnt";
$base_parameters = "   -germline_db_V Human_gl_V -germline_db_J Human_gl_J -germline_db_D Human_gl_D -query $testcasefile -show_translation -db igSeqNt -num_alignments 60 -num_threads 4  -outfmt "; 



foreach my $fmt (@format){
  my $parameters = $base_parameters." ".$fmt;
  print ("$parameters\n");
  print ("old\n");
  
  system("time $old_program $parameters > $testcasefile.$fmt.old.out");
  print ("new\n");
  system("time $new_program $parameters > $testcasefile.$fmt.new.out");
  system("diff  $testcasefile.$fmt.old.out $testcasefile.$fmt.new.out > $testcasefile.$fmt.diff");
}

#test imgt domain
$testcasefile = "igblastntestcase.imgt";
$base_parameters = " -germline_db_V Human_gl_V -germline_db_J Human_gl_J -germline_db_D Human_gl_D -query igblastntestcase -show_translation  -domain_system imgt -outfmt "; 



foreach my $fmt (@format){
  my $parameters = $base_parameters." ".$fmt;
  print ("$parameters\n");
  print ("old\n");
  
  system("time $old_program $parameters > $testcasefile.$fmt.old.out");
  print ("new\n");
  system("time $new_program $parameters > $testcasefile.$fmt.new.out");
  system("diff  $testcasefile.$fmt.old.out $testcasefile.$fmt.new.out > $testcasefile.$fmt.diff");
}

#custom db
$testcasefile = "igblastntestcase.customdb";
$base_parameters = "  -germline_db_V UNSWIgVRepertoire_fasta.txt -germline_db_J UNSWIgJRepertoire_fasta.txt -germline_db_D UNSWIgDRepertoire_fasta.txt -show_translation -query $testcasefile  -outfmt  "; 
foreach my $fmt (@format){
  my $parameters = $base_parameters." ".$fmt;
  print ("$parameters\n");
  print ("old\n");
  
  system("time $old_program $parameters > $testcasefile.$fmt.old.out");
  print ("new\n");
  system("time $new_program $parameters > $testcasefile.$fmt.new.out");
  system("diff  $testcasefile.$fmt.old.out $testcasefile.$fmt.new.out > $testcasefile.$fmt.diff");
}
