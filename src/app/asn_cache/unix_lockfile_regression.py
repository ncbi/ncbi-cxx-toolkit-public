import  time
import  subprocess

import  asn_cache_subprocess

def main():
    test_lockfile_name = "lockfile.test"
    
    #First, clean up any droppings from previous runs.
    args = [ "rm", "-f", test_lockfile_name ]
    asn_cache_subprocess.Run( args, "Droppings cleanup failed" )
        
    print "Test 1:  Create lockfile, test that C++ lockfile fails."
    args = [ "lockfile", "-s0", "-r0", test_lockfile_name ]
    asn_cache_subprocess.Run( args, "First test lockfile failed" )
    
    args = [ "test_unix_lockfile", "-sleep", "0", "-name", test_lockfile_name ]
    asn_cache_subprocess.Run( args, "C++ test should return 1, but failed", False, 1 )

    args = [ "rm", "-f", test_lockfile_name ]
    asn_cache_subprocess.Run( args, "Deletion of first test lockfile failed" )
    
    print "Test 2:  Create C++ lockfile, test that system lockfile fails."
    args = [ "test_unix_lockfile", "-postwait", "2", "-name", test_lockfile_name ]
    cpp_lockfile_proc = subprocess.Popen( args )

    time.sleep(0.1)
    args = [ "lockfile", "-s0", "-r0", test_lockfile_name ]
    asn_cache_subprocess.RunExpectingFailure( args, "Second test lockfile failed to fail" )
    
    return_code = cpp_lockfile_proc.wait()
    if return_code != 0:
        print   "Second C++ lockfile failed with return code ", return_code
    
    print "Test 3:  Create lockfile, start C++ lockfile and delete system lockfile."
    args = [ "lockfile", "-s0", "-r0", test_lockfile_name ]
    asn_cache_subprocess.Run( args, "Third test lockfile failed" )
    
    args = [ "test_unix_lockfile", "-sleep", "1", "-name", test_lockfile_name ]
    asn_cache_subprocess.Run( args, "C++ test failed", False, 1 )

    args = [ "rm", "-f", test_lockfile_name ]
    asn_cache_subprocess.Run( args, "Deletion of third test lockfile failed" )
    
    print   "Test completed successfully!"
 
 
if __name__ == "__main__":
    main()
