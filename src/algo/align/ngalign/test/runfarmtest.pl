#!/usr/bin/perl -w
use Shell qw(rm cp);
use File::Basename;


my $testdir = "/panfs/pan1/genome_maint/work/uberalign/test/";

my $builddir = "../../../../../GCC401-ReleaseMT64/build/algo/align/ngalign/";

chdir($builddir);
system("make");
chdir("test");
system("make");
cp("./ngalign_test  ".$testdir);


my $command = MakeFarmJob($testdir, $testdir, "test", $testdir."/ngalign_test");

print $command."\n";
system($command);


sub MakeFarmJob
{
    my $logdir = shift;
    my $errdir = shift;
    my $outprefix = shift;
    my $command =  shift;

    my $log = "$logdir/$outprefix.log";
    my $err = "$errdir/$outprefix.err";

    my $timelimit = 72 * (60 * 60);

    my $priority = "gpipe"; #Nathans doesnt have permissions for gpipe
    my $command_cf = "qsub -P \"$priority\" -j n -e $err -o $log -m e ".
                     "-v SGE_NOMAIL -v SGE_SUMMARY=\"stderr\" -b y -l ".
                     "h_rt=$timelimit,h_vmem=6G,mem_free=4G \"$command\"";
    return $command_cf;
#            my $log = "$logdir/$outprefix.log";
#            my $err = "$errdir/$outprefix.err";
#            my $priority = "unified"; #Nathans doesnt have permissions for gpipe
#            # my $command_cf =  qq{bsub -J $library.$bname -oo $log -eo $err -q "genome-pipeline" -R "rusage[mem=4096]" "$command_loc"};
#            my $command_cf =  qq{qsub -P "$priority" -j n -e $err -o $log -m e -v SGE_NOMAIL -v SGE_SUMMARY="stderr" -b y -l h_rt=$timelimit,h_vmem=6G,mem_free=4G "$command_loc"};
}
