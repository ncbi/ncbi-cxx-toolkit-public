import  subprocess
import  sys

def Run( args, error_message, debug = False, expected_return = 0 ):
    if debug:
        print " ".join( args )
    return_code = subprocess.call( args )
    if debug:
        print "Returned ", return_code
    
    if return_code != expected_return:
        ReportFailedSubprocess( return_code, args, error_message )


def RunExpectingFailure( args, error_message, debug = False ):
    if debug:
        print " ".join( args )
    return_code = subprocess.call( args )
    if debug:
        print "Returned ", return_code
    
    if return_code == 0:
        ReportFailedSubprocess( return_code, args, error_message )


def ReportFailedSubprocess( return_code, args, error_message ):
    error_string = "%s with return code: %d.  Call arguments were %s" \
    % ( error_message, return_code, " ".join( args ) )
    raise Exception( error_string )
