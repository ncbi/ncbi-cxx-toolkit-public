This folder consists of test cases for BasicCleanup.

========================================
RUNNING TESTS
========================================

Besides running tests via the unit tests like unit_test_basic_cleanup,
you can also run tests manually.  Simply run "+run_tests [file or dir name]",
and it will output differences between output and expected answers.
"+run_tests" has other options, too, the most useful of which is "-ch=", which 
can be "-ch=T", "-ch=F", or "-ch=X" to mean, respectively, "error if there are 
no changes to the test file (the default, since test files are expected to
have a change)", "error if any changes" and "don't care".
Note that "-ch=" changes means changes to the input file, NOT changes
between cleanasn and the new C++ cleanup. 

In the future, "+run_tests" should be changed to deal with and create
only compressed files, though it hasn't been done yet.

========================================
FOLDER HIERARCHY
========================================

The hierarchy of folders mostly just exists for human organizational
purposes and you can feel free to rearrange it if it makes more logical sense 
to you, except for these special folders:

cb_cleanup:
- For these files, some have no change, so tests run in this folder should
  be run with the -ch=X option (explained elsewhere).

highest-level:
- This has test cases for files which are not Seq-entry (e.g. Bioseq, etc.). 
  Generally, these don't work with the old cleanasn, but you might be able
  to run test_basic_cleanup manually against them.

oversized:
- Folder is basically normal except that the test cases are very big
  and you might want to skip them if you are doing a quicker test.

========================================
FILE EXTENSIONS
========================================

Input types:

.raw_test: 
- A direct .asn file that can be used directly as input to cleanasn or
  watever.

.template:
- Run "+easy_auto_replace [filename]" to expand a template file into many test cases (sometimes as much as 1000), which have the extension ".test".  The format of .template files is described in +easy_auto_replace.  Each .test file can be used directly as input, like the .raw_test files, and it's generally recommended to delete the .test files when you're done.  Better, do "+easy_auto_replace -1 [filename]" to get the .template_expanded file from the .template file.

.test:
- See .template.  This is really only used for manual testing, or via 
  +run_tests, so you shouldn't normally see these around.

.template_expanded:
- Run "+easy_auto_replace -1 [filename]" (note the "-1") to expand a template file into ONE big .template_expanded which contains all the outputs that would've been in separate files if you had not used the "-1" arg.


Expected output types:

.cleanasns_output:
- This holds the output that cleanasn gives of the .raw_test or .template_expanded file with the same base name.

.answer
- This holds the expected output of the .raw_test or .template_expanded file with the same base name.  It's just like .cleanasns_output except that the extension of ".answer" tells you that the correct answer differs from cleanasn's output.

.sqn
- These are input files which are given the extension ".sqn" so they are not
  picked up by the automatic tests.  You can try using them manually, but
  if they have this name it's probably because they are somehow unusable.
  For example, they may be a "Seq-submit" instead of a "Seq-entry".

========================================
SCRIPTS
========================================

+run_tests
- Described above.

+search_tests
- This lets you egrep input files ( Look at the source, it's a very 
  short file ).

+create_template_expanded_files_and_cleanasns_output
- Occasionally, you will want to refresh the cleanasns_output files
  to reflect the fact that cleanasn might change from time to time.
  This script does that.
  If a .template file needs to be expanded, it also does that.

+easy_auto_replace
- Converts .template files into .template_expanded files or multiple .test 
  files.  Users might consider copying this script and using it for their
  own purposes, since it's good in general for generating a large
  number of test cases that differ only a little.
