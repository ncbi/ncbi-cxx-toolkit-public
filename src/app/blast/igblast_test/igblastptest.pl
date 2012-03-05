#!/usr/bin/perl -w

use strict;



my $new_program = "./igblastp";
my $old_program = "/export/home/jianye/igblast/trunk/c++/src/app/blast/igblastp";
my $testcasefile = "igblastptestcase";
my $base_parameters = " -germline_db_V IG_DB/human_gl_V  -query $testcasefile  -outfmt "; 
my @format = ("3", "4", "7");


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

#test imgt domain
$testcasefile = "igblastptestcase.imgt";
$base_parameters = " -germline_db_V IG_DB/human_gl_V  -query $testcasefile  -domain_system imgt -outfmt "; 



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
$testcasefile = "igblastptestcase.mouse";
$base_parameters = " -germline_db_V IG_DB/mouse_gl_V  -query $testcasefile -organism mouse -outfmt "; 



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
$testcasefile = "igblastptestcase.customdb";
$base_parameters = "  -germline_db_V IG_DB/UNSWIgVRepertoire_fasta.txt -query $testcasefile  -outfmt  "; 
foreach my $fmt (@format){
  my $parameters = $base_parameters." ".$fmt;
  print ("$parameters\n");
  print ("old\n");
  
  system("time $old_program $parameters > $testcasefile.$fmt.old.out");
  print ("new\n");
  system("time $new_program $parameters > $testcasefile.$fmt.new.out");
  system("diff  $testcasefile.$fmt.old.out $testcasefile.$fmt.new.out > $testcasefile.$fmt.diff");
}


#test gi
$testcasefile = "igblastptestcase.gi";
$base_parameters = "  -germline_db_V IG_DB/human_gl_V  -query $testcasefile  -num_alignments 60  -outfmt "; 



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
$testcasefile = "igblastptestcase.idlist";
$base_parameters = " -germline_db_V IG_DB/human_gl_V  -query $testcasefile  -germline_db_V_seqidlist seqid.v_prot  -outfmt "; 



foreach my $fmt (@format){
  my $parameters = $base_parameters." ".$fmt;
  print ("$parameters\n");
  print ("old\n");
  
  system("time $old_program $parameters > $testcasefile.$fmt.old.out");
  print ("new\n");
  system("time $new_program $parameters > $testcasefile.$fmt.new.out");
  system("diff  $testcasefile.$fmt.old.out $testcasefile.$fmt.new.out > $testcasefile.$fmt.diff");
}



#test gi igseqprot
$testcasefile = "igblastptestcase.giigseqprot";
$base_parameters = " -germline_db_V IG_DB/human_gl_V -query $testcasefile  -db IG_DB/igseq -num_threads 4 -outfmt ";



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
$testcasefile = "igblastptestcase.vfocusnr";
$base_parameters = "  -germline_db_V IG_DB/human_gl_V -query $testcasefile  -db nr -remote -focus_on_V_segment -num_alignments 60  -outfmt "; 



foreach my $fmt (@format){
  my $parameters = $base_parameters." ".$fmt;
  print ("$parameters\n");
  print ("old\n");
  
  system("time $old_program $parameters > $testcasefile.$fmt.old.out");
  print ("new\n");
  system("time $new_program $parameters > $testcasefile.$fmt.new.out");
  system("diff  $testcasefile.$fmt.old.out $testcasefile.$fmt.new.out > $testcasefile.$fmt.diff");
}

#test gi nr
$testcasefile = "igblastptestcase.ginr";
$base_parameters = "  -germline_db_V IG_DB/human_gl_V  -query $testcasefile -db nr -remote  -num_alignments 60  -outfmt "; 



foreach my $fmt (@format){
  my $parameters = $base_parameters." ".$fmt;
  print ("$parameters\n");
  print ("old\n");
  
  system("time $old_program $parameters > $testcasefile.$fmt.old.out");
  print ("new\n");
  system("time $new_program $parameters > $testcasefile.$fmt.new.out");
  system("diff  $testcasefile.$fmt.old.out $testcasefile.$fmt.new.out > $testcasefile.$fmt.diff");
}
