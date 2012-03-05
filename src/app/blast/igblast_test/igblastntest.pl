#!/usr/bin/perl -w

use strict;



my $new_program = "./igblastn";
my $old_program = "/export/home/jianye/igblast/trunk/c++/src/app/blast/igblastn";
my $testcasefile = "igblastntestcase";
my $base_parameters = " -germline_db_V IG_DB/human_gl_V -germline_db_J IG_DB/human_gl_J -germline_db_D IG_DB/human_gl_D -query $testcasefile -show_translation  -auxilary_data optional_file/human_gl.aux -outfmt "; 
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

#test gi igseqnt
$testcasefile = "igblastntestcase.giigseqnt";
$base_parameters = " -germline_db_V IG_DB/human_gl_V -germline_db_J IG_DB/human_gl_J -germline_db_D IG_DB/human_gl_D -query $testcasefile  -auxilary_data optional_file/human_gl.aux -show_translation -db IG_DB/igseq -num_threads 4 -outfmt ";



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
$testcasefile = "igblastntestcase.gi";
$base_parameters = "  -germline_db_V IG_DB/human_gl_V -germline_db_J IG_DB/human_gl_J -germline_db_D IG_DB/human_gl_D -query $testcasefile -show_translation -auxilary_data optional_file/human_gl.aux -num_alignments 60  -outfmt "; 



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
$base_parameters = " -germline_db_V IG_DB/mouse_gl_V -germline_db_J IG_DB/mouse_gl_J -germline_db_D IG_DB/mouse_gl_D -query $testcasefile -auxilary_data optional_file/mouse_gl.aux -show_translation -organism mouse -outfmt "; 



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
$base_parameters = " -germline_db_V IG_DB/human_gl_V -germline_db_J IG_DB/human_gl_J -germline_db_D IG_DB/human_gl_D -query $testcasefile -auxilary_data optional_file/human_gl.aux -show_translation  -germline_db_V_seqidlist seqid.v  -germline_db_D_seqidlist seqid.dlist -germline_db_J_seqidlist seqid.j -outfmt "; 



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
$base_parameters = "  -germline_db_V IG_DB/human_gl_V -germline_db_J IG_DB/human_gl_J -germline_db_D IG_DB/human_gl_D -query $testcasefile -auxilary_data optional_file/human_gl.aux -show_translation -db nt -remote -focus_on_V_segment -num_alignments 60  -outfmt "; 



foreach my $fmt (@format){
  my $parameters = $base_parameters." ".$fmt;
  print ("$parameters\n");
  print ("old\n");
  
  system("time $old_program $parameters > $testcasefile.$fmt.old.out");
  print ("new\n");
  system("time $new_program $parameters > $testcasefile.$fmt.new.out");
  system("diff  $testcasefile.$fmt.old.out $testcasefile.$fmt.new.out > $testcasefile.$fmt.diff");
}

#test nt
$testcasefile = "igblastntestcase.nt";
$base_parameters = "  -germline_db_V IG_DB/human_gl_V -germline_db_J IG_DB/human_gl_J -germline_db_D IG_DB/human_gl_D -query $testcasefile -auxilary_data optional_file/human_gl.aux -show_translation -db nt -remote  -num_alignments 60  -outfmt "; 



foreach my $fmt (@format){
  my $parameters = $base_parameters." ".$fmt;
  print ("$parameters\n");
  print ("old\n");
  
  system("time $old_program $parameters > $testcasefile.$fmt.old.out");
  print ("new\n");
  system("time $new_program $parameters > $testcasefile.$fmt.new.out");
  system("diff  $testcasefile.$fmt.old.out $testcasefile.$fmt.new.out > $testcasefile.$fmt.diff");
}
#test gi nt
$testcasefile = "igblastntestcase.gint";
$base_parameters = "  -germline_db_V IG_DB/human_gl_V -germline_db_J IG_DB/human_gl_J -germline_db_D IG_DB/human_gl_D -query $testcasefile  -auxilary_data optional_file/mouse_gl.aux -show_translation -db nt -remote  -num_alignments 60  -outfmt "; 



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
$base_parameters = "   -germline_db_V IG_DB/human_gl_V -germline_db_J IG_DB/human_gl_J -germline_db_D IG_DB/human_gl_D -query $testcasefile -auxilary_data optional_file/human_gl.aux -show_translation -db IG_DB/igseq -focus_on_V_segment -num_alignments 60 -num_threads 4  -outfmt "; 



foreach my $fmt (@format){
  my $parameters = $base_parameters." ".$fmt;
  print ("$parameters\n");
  print ("old\n");
  
  system("time $old_program $parameters > $testcasefile.$fmt.old.out");
  print ("new\n");
  system("time $new_program $parameters > $testcasefile.$fmt.new.out");
  system("diff  $testcasefile.$fmt.old.out $testcasefile.$fmt.new.out > $testcasefile.$fmt.diff");
}

#test igSeqNT db 
$testcasefile = "igblastntestcase.igseqnt";
$base_parameters = "   -germline_db_V IG_DB/human_gl_V -germline_db_J IG_DB/human_gl_J -germline_db_D IG_DB/human_gl_D -query $testcasefile  -auxilary_data optional_file/human_gl.aux -show_translation -db IG_DB/igseq -num_alignments 60 -num_threads 4  -outfmt "; 



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
$base_parameters = " -germline_db_V IG_DB/human_gl_V -germline_db_J IG_DB/human_gl_J -germline_db_D IG_DB/human_gl_D -query $testcasefile -auxilary_data optional_file/human_gl.aux -show_translation  -domain_system imgt -outfmt "; 



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
$base_parameters = "  -germline_db_V IG_DB/UNSWIgVRepertoire_fasta.txt -germline_db_J IG_DB/UNSWIgJRepertoire_fasta.txt -germline_db_D IG_DB/UNSWIgDRepertoire_fasta.txt  -auxilary_data optional_file/human_gl.aux.testonly -show_translation -query $testcasefile  -outfmt  "; 
foreach my $fmt (@format){
  my $parameters = $base_parameters." ".$fmt;
  print ("$parameters\n");
  print ("old\n");
  
  system("time $old_program $parameters > $testcasefile.$fmt.old.out");
  print ("new\n");
  system("time $new_program $parameters > $testcasefile.$fmt.new.out");
  system("diff  $testcasefile.$fmt.old.out $testcasefile.$fmt.new.out > $testcasefile.$fmt.diff");
}
