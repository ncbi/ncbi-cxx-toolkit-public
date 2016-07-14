import  os
import  sys
import  tempfile
import  shutil
import  filecmp
from optparse import OptionParser

import  asn_cache_subprocess


def main():
    usage_string = "usage: %prog [options] <GI list File>"
    cl_option_parser = OptionParser( usage=usage_string )
    cl_option_parser.add_option( "-d", "--leave-droppings", dest="leave_droppings",
                                    action="store_true",
                                    help="Do not remove the generated files on success",
                                    metavar="FILE", default=False)
    cl_option_parser.add_option( "-s", "--scratch-directory", dest="scratch_dir", metavar="DIR",
                                    help="Directory to hold the temporary files for the test." )
    cl_option_parser.add_option( "-m", "--missing-gis", dest = "missing_gis", metavar="FILE",
                                    help="File with a list of GIs that will not be cached, but read \
    from ID during subcache creation." )
    cl_option_parser.add_option( "-l", "--lds", dest = "lds", metavar="FILE",
                                    help="LDS2 used for fetching missing gis instead of ID." )
    cl_option_parser.add_option( "-i", "--reference-index", dest = "reference_index", metavar="FILE",
                                    help="Expected index for subcache containing fetched gis." )
    cl_option_parser.add_option( "-e", "--extract", dest = "extract", default=False,
                                    action="store_true",
                                    help="Test options extract-product and extract-delta." )
    cl_option_parser.add_option( "-v", "--verbose", dest="verbose", default=False,
                                    action="store_true",
                                    help="Output debugging information." )
    
    (options, remaining_args) = cl_option_parser.parse_args()
    
    if len( remaining_args ) == 0:
        cl_option_parser.error( "Please supply a file with a list of at least five GIs." )
    elif len( remaining_args ) > 1:
        cl_option_parser.error( "Too many arguments were supplied." )
    
    if options.scratch_dir and not ( os.path.exists( options.scratch_dir ) ):
        os.makedirs( options.scratch_dir )
        scratch_dir_was_created = True
    else:
        scratch_dir_was_created = False

    # Switch to the test data directory, but make sure the scratch
    # directory isn't affected by the switch.
    test_dir = os.path.join(os.environ['NCBI_TEST_DATA'], 'asn_cache')
    tools_dir =  os.path.join(os.environ['NCBI'], 'bin')
    os.environ['PATH'] = os.pathsep.join((os.environ['CFG_BIN'], tools_dir, os.environ['PATH']))
    options.scratch_dir = os.path.abspath(options.scratch_dir)
    os.chdir(test_dir)
    
    gi_file_name = remaining_args[0]
    gi_file = open( gi_file_name, 'r' )
    gi_list = gi_file.readlines()
    gi_file.close()
    if len( gi_list ) < 5:
        print "You need at least five GIs in your list."
        print "three for three subcaches and two for two updates."
        exit(2)
    
    gi_file = open( gi_file_name, 'r' )
    orig_gi_list = gi_file.readlines()
    gi_file.close()

    unit_size = len( gi_list ) / 5

    if options.missing_gis and not options.extract:
        missing_gi_file = open( options.missing_gis, 'r' )
        gi_list += missing_gi_file.readlines()
        missing_gi_file.close()
    
    asn_file1, asn_file1_name = tempfile.mkstemp( ".asn", "tmp", options.scratch_dir )
    os.close( asn_file1 )
    asn_file2, asn_file2_name = tempfile.mkstemp( ".asn", "tmp", options.scratch_dir )
    os.close( asn_file2 )
    asn_file3, asn_file3_name = tempfile.mkstemp( ".asn", "tmp", options.scratch_dir )
    os.close( asn_file3 )
    update1_asn_file, update1_asn_file_name = tempfile.mkstemp( ".asn", "tmp", options.scratch_dir )
    os.close( update1_asn_file )
    update2_asn_file, update2_asn_file_name = tempfile.mkstemp( ".asn", "tmp", options.scratch_dir )
    os.close( update2_asn_file )
    complete_asn_file, complete_asn_file_name = tempfile.mkstemp( ".asn", "tmp", options.scratch_dir )
    os.close( complete_asn_file )
    fetched_cache_index_file, fetched_cache_index_file_name = tempfile.mkstemp( ".idx", "tmp", options.scratch_dir )
    os.close( fetched_cache_index_file )
    
    id_dump_dir_path = tempfile.mkdtemp( ".id_dump", "tmp", options.scratch_dir )
    cache_dir_path = tempfile.mkdtemp( ".cache", "tmp", options.scratch_dir )
    subcache_dir_path = tempfile.mkdtemp( ".subcache", "tmp", options.scratch_dir )
    fetched_subcache_dir_path = tempfile.mkdtemp( ".subcache.fetched", "tmp", options.scratch_dir )
    
    target_asn_file, target_asn_file_name = tempfile.mkstemp( ".asn", "tmp", options.scratch_dir )
    os.close( target_asn_file )
    
    target2_asn_file, target2_asn_file_name = tempfile.mkstemp( ".asn", "tmp", options.scratch_dir )
    os.close( target2_asn_file )
    
    log_file, log_file_name = tempfile.mkstemp( ".log", "tmp", options.scratch_dir )
    os.close( log_file )
    
    subcache_gi_file_name = gi_file_name
    if options.missing_gis and not options.extract:
        subcache_gi_fd, subcache_gi_file_name = tempfile.mkstemp( ".gis", "tmp", options.scratch_dir )
        subcache_gi_file = os.fdopen( subcache_gi_fd, 'w' )
        subcache_gi_file.writelines( gi_list );
        subcache_gi_file.close()

    second_update_gis = orig_gi_list[4*unit_size : len(orig_gi_list)]
    second_update_gi_fd, second_update_gi_file_name = tempfile.mkstemp( ".gis", "tmp", options.scratch_dir )
    second_update_file = os.fdopen( second_update_gi_fd, 'w' )
    second_update_file.writelines( second_update_gis )
    second_update_file.close()
    
    first_update_gis = orig_gi_list[3*unit_size : 4*unit_size]
    first_update_gi_fd, first_update_gi_file_name = tempfile.mkstemp( ".gis", "tmp", options.scratch_dir )
    first_update_file = os.fdopen( first_update_gi_fd, 'w' )
    first_update_file.writelines( first_update_gis )
    first_update_file.close()
        
    del orig_gi_list[3*unit_size : len(orig_gi_list)]
    del gi_list[3*unit_size : len(orig_gi_list)]

    gi_dump1_fd, gi_dump1_file_name = tempfile.mkstemp( ".gis", "tmp", options.scratch_dir )
    gi_dump1_file = os.fdopen( gi_dump1_fd, 'w' )
    gi_dump1_file.writelines(orig_gi_list[0 : unit_size])
    gi_dump1_file.close()
    
    gi_dump2_fd, gi_dump2_file_name = tempfile.mkstemp( ".gis", "tmp", options.scratch_dir )
    gi_dump2_file = os.fdopen( gi_dump2_fd, 'w' )
    gi_dump2_file.writelines(orig_gi_list[unit_size : unit_size*2])
    gi_dump2_file.close()
    
    gi_dump3_fd, gi_dump3_file_name = tempfile.mkstemp( ".gis", "tmp", options.scratch_dir )
    gi_dump3_file = os.fdopen( gi_dump3_fd, 'w' )
    gi_dump3_file.writelines(orig_gi_list[unit_size*2 : unit_size*3])
    gi_dump3_file.close()
    
    print "Fetching ASN:"
    args = [ "id1_fetch", "-in", gi_file_name, "-fmt", "asnb", "-out",
                complete_asn_file_name, "-logfile", log_file_name ]
    asn_cache_subprocess.Run( args, "complete ASN id1_fetch failed ", options.verbose )
    args = [ "id1_fetch", "-in", gi_dump1_file_name, "-fmt", "asnb", "-out",
                asn_file1_name, "-logfile", log_file_name ]
    asn_cache_subprocess.Run( args, "main id1_fetch failed ", options.verbose )
    args = [ "id1_fetch", "-in", gi_dump2_file_name, "-fmt", "asnb", "-out",
                asn_file2_name, "-logfile", log_file_name ]
    asn_cache_subprocess.Run( args, "main id1_fetch failed ", options.verbose )
    args = [ "id1_fetch", "-in", gi_dump3_file_name, "-fmt", "asnb", "-out",
                asn_file3_name, "-logfile", log_file_name ]
    asn_cache_subprocess.Run( args, "main id1_fetch failed ", options.verbose )
    args = [ "id1_fetch", "-in", first_update_gi_file_name, "-fmt", "asnb", "-out",
                update1_asn_file_name, "-logfile", log_file_name ]
    asn_cache_subprocess.Run( args, "first update main id1_fetch failed ", options.verbose )
    args = [ "id1_fetch", "-in", second_update_gi_file_name, "-fmt", "asnb", "-out",
                update2_asn_file_name, "-logfile", log_file_name ]
    asn_cache_subprocess.Run( args, "second update id1_fetch failed ", options.verbose )
        
    print "Creating dump2:"
    args = [ "test_dump_asn_index", "-i", asn_file1_name,
                "-dump", id_dump_dir_path, "-logfile", log_file_name,
                "-satid", "3", "-satkeystart", "1", "-satkeyend", "4" ]
    asn_cache_subprocess.Run( args, "main test_dump_asn_index failed ", options.verbose )
    args = [ "test_dump_asn_index", "-i", asn_file2_name,
                "-dump", id_dump_dir_path, "-logfile", log_file_name,
                "-satid", "3", "-satkeystart", "10", "-satkeyend", "14" ]
    asn_cache_subprocess.Run( args, "main test_dump_asn_index failed ", options.verbose )
    args = [ "test_dump_asn_index", "-i", asn_file3_name,
                "-dump", id_dump_dir_path, "-logfile", log_file_name,
                "-satid", "3", "-satkeystart", "20", "-satkeyend", "24" ]
    asn_cache_subprocess.Run( args, "main test_dump_asn_index failed ", options.verbose )
    
    print "Merging into master cache:"
    args = [ "merge_sub_caches", "-dump", id_dump_dir_path,
                "-cache", cache_dir_path, "-logfile", log_file_name, "-size", "50" ]
    asn_cache_subprocess.Run( args, "merge_sub_caches failed ", options.verbose )

    print "Dumping first update:"
