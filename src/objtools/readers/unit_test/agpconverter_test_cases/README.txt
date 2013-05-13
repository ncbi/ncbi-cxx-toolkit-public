In this directory (and/or its subdirectories) all files besides this README.txt
will have names consisting of two parts. Up to the first period is the name of the test and after the first period is the suffix.  All the files with the same test name are considered part of the same test.  These are the suffixes:

REQUIRED:
* template.asn
  * This holds a text ASN.1 Bioseq representing the template
* agp
  * This holds the AGP file that is used as input
* expected_bioseq_set.asn
  * This is the text ASN.1 Bioseq-set that is expected as output.

OPTIONAL:

* submit_block.asn
  * this holds the submit-block, which is actually ignored because
    we're in the Bioseq-set mode.
* flags.txt
  * Each line is the text name of a flag to set (e.g. fOutputFlags_Fuzz100)
* expected_errors.txt
  * Each line is the error (e.g. eError_OutputDirNotFoundOrNotADir), then
    a tab, then the error message that's expected
