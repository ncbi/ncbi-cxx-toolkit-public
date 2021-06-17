To add additional test cases to this unit test:
--------------------------------------------------------------------------------
(1) Decide on a test name that has not been taken by another test, f.e. "dummy".
(2) Provide a ### input file for the test, name it dummy.###, and put it in this
    directory.
(3) Provide an asn file that reflects the expected conversion of dummy.###. Name 
    it dummy.asn and also put it in this directory.
(4) Provide an error listing (potentially empty) that reflects the output of an
    CErrorLogger (unit_test/error_logger.hpp) object. Name it dummy.errors and
    also put it into this directory.
    
Alternatively:
(3) Run "unit_test_bedreader -test-cases-dir [[here]] -update-case dummy"
    and unit_test_bedreader will generate the corresponding dummy.asn and 
	dummy.error files based on the ###reader code the unit test was compiled 
	against.
	You may want to desk check the aute generated files before committing to
	them.

That's it - the unit test will scan this directory each time it runs, and thus
find and execute the new test automatically.
