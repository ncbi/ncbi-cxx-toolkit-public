#ifndef ALGO_BLAST_VDB___VDBSRC_VDBALIAS_HPP
#define ALGO_BLAST_VDB___VDBSRC_VDBALIAS_HPP

/// @file vdbalias.hpp
/// Defines database alias file access classes.
///
/// Defines classes:
///     CVDBAliasNode
///     CVDBAliasFile
///

//#include <iostream>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>



BEGIN_NCBI_SCOPE

USING_SCOPE(ncbi::objects);

class CVDBAliasNode;

/// CVDBAliasFile class
///
/// This class is an interface to the alias node tree.  It provides
/// functionality to classes like CSeqDBImpl (and others) that do not
/// need to understand alias walkers, nodes, and tree traversal.

class NCBI_VDB2BLAST_EXPORT CVDBAliasUtil {

public:

    /// Get the list of vdb names
    ///
    /// The alias node tree is iterated to produce a list of all
    /// db names.  This list will be sorted and unique. Note that
    /// only vdbs are returned (blast dbs are ignored)
    ///
    /// @param dbname
    ///   Can be a mix of blast db and vdb names
	/// @param paths
	///	  If recurisve, it will transverse down the alias tree
	///	  and reolsve all the paths for vdbs found (Note: no alias file returned)
	///	  In the non-recurisve case, function will return any dbs and alias files
	///   found from the top-level node
	/// @param db_alias_list
    ///   The returned set of blast alias names
	///	  If recursive, will return all blast db alias files found
	///	  Otherwise only the top level blast db alias files are returned
    /// @param vdb_alias_list
    ///   The returned set of blast alias names (same recursive behaviour as db_alias_list)
    /// @param recursive
    ///   If true will descend the alias tree to the volume nodes
    /// @param expand_link
    ///   If ture, links are expanded in the resolved paths

    static void FindVDBPaths(const string   & dbname,
            		  	  	 bool             isProtein,
            		  	  	 vector<string> & paths,
            		  		 vector<string> * db_alias_list = NULL,
            		  		 vector<string> * vdb_alias_list = NULL,
            		  		 bool             recursive = true,
            		  		 bool             expand_links = true,
            		  		 bool             verify_dbs = true);

    /// Process a list with mixed blast dbs and vdbs
    /// Expand all alais file to find and return vdbs with full path
    /// Return a modified input_list where top level
    /// vdbs and vdb alias files are filtered out (safe for CSeqDB)
    /// @param input_list 	input list with mixed vdbs and blast dbs
    /// @param isProtein	is protein db ?
    /// @param vdb_list		expanded vdb list with full path delimited by " "
    /// @param blastdb_list	top level blast db list delimited by " "
    /// @param expand and check vdb list
    static void ProcessMixedDBsList(const string   & input_list,
    					            bool             isProtein,
    					 	        vector<string> & vdb_list,
    						        string         & blastdb_list,
    						        bool			 verify_vdbs);

};


END_NCBI_SCOPE

#endif /* ALGO_BLAST_VDB___VDBSRC_VDBALIAS_HPP */