# To fail by creating a lockfile, comment in this code.
#    args = [ "lockfile", id_dump_dir_path + "/.asndump_lock.1.0_0" ]
#    asn_cache_subprocess.Run( args, "creating the lockfile ", options.verbose )
    args = [ "test_dump_asn_index", "-i", update1_asn_file_name,
                "-dump", id_dump_dir_path, "-logfile", log_file_name,
                "-satid", "1", "-satkeystart", "0", "-satkeyend", "0" ]
    asn_cache_subprocess.Run( args, "first update test_dump_asn_index failed ", options.verbose )

    print "Updating master cache:"
    args = [ "update_cache", "-dump", id_dump_dir_path,
                "-cache", cache_dir_path, "-logfile", log_file_name ]
    asn_cache_subprocess.Run( args, "first update failed ", options.verbose )

    print "Dumping second update:"
    args = [ "test_dump_asn_index", "-i", update2_asn_file_name,
                "-dump", id_dump_dir_path, "-logfile", log_file_name,
                "-satid", "1", "-satkeystart", "0", "-satkeyend", "0" ]
    asn_cache_subprocess.Run( args, "second update test_dump_asn_index failed ", options.verbose )

    print "Updating master cache second time:"
    args = [ "update_cache", "-dump", id_dump_dir_path,
                "-cache", cache_dir_path, "-logfile", log_file_name ]
    asn_cache_subprocess.Run( args, "second update failed ", options.verbose )
        
    print "Creating subcache:"
    args = [ "sub_cache_create", "-cache", cache_dir_path,
                "-subcache", subcache_dir_path, "-i", subcache_gi_file_name, "-logfile",
                log_file_name ]
    if options.missing_gis and not options.extract:
        args.append( "-fetch-missing" )
    asn_cache_subprocess.Run( args, "sub_cache_create failed ", options.verbose )
    
    print "Extracting ASN from cache:"
    args =  [ "asn_cache_test", "-cache", subcache_dir_path, "-i", gi_file_name,
              "-o", target_asn_file_name, "-logfile", log_file_name, "-verify-ids", ]
    asn_cache_subprocess.Run( args, "asn_cache_test failed ", options.verbose )
    
    print "Comparing extracted ASN to original ASN:",
    if not filecmp.cmp( complete_asn_file_name, target_asn_file_name ):
        print "  fail"
        print "Extracted ASN does not match input ASN from ID."
        exit(1)
    else:
        print "  ok"
    
    if options.missing_gis and not options.extract:
        print "Extracting missing GIs ASN:"
        args = [ "asn_cache_test", "-cache", subcache_dir_path,
                    "-i", options.missing_gis, "-o", "/dev/null", "-logfile", log_file_name ]
        asn_cache_subprocess.Run( args, "asn_cache_test failed ", options.verbose )
    
    print "Extracting ASN from orig cache:"
    args =  [ "asn_cache_test", "-cache", cache_dir_path, "-i", gi_file_name,
              "-o", "/dev/null", "-logfile", log_file_name, "-verify-ids" ]
    asn_cache_subprocess.Run( args, "asn_cache_test failed ", options.verbose )
    
    print "Extracting ASN with data loader:"
    args =  [ "asn_cache_test", "-cache", subcache_dir_path, "-i", gi_file_name,
              "-o", target2_asn_file_name, "-logfile", log_file_name, "-verify-ids", "-test-loader" ]
    asn_cache_subprocess.Run( args, "asn_cache_test failed ", options.verbose )
    
    print "Comparing extracted ASN to original ASN:",
    if not filecmp.cmp( complete_asn_file_name, target2_asn_file_name ):
        print "  fail"
        print "Extracted ASN does not match input ASN from ID."
        exit(1)
    else:
        print "  ok"
    
    if options.extract:
        if not (options.missing_gis and options.reference_index):
            print "extract option requires list of missing gis and reference index output."
            exit(1)
        print "Creating subcache with cached:"
        args = [ "sub_cache_create", "-cache", "", "-fetch-missing",
                "-extract-product", "-extract-delta",
                "-subcache", fetched_subcache_dir_path, "-i", options.missing_gis, "-logfile",
                log_file_name ]
        if options.lds:
            args.append( "-lds2" )
            args.append( options.lds )
            args.append( "-nogenbank" )
        asn_cache_subprocess.Run( args, "sub_cache_create failed ", options.verbose )
        print "Comparing index of new subcache to reference:",
        args =  [ "dump_index", "-cache", fetched_subcache_dir_path,
                  "-o", fetched_cache_index_file_name, "-logfile", log_file_name,
                  "-no-timestamp", "-no-location" ]
        asn_cache_subprocess.Run( args, "dump_index failed ", options.verbose )
        if not filecmp.cmp( fetched_cache_index_file_name, options.reference_index ):
            print "  fail"
            print "Index of fetched gis subcache does not match reference index."
            exit(1)
        else:
            print "  ok"

    print "Testing completed successfully!"
    
    if not options.leave_droppings:
        os.remove( asn_file1_name )
        os.remove( asn_file2_name )
        os.remove( asn_file3_name )
        shutil.rmtree( id_dump_dir_path )
        shutil.rmtree( cache_dir_path )
        shutil.rmtree( subcache_dir_path )
        if options.extract:
            shutil.rmtree( fetched_subcache_dir_path )
        os.remove( target_asn_file_name )
        os.remove( target2_asn_file_name )
        os.remove( complete_asn_file_name )
        os.remove( fetched_cache_index_file_name )
        os.remove( gi_dump1_file_name )
        os.remove( gi_dump2_file_name )
        os.remove( gi_dump3_file_name )
        os.remove( first_update_gi_file_name )
        os.remove( update1_asn_file_name )
        os.remove( second_update_gi_file_name )
        os.remove( update2_asn_file_name )
        os.remove( log_file_name )
        if options.missing_gis and not options.extract:
            os.remove( subcache_gi_file_name )
        if scratch_dir_was_created:
            shutil.rmtree( options.scratch_dir )


if __name__ == "__main__":
    main()
