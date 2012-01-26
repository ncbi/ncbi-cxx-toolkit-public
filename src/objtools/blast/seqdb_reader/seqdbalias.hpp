#ifndef OBJTOOLS_READERS_SEQDB__SEQDBALIAS_HPP
#define OBJTOOLS_READERS_SEQDB__SEQDBALIAS_HPP

/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Author:  Kevin Bealer
 *
 */

/// @file seqdbalias.hpp
/// Defines database alias file access classes.
///
/// Defines classes:
///     CSeqDB_AliasWalker
///     CSeqDBAliasNode
///     CSeqDBAliasFile
///
/// Implemented for: UNIX, MS-Windows

#include <iostream>

#include <objtools/blast/seqdb_reader/seqdb.hpp>
#include <objtools/blast/seqdb_reader/seqdbcommon.hpp>
#include "seqdboidlist.hpp"
#include <objtools/blast/seqdb_reader/impl/seqdbfile.hpp>
#include "seqdbvol.hpp"
#include "seqdbvolset.hpp"
#include <objtools/blast/seqdb_reader/impl/seqdbgeneral.hpp>

BEGIN_NCBI_SCOPE

using namespace ncbi::objects;


/// CSeqDBAliasWalker class
/// 
/// Derivatives of this abstract class can be used to gather summary
/// data from the entire include tree of alias files.  For details of
/// the traversal order, see the WalkNodes documentation.

class CSeqDB_AliasWalker {
public:
    /// Destructor
    virtual ~CSeqDB_AliasWalker() {}
    
    /// Override to provide the alias file KEY name for the type of
    /// summary data you want to gather, for example "NSEQ".
    virtual const char * GetFileKey() const = 0;
    
    /// This will be called with each CVolume that is in the alias
    /// file tree structure (in order of traversal).
    virtual void Accumulate(const CSeqDBVol &) = 0;
    
    /// This will be called with the value associated with this key in
    /// the alias file.
    virtual void AddString (const string &) = 0;
};


/// CSeqDBAliasExplorer class
/// 
/// This is similar to the AliasWalker class.  Where the AliasWalker
/// provides a search key, the AliasExplorer is provided access to the
/// name->value map.  This allows it to examine relationships between
/// values and to do more complex analyses.

class CSeqDB_AliasExplorer {
public:
    /// Type of set used for KEY/VALUE pairs within each node
    typedef map<string, string> TVarList;
    
    /// Destructor
    virtual ~CSeqDB_AliasExplorer() {}
    
    /// This will be called with each CVolume that is in the alias
    /// file tree structure (in order of traversal).
    /// 
    /// @param volumes
    ///   A volume found during alias file traversal.
    virtual void Accumulate(const CSeqDBVol & volumes) = 0;
    
    /// This will be called with the map of key/value pairs associated
    /// with this alias file.  It should return true if this branch of
    /// the traversal tree has been satisfied, or false if traversal
    /// below this point is desireable.
    /// 
    /// @param values
    ///   The name/value pair map for this node.
    /// @return
    ///   True if this branch of traversal is done.
    virtual bool Explore(const TVarList & values) = 0;
};


/// CSeqDBAliasStack
/// 
/// When expanding a CSeqDBAliasNode, a test must be done to determine
/// whether each child nodes has already been expanded in this branch
/// of the traversal.  This class provides a set mechanism which
/// tracks node ancestry.

class CSeqDBAliasStack {
public:
    /// Constructor
    CSeqDBAliasStack()
        : m_Count(0)
    {
        m_NodeNames.resize(4);
    }
    
    /// Check whether the stack contains the specified string.
    ///
    /// This iterates over the vector of strings and returns true if
    /// the specified string is found.
    ///
    /// @param name
    ///   The alias file base name to add.
    /// @return
    ///   True if the string was found in the stack.
    bool Exists(const CSeqDB_Path & name)
    {
        for(unsigned i=0; i<m_Count; i++) {
            if (m_NodeNames[i] == name) {
                return true;
            }
        }
        return false;
    }
    
    /// Push a new string onto to the stack.
    ///
    /// The specified string is added to the stack.
    ///
    /// @param name
    ///   The alias file base name to add.
    void Push(const CSeqDB_Path & name)
    {
        // This design aims at efficiency (cycles, not memory).
        // Specifically, it tries to accomplish the following:
        //
        // 1. The m_NodeNames vector will be resized at most ln2(N)
        //    times where N is the maximal DEPTH of traversal.
        //
        // 2. Strings are not deallocated on return from lower depths,
        //    instead they are left in place as buffers for future
        //    assignments.
        //
        // 3. A particular element of the string array should be
        //    reallocated at most ln2(M/16) times, where M is the
        //    maximal length of the string, regardless of the number
        //    of traversals through that node-depth.
        //
        // The vector size is increased with resize(), in a doubling
        // pattern, and string data is reserve()d.  This code will
        // maintain vector.size == vector.capacity at all times.  If
        // vector.size fluctuated with each adding and removing of an
        // element, the strings between old-size and new-size would be
        // destructed, losing existing allocations.  With strings, the
        // resize method might cause blanking of memory, but the
        // reserve method should not.  In either case, the string size
        // will be set by the assign() method, and the true vector
        // usage is tracked via the m_Count field.
        
        if (m_NodeNames.size() == m_Count) {
            m_NodeNames.resize(m_NodeNames.size() * 2);
        }
        
        m_NodeNames[m_Count++].Assign(name.GetPathS());
    }
    
    /// Remove the top element of the stack
    void Pop()
    {
        _ASSERT(m_Count);
        m_Count--;
    }
    
    /// Return the number of in-use elements.
    unsigned Size()
    {
        return m_Count;
    }
    
private:
    /// List of node names.
    vector<CSeqDB_Path> m_NodeNames;
    
    /// Number of in-use node names.
    unsigned m_Count;
    
    /// Disable copy operator.
    CSeqDBAliasStack & operator =(const CSeqDBAliasStack &);
    
    /// Disable copy constructor.
    CSeqDBAliasStack(const CSeqDBAliasStack &);
};


/// CSeqDBAliasSets class
///
/// This acts as a layer between the alias processing code and the
/// atlas code in the case where a combined alias is used.  It
/// intercepts calls to find and use individual alias files and uses
/// combined alias files instead.

class CSeqDBAliasSets {
public:
    /// Constructor
    CSeqDBAliasSets(CSeqDBAtlas & atlas)
        : m_Atlas(atlas)
    {
    }
    
    /// Read an alias file given the path.
    ///
    /// This finds an alias file, or an equivalent section of a group
    /// alias file, given a filename.  The contents of the file (or of
    /// the corresponding part of the group file) are returned as a
    /// pair of pointers to the start and end of the buffer stored in
    /// the string that contains this data.  This code triggers the
    /// parsing of the entire group alias file if it exists and has
    /// not hithereto been read.  Group alias files could replace
    /// individual alias files, but at the moment, both will always be
    /// present.  If the group alias file does exist, it is assumed to
    /// be authoritative and complete.
    ///
    /// @param dbpath The name of the alias file (if it exists).
    /// @param bp The start of the alias file contents. [out]
    /// @param ep The end of the alias file contents. [out]
    /// @param locked The lock holder object for this thread. [in]
    /// @return True if an alias file (or equivalent data) was found.
    bool ReadAliasFile(const CSeqDB_Path  & dbpath,
                       const char        ** bp,
                       const char        ** ep,
                       CSeqDBLockHold     & locked);
    
    /// Resolve the alias file path.
    ///
    /// Given a partial path and name designating a particular db
    /// alias file, this method finds the absolute path of the group
    /// index file for that alias file, or if that is not found, the
    /// individual alias file.
    ///
    /// @param dbpath The path to the file. [in]
    /// @param resolved The resolved path is returned here. [out]
    /// @param locked The lock holder object for this thread. [in]
    /// @return True if the path was found.
    bool FindAliasPath(const CSeqDB_Path & dbpath,
                       CSeqDB_Path       * resolved,
                       CSeqDBLockHold    & locked);
    
    /// Find a file given a partial path and name.
    ///
    /// Given a path designating a particular disk file, this method
    /// finds the absolute path of that file.  The filename is assumed
    /// to contain the correct extension.
    ///
    /// @param dbname The partial path to the file, with extension. [in]
    /// @param resolved The resolved path is returned here. [out]
    /// @param locked The lock holder object for this thread. [in]
    /// @return True if the path was found.
    bool FindBlastDBPath(const CSeqDB_Path & dbname,
                         CSeqDB_Path       & resolved,
                         CSeqDBLockHold    & locked)
    {
        string resolved_str;
        
        if (x_FindBlastDBPath(dbname.GetPathS(),
                              '-',
                              true,
                              resolved_str,
                              locked)) {
            
            resolved.Assign(resolved_str);
            return true;
        }
        
        return false;
    }
    
    /// Find a file given a partial path and name.
    ///
    /// Given a path designating a particular disk file, this method
    /// finds the absolute path of that file.  The filename is assumed
    /// to not contain an extension.  Instead, the user indicates the
    /// type of database (p or n) and the function will search for
    /// that kind of database volume or alias file ('pin' or 'pal' for
    /// protein, 'nin' or 'nal' for nucleotide.)
    ///
    /// @param dbname The partial path to the file. [in]
    /// @param dbtype The type of sequences used. [in]
    /// @param resolved The resolved path is returned here. [out]
    /// @param locked The lock holder object for this thread. [in]
    /// @return True if the path was found.
    bool FindBlastDBPath(const CSeqDB_BasePath & dbname,
                         char                    dbtype,
                         CSeqDB_BasePath       & resolved,
                         CSeqDBLockHold        & locked)
    {
        string resolved_str;
        
        if (x_FindBlastDBPath(dbname.GetBasePathS(),
                              dbtype,
                              false,
                              resolved_str,
                              locked)) {
            
            resolved.Assign(resolved_str);
            return true;
        }
        
        return false;
    }
    
private:
    /// Find a file given a partial path and name.
    ///
    /// Given a path designating a particular disk file, this method
    /// finds the absolute path of that file.  The user indicates the
    /// type of database (p or n) to find appropriate extensions for
    /// index or alias files, or specifies exact=true if the filename
    /// already has the correct extension.  Before the filesystem is
    /// consulted, however, the m_PathLookup map is checked to see if
    /// an answer to this query already exists.
    ///
    /// @param dbname The partial path to the file. [in]
    /// @param dbtype The type of sequences in the DB. [in]
    /// @param exact Specify true if dbname contains the extension. [in]
    /// @param resolved The resolved path is returned here. [out]
    /// @param locked The lock holder object for this thread. [in]
    /// @return True if the path was found.
    bool x_FindBlastDBPath(const string   & dbname,
                           char             dbtype,
                           bool             exact,
                           string         & resolved,
                           CSeqDBLockHold & locked);
    
    /// Find the path of a group index from an alias file name.
    ///
    /// This method takes the path of an alias file as input.  The
    /// filename is extracted and returned in alias_name.  The name
    /// of the associated group index file is computed and returned
    /// in index_path.  This consists of the directory of the alias
    /// file combined with the standard group index filename.
    ///
    /// @param fname Location of the individual alias file. [in]
    /// @param index_name Location of the group index file. [out]
    /// @param alias_name Filename portion of the alias file. [out]
    void x_DbToIndexName(const CSeqDB_Path & fname,
                         CSeqDB_Path       & index_name,
                         CSeqDB_FileName   & alias_name);
    
    /// Read the contents of the group alias file.
    ///
    /// This reads a group alias file.  The individual alias file
    /// contents are stored in m_Groups, but are not parsed yet.
    ///
    /// @param group_fname The filename for the group file. [in]
    /// @param locked The lock holder object for this thread. [in]
    void x_ReadAliasSetFile(const CSeqDB_Path & group_fname,
                            CSeqDBLockHold    & locked);
    
    /// Reference to the memory management layer.
    CSeqDBAtlas & m_Atlas;
    
    /// Aggregated alias file - maps filename to file contents.
    typedef map<string, string> TAliasGroup;
    
    /// Full index filename to aggregated alias file.
    typedef map< string, TAliasGroup > TAliasGroupMap;
    
    /// Alias groups.
    TAliasGroupMap m_Groups;
    
    /// Caches results of FindBlastDBPath
    map<string, string> m_PathLookup;
    
    /// Disable copy operator.
    CSeqDBAliasSets & operator =(const CSeqDBAliasSets &);
    
    /// Disable copy constructor.
    CSeqDBAliasSets(const CSeqDBAliasSets &);
};

/// CSeqDBAliasNode class
///
/// This is one node of the alias node tree, an n-ary tree which
/// represents the relationships of the alias files and volumes used
/// by a CSeqDB instance.  The children of this node are the other
/// alias files mentioned in this node's DBLIST key.  Each node may
/// also have volumes, which are not strictly children (not the same
/// species), but are treated that way for the purpose of some
/// computations.  The volumes are the non-alias objects mentioned in
/// the DBLIST, and are the containers for actual sequence, header,
/// and summary data.
///
/// As a special case, an alias node which mentions its own name in
/// the DBLIST is interpreted as referring to an index file with the
/// same base name and path.  Alias node trees can be quite complex
/// and nodes can share database volumes; sometimes there are hundreds
/// of nodes which refer to only a few underlying database volumes.
///
/// Nodes have two primary purposes: to override summary data (such as
/// the "title" field) which would otherwise be taken from the volume,
/// and to aggregate other alias files or volumes.  The top level
/// alias node is virtual - it does not refer to a real file on disk.
/// It's purpose is to aggregate the space-seperated list of databases
/// which are provided to the CSeqDB constructor.

class CSeqDBAliasNode : public CObject {
    /// Type of set used for KEY/VALUE pairs within each node
    typedef map<string, string> TVarList;
    
    /// Import type to allow shorter name.
    typedef TSeqDBAliasFileValues TAliasFileValues;
    
public:
    /// Public Constructor
    ///
    /// This is the user-visible constructor, which builds the top level
    /// node in the dbalias node tree.  This design effectively treats the
    /// user-input database list as if it were an alias file containing
    /// only the DBLIST specification.
    ///
    /// @param atlas
    ///   The memory management layer.
    /// @param name_list
    ///   The space delimited list of database names.
    /// @param prot_nucl
    ///   The type of sequences stored here.
    /// @param alias_sets
    ///   An alias file caching and combining layer.
    /// @param expand_links
    ///   Indicate if soft links should be expanded
    CSeqDBAliasNode(CSeqDBAtlas     & atlas,
                    const string    & name_list,
                    char              prot_nucl,
                    CSeqDBAliasSets & alias_sets,
                    bool              expand_links);
    
    /// Get the list of volume names
    ///
    /// The alias node tree is iterated to produce a list of all
    /// volume names.  This list will be sorted and unique.
    ///
    /// @param vols
    ///   The returned set of volume names
    /// @param alias
    ///   The returned set of alias names
    /// @param recursive
    ///   If true will descend the alias tree to the volume nodes
    void FindVolumePaths(vector<string> & vols, vector<string> * alias, bool recursive) const;
    
    /// Get the title
    ///
    /// This iterates this node and possibly subnodes of it to build
    /// and return a title string.  Alias files may override this
    /// value (stopping traversal at that depth).
    ///
    /// @param volset
    ///   The set of database volumes
    /// @return
    ///   A string describing the database
    string GetTitle(const CSeqDBVolSet & volset) const;
    
    /// Get the number of sequences available
    ///
    /// This iterates this node and possibly subnodes of it to compute
    /// the shortest sequence length.
    ///
    /// @param volset
    ///   The set of database volumes
    /// @return
    ///   The shortest sequence length
    Int4 GetMinLength(const CSeqDBVolSet & volset) const;

    /// Get the number of sequences available
    ///
    /// This iterates this node and possibly subnodes of it to compute
    /// the number of sequences available here.  Alias files may
    /// override this value (stopping traversal at that depth).  It is
    /// normally used to provide information on how many OIDs exist
    /// after filtering has been applied.
    ///
    /// @param volset
    ///   The set of database volumes
    /// @return
    ///   The number of included sequences
    Int8 GetNumSeqs(const CSeqDBVolSet & volset) const;
    
    /// Get the number of sequences available
    ///
    /// This iterates this node and possibly subnodes of it to compute
    /// the number of sequences available here.  Alias files may
    /// override this value (stopping traversal at that depth).  It is
    /// normally used to provide information on how many OIDs exist
    /// after filtering has been applied.
    ///
    /// @param volset
    ///   The set of database volumes
    /// @return
    ///   The number of included sequences
    Int8 GetNumSeqsStats(const CSeqDBVolSet & volset) const;
    
    /// Get the size of the OID range
    ///
    /// This iterates this node and possibly subnodes of it to compute
    /// the number of sequences in all volumes as encountered in
    /// traversal.  Alias files cannot override this value.  Filtering
    /// does not affect this value.
    ///
    /// @param volset
    ///   The set of database volumes
    /// @return
    ///   The number of OIDs found during traversal
    Int8 GetNumOIDs(const CSeqDBVolSet & volset) const;
    
    /// Get the total length of the set of databases
    ///
    /// This iterates this node and possibly subnodes of it to compute
    /// the total length of all sequences in all volumes included in
    /// the database.  This may count volumes several times (depending
    /// on alias tree structure).  Alias files can override this value
    /// (stopping traversal at that depth).  It is normally used to
    /// describe the amount of sequence data remaining after filtering
    /// has been applied.
    ///
    /// @param volset
    ///   The set of database volumes
    /// @return
    ///   The total length of all included sequences
    Uint8 GetTotalLength(const CSeqDBVolSet & volset) const;
    
    /// Get the total length of the set of databases
    ///
    /// This iterates this node and possibly subnodes of it to compute
    /// the total length of all sequences in all volumes included in
    /// the database.  This may count volumes several times (depending
    /// on alias tree structure).  Alias files can override this value
    /// (stopping traversal at that depth).  It is normally used to
    /// describe the amount of sequence data remaining after filtering
    /// has been applied.
    ///
    /// @param volset
    ///   The set of database volumes
    /// @return
    ///   The total length of all included sequences
    Uint8 GetTotalLengthStats(const CSeqDBVolSet & volset) const;
    
    /// Get the sum of the volume lengths
    ///
    /// This iterates this node and possibly subnodes of it to compute
    /// the total length of all sequences in all volumes as
    /// encountered in traversal.  This may count volumes several
    /// times (depending on alias tree structure).  Alias files cannot
    /// override this value.
    ///
    /// @param volset
    ///   The set of database volumes
    /// @return
    ///   The sum of all volumes lengths as traversed
    Uint8 GetVolumeLength(const CSeqDBVolSet & volset) const;
    
    /// Get the membership bit
    ///
    /// This iterates this node and possibly subnodes of it to find
    /// the membership bit, if there is one.  If more than one alias
    /// node provides a membership bit, only one will be used.  This
    /// value can only be found in alias files (volumes do not have a
    /// single internal membership bit).
    ///
    /// @param volset
    ///   The set of database volumes
    /// @return
    ///   The membership bit, or zero if none was found.
    int GetMembBit(const CSeqDBVolSet & volset) const;
    
    /// Check whether a db scan is need to compute correct totals.
    ///
    /// This traverses this node and its subnodes to determine whether
    /// accurate estimation of the total number of sequences and bases
    /// requires a linear time scan of the index files.
    ///
    /// @param volset
    ///   The set of database volumes.
    /// @return
    ///   True if the database scan is required.
    bool NeedTotalsScan(const CSeqDBVolSet & volset) const;
    
    /// Apply a simple visitor to each node of the alias node tree
    ///
    /// This iterates this node and possibly subnodes of it.  If the
    /// alias file contains an entry with the key returned by
    /// walker.GetFileKey(), the string will be sent to walker via the
    /// AddString() method.  If the alias file does not provide the
    /// value, the walker object will be applied to each subnode (by
    /// calling WalkNodes), and then to each volume of the tree by
    /// calling the Accumulate() method on the walker object.  Each
    /// type of summary data has its own properties, so there is a
    /// CSeqDB_AliasWalker derived class for each type of summary data
    /// that needs this kind of traversal.  This technique is referred
    /// to as the "visitor" design pattern.
    ///
    /// @param walker
    ///   The visitor object to recursively apply to the tree.
    /// @param volset
    ///   The set of database volumes
    void WalkNodes(CSeqDB_AliasWalker * walker,
                   const CSeqDBVolSet & volset) const;
    
    /// Apply a complex visitor to each node of the alias node tree
    ///
    /// This iterates this node and possibly subnodes of it.  At each
    /// node, the map of keys to values is provided to the explorer
    /// object via the Explore() method.  If the explorer object
    /// returns false, this branch of the tree has been pruned and
    /// traversal will not continue downward.  If it returns true,
    /// traversal continues down through the tree.  If traversal was
    /// not pruned, and volumes exist for this node, the Accumulate
    /// method is called for each volume after traversal through
    /// subnodes has been done.  Compared to the version that takes a
    /// CSeqDB_AliasWalker, this version of this method allows more
    /// flexibility because the explorer object has access to the
    /// entire map of name/value pairs.
    ///
    /// @param explorer
    ///   The visitor object to recursively apply to the tree.
    /// @param volset
    ///   The set of database volumes
    void WalkNodes(CSeqDB_AliasExplorer * explorer,
                   const CSeqDBVolSet   & volset) const;
    
    /// Set filtering options for all volumes
    ///
    /// This method applies all of this alias node's filtering options
    /// to all of its associated volumes (and subnodes, for GI lists).
    /// It then iterates over subnodes, recursively calling SetMasks()
    /// to apply filtering options throughout the alias node tree.
    /// The virtual OID lists are not built as a result of this
    /// process, but the data necessary for virtual OID construction
    /// is copied to the volume objects.
    ///
    /// @param volset
    ///   The database volume set
    void SetMasks(CSeqDBVolSet & volset);
    
    /// Get Name/Value Data From Alias Files
    ///
    /// SeqDB treats each alias file as a map from a variable name to
    /// a value.  This method will return a map from the basename of
    /// the filename of each alias file, to a mapping from variable
    /// name to value for each entry in that file.  For example, the
    /// value of the "DBLIST" entry in the "wgs.nal" file would be
    /// values["wgs"]["DBLIST"].  The lines returned have been
    /// processed somewhat by SeqDB, including normalizing tabs to
    /// whitespace, trimming leading and trailing whitespace, and
    /// removal of comments and other non-value lines.  Care should be
    /// taken when using the values returned by this method.  SeqDB
    /// uses an internal "virtual" alias file entry to aggregate the
    /// values passed into SeqDB by the user.  This mapping uses a
    /// filename of "-" and contains a single entry mapping "DBLIST"
    /// to SeqDB's database name input.  This entry is the root of the
    /// alias file inclusion tree.  Also note that alias files that
    /// appear in several places in the alias file inclusion tree only
    /// have one entry in the returned map (and are only parsed once
    /// by SeqDB).
    /// 
    /// @param afv
    ///   The alias file values will be returned here.
    void GetAliasFileValues(TAliasFileValues & afv) const;
    
    /// Add computed values to alias node lacking them.
    ///
    /// Some of the standard alias file key/values pairs are, in fact,
    /// designed to override for values found in the corresponding
    /// volumes.  The callers of the GetAliasFileValues() method may
    /// want to use these values on a per-alias-file basis.  But of
    /// these values are only present in the alias file if the author
    /// of that file wanted to replace the value found in the volume.
    /// 
    /// This method iterates over the alias file nodes, filling in
    /// values found in the volumes, in those cases where the alias
    /// file did not override the value.  Only those values that have
    /// been useful to a user of CSeqDB are added via this method,
    /// which so only includes the TITLE.
    ///
    /// @param volset The set of volumes for this database.
    void CompleteAliasFileValues(const CSeqDBVolSet & volset);
    
    /// Build the filter tree for this node and its children.
    /// @param ftree The result is returned here.
    void BuildFilterTree(class CSeqDB_FilterTree & ftree) const;
    
    /// Computes the masking information for each alias node.
    ///
    /// This object process each alias file node to construct a
    /// summary of the kind of OID filtering applied there.  The
    /// has_filters parameter will be set to true if any filtering
    /// was done.
    ///
    /// @param has_filters Will be set true if any filtering is done.
    void ComputeMasks(bool & has_filters);

    /// Get Gi-based Mask Names From Alias Files
    ///
    /// This will return the MASKLIST field of the alias node.
    ///
    /// @param mask_list
    ///   The mask names will be returned here.
    void GetMaskList(vector <string> & mask_list);
    
    /// Is the top node alias file associated with Gi based masks?
    ///
    /// This will return true if the MASKLIST field of the top alias
    /// node is set.
    ///
    /// @return TRUE if MASKLIST field is present
    bool HasGiMask() const
    {
        return m_HasGiMask;
    };

private:
    /// Private Constructor
    ///
    /// This constructor is used to build the alias nodes other than
    /// the topmost node.  It is private, because such nodes are only
    /// constructed via internal mechanisms of this class.  One
    /// potential issue for alias node hierarchies is that it is easy
    /// to (accidentally) construct mutually recursive alias
    /// configurations.  To prevent an infinite recursion in this
    /// case, this constructor takes a set of strings, which indicate
    /// all the nodes that have already been constructed.  It is
    /// passed by value (copied) because the same node can be used,
    /// legally and safely, in more than one branch of the same alias
    /// node tree.  If the node to build is already in this set, the
    /// constructor will throw an exception.  As a special case, if a
    /// name in a DBLIST line is the same as the node it is in, it is
    /// assumed to be a volume name (even though an alias file exists
    /// with that name), so this will not trigger the cycle detection
    /// exception.
    ///
    /// @param atlas
    ///   The memory management layer
    /// @param dbpath
    ///   The working directory for relative paths in this node
    /// @param dbname
    ///   The name of this node
    /// @param prot_nucl
    ///   Indicates whether protein or nucletide sequences will be used
    /// @param recurse
    ///   Node history for cycle detection
    /// @param locked
    ///   The lock holder object for this thread. [in]
    /// @param alias_sets
    ///   An alias file caching and combining layer.
    /// @param expand_links
    ///   Indicate if soft links should be expanded
    CSeqDBAliasNode(CSeqDBAtlas           & atlas,
                    const CSeqDB_DirName  & dbpath,
                    const CSeqDB_BaseName & dbname,
                    char                    prot_nucl,
                    CSeqDBAliasStack      & recurse,
                    CSeqDBLockHold        & locked,
                    CSeqDBAliasSets       & alias_sets,
                    bool                    expand_links);
    
    /// Read the alias file
    ///
    /// This function read the alias file from the atlas, parsing the
    /// lines and storing the KEY/VALUE pairs in this node.  It
    /// ignores KEY values that are not supported in SeqDB, although
    /// currently SeqDB should support all of the defined KEYs.
    ///
    /// @param fn
    ///   The name of the alias file
    /// @param locked
    ///   The lock holder object for this thread. [in]
    void x_ReadValues(const CSeqDB_Path & fn, CSeqDBLockHold & locked);
    
    /// Read one line of the alias file
    ///
    /// This method parses the specified character string, storing the
    /// results in the KEY/VALUE map in this node.  The input string
    /// is specified as a begin/end pair of pointers.  If the string
    /// starts with "#", the function has no effect.  Otherwise, if
    /// there are tabs in the input string, they are silently
    /// converted to spaces, and the part of the string before the
    /// first space after the first nonspace is considered to be the
    /// key.  The rest of the line (with initial and trailing spaces
    /// removed) is taken as the value.
    ///
    /// @param bp
    ///   A pointer to the first character of the line
    /// @param ep
    ///   A pointer to (one past) the last character of the line
    /// @param name_s
    ///   The variable name from the file
    /// @param value_s
    ///   The value from the file
    void x_ReadLine(const char * bp,
                    const char * ep,
                    string     & name_s,
                    string     & value_s);
    
    /// Expand a node of the alias node tree recursively
    ///
    /// This method expands a node of the alias node tree, recursively
    /// building the tree from the specified node downward.  (This
    /// method and the private version of the constructor are mutually
    /// recursive.)  The cyclic tree check is done, and paths of these
    /// components are resolved.  The alias file is parsed, and for
    /// each member of the DBLIST set, a subnode is constructed or a
    /// volume name is stored (if the element is the same as this
    /// node's name).
    ///
    /// @param this_name
    ///   The name of this node
    /// @param prot_nucl
    ///   Indicates whether this is a protein or nucleotide database.
    /// @param recurse
    ///   Set of all ancestor nodes for this node.
    /// @param locked
    ///   The lock holder object for this thread. [in]
    void x_ExpandAliases(const CSeqDB_BasePath & this_name,
                         char                    prot_nucl,
                         CSeqDBAliasStack      & recurse,
                         CSeqDBLockHold        & locked);
    
    /// Build a list of volume names used by the alias node tree
    /// 
    /// This adds the volume names used here to the specified set.
    /// The same method is called on all subnodes, so all volumes from
    /// this point of the tree down will be added by this call.
    ///
    /// @param vols
    ///   The set of strings to receive the volume names
    void x_FindVolumePaths(set<string> & vols, set<string> & alias) const;
    
    /// Name resolution
    ///
    /// This finds the path for each name in m_DBList, and resolves
    /// the path for each.  This is only done during construction of
    /// the topmost node.  Names supplied by the end user get this
    /// treatment, lower level nodes will have absolute or relative
    /// paths to specify the database locations.
    ///
    /// After alls names are resolved, the longest common prefix (of
    /// all names) is found and moved to the dbname_path variable (and
    /// removed from each individual name).
    ///
    /// @param prot_nucl
    ///   Indicates whether this is a protein or nucleotide database
    /// @param locked
    ///   The lock hold object for this thread. [in]
    void x_ResolveNames(char prot_nucl, CSeqDBLockHold & locked);
    
    /// Get the contents of an alias file.
    ///
    /// Fetches the lines belonging to an alias file, either directly
    /// or via a combined alias file.
    void x_ReadAliasFile(CSeqDBMemLease    & lease,
                         const CSeqDB_Path & fname,
                         const char       ** bp,
                         const char       ** ep,
                         CSeqDBLockHold    & locked);
    
    /// Tokenize (split) the list of database names.
    ///
    /// The provided string is split using the space character as a
    /// delimiter.  The resulting names are added to the m_DBList
    /// vector and will become sub-nodes or opened as volumes.
    ///
    /// @param dbnames Space seperated list of database names.
    void x_Tokenize(const string & dbnames);
    
    /// Append a subnode to this alias node.
    ///
    /// This method appends a new subnode to this node of the alias
    /// node tree.  It is called by the x_ExpandAliases method.
    ///
    /// @param node_path
    ///   The base path of the new node's volume.
    /// @param prot_nucl
    ///   Indicates whether this is a protein or nucleotide database.
    /// @param recurse
    ///   Set of all ancestor nodes for this node.
    /// @param locked
    ///   The lock holder object for this thread. [in]
    void x_AppendSubNode(CSeqDB_BasePath  & node_path,
                         char               prot_nucl,
                         CSeqDBAliasStack & recurse,
                         CSeqDBLockHold   & locked);
    
    /// Type used to store a set of volume names for each node
    typedef vector<CSeqDB_BasePath> TVolNames;
    
    /// Type used to store the set of subnodes for this node
    typedef vector< CRef<CSeqDBAliasNode> > TSubNodeList;
    
    
    /// The memory management layer for this SeqDB instance
    CSeqDBAtlas & m_Atlas;
    
    /// The common prefix for the DB paths.
    CSeqDB_DirName m_DBPath;
    
    /// List of KEY/VALUE pairs from this alias file
    TVarList m_Values;
    
    /// Set of volume names associated with this node
    TVolNames m_VolNames;
    
    /// List of subnodes contained by this node
    TSubNodeList m_SubNodes;
    
    /// Filename of this alias file
    CSeqDB_Path m_ThisName;
    
    /// Tokenized version of DBLIST
    vector<CSeqDB_BasePath> m_DBList;

    /// Do we have Gi masks for the top node?
    /// (only applicable to the top node)
    bool m_HasGiMask;

    /// Should we skip local DB search for this DBLIST?
    vector<bool> m_SkipLocal;
    
    /// Combined alias files.
    CSeqDBAliasSets & m_AliasSets;
    
    /// Mask objects for this node.
    vector< CRef<CSeqDB_AliasMask> > m_NodeMasks;

    /// Do not expand link when resolving paths
    bool m_ExpandLinks;
    
    /// Disable copy operator.
    CSeqDBAliasNode & operator =(const CSeqDBAliasNode &);
    
    /// Disable copy constructor.
    CSeqDBAliasNode(const CSeqDBAliasNode &);
};


/// CSeqDBAliasFile class
///
/// This class is an interface to the alias node tree.  It provides
/// functionality to classes like CSeqDBImpl (and others) that do not
/// need to understand alias walkers, nodes, and tree traversal.

class CSeqDBAliasFile : CObject {
    /// Import type to allow shorter name.
    typedef TSeqDBAliasFileValues TAliasFileValues;
    
public:
    /// Constructor
    ///
    /// This builds a tree of CSeqDBAliasNode objects from a
    /// space-seperated list of database names.  Every database
    /// instance has at least one node, because the top most node is
    /// an "artificial" node, which serves only to aggregate the list
    /// of databases specified to the constructor.  The tree is
    /// constructed in a depth first manner, and will be complete upon
    /// return from this constructor.
    ///
    /// @param atlas
    ///   The SeqDB memory management layer.
    /// @param name_list
    ///   A space seperated list of database names.
    /// @param prot_nucl
    ///   Indicates whether the database is protein or nucleotide.
    /// @param expand_links
    ///   Indicates whether the soft links should be expanded
    CSeqDBAliasFile(CSeqDBAtlas  & atlas,
                    const string & name_list,
                    char           prot_nucl,
                    bool           expand_links = true);
    
    /// Get the list of volume names.
    ///
    /// This method returns a reference to the vector of volume names.
    /// The vector will contain all volume names mentioned in any of
    /// the DBLIST lines in the hierarchy of the alias node tree.  The 
    /// volume names do not include an extension (such as .pin or .nin).
    ///
    /// @return
    ///   Reference to the internal vector of volume names.
    const vector<string> & GetVolumeNames() const
    {
        return m_VolumeNames;
    }
    
    /// Find the base names of volumes.
    /// 
    /// This method populates the vector with volume names.
    /// 
    /// @param vols  The vector to be populated with volume names
    /// @param recursive  If true, vol will include all volume names
    /// within the alias node tree.  Otherwise, only the top-node volume
    /// names are included
    void FindVolumePaths(vector<string> & vols, vector<string> * alias, bool recursive) const
    {
        if (recursive) {
            // use the cached results 
            vols = m_VolumeNames; 
            if (alias) {
                 *alias = m_AliasNames;
            }
        }
        else {
            m_Node->FindVolumePaths(vols, alias, recursive);
        }
    };

    /// Get the title
    ///
    /// This iterates the alias node tree to build and return a title
    /// string.  Alias files may override this value (stopping
    /// traversal at that depth).
    ///
    /// @param volset
    ///   The set of database volumes
    /// @return
    ///   A string describing the database
    string GetTitle(const CSeqDBVolSet & volset) const;
    
    /// Get the number of sequences available
    ///
    /// This iterates this node and possibly subnodes of it to compute
    /// the shortest sequence length.
    ///
    /// @param volset
    ///   The set of database volumes
    /// @return
    ///   The shortest sequence length
    Int4 GetMinLength(const CSeqDBVolSet & volset) const;

    /// Get the number of sequences available
    ///
    /// This iterates the alias node tree to compute the number of
    /// sequences available here.  Alias files may override this value
    /// (stopping traversal at that depth).  It is normally used to
    /// provide information on how many OIDs exist after filtering has
    /// been applied.
    ///
    /// @param volset
    ///   The set of database volumes
    /// @return
    ///   The number of included sequences
    Int8 GetNumSeqs(const CSeqDBVolSet & volset) const;
    
    /// Get the number of sequences available
    ///
    /// This iterates the alias node tree to compute the number of
    /// sequences available here.  Alias files may override this value
    /// (stopping traversal at that depth).  It is normally used to
    /// provide information on how many OIDs exist after filtering has
    /// been applied.  This is like GetNumSeqs, but uses STATS_NSEQ.
    ///
    /// @param volset
    ///   The set of database volumes
    /// @return
    ///   The number of included sequences
    Int8 GetNumSeqsStats(const CSeqDBVolSet & volset) const;
    
    /// Get the size of the OID range
    ///
    /// This iterates the alias node tree to compute the number of
    /// sequences in all volumes as encountered in traversal.  Alias
    /// files cannot override this value.  Filtering does not affect
    /// this value.
    ///
    /// @param volset
    ///   The set of database volumes
    /// @return
    ///   The number of OIDs found during traversal
    Int8 GetNumOIDs(const CSeqDBVolSet & volset) const;
    
    /// Get the total length of the set of databases
    ///
    /// This iterates the alias node tree to compute the total length
    /// of all sequences in all volumes included in the database.
    /// This may count volumes several times (depending on alias tree
    /// structure).  Alias files can override this value (stopping
    /// traversal at that depth).  It is normally used to describe the
    /// amount of sequence data remaining after filtering has been
    /// applied.
    ///
    /// @param volset
    ///   The set of database volumes
    /// @return
    ///   The total length of all included sequences
    Uint8 GetTotalLength(const CSeqDBVolSet & volset) const;
    
    /// Get the total length of the set of databases
    ///
    /// This iterates the alias node tree to compute the total length
    /// of all sequences in all volumes included in the database.
    /// This may count volumes several times (depending on alias tree
    /// structure).  Alias files can override this value (stopping
    /// traversal at that depth).  It is normally used to describe the
    /// amount of sequence data remaining after filtering has been
    /// applied.  This is like GetTotalLength but uses STATS_TOTLEN.
    ///
    /// @param volset
    ///   The set of database volumes
    /// @return
    ///   The total length of all included sequences
    Uint8 GetTotalLengthStats(const CSeqDBVolSet & volset) const;
    
    /// Get the sum of the volume lengths
    ///
    /// This iterates the alias node tree to compute the total length
    /// of all sequences in all volumes as encountered in traversal.
    /// This may count volumes several times (depending on alias tree
    /// structure).  Alias files cannot override this value.
    ///
    /// @param volset
    ///   The set of database volumes
    /// @return
    ///   The sum of all volumes lengths as traversed
    Uint8 GetVolumeLength(const CSeqDBVolSet & volset) const;
    
    /// Get the membership bit
    ///
    /// This iterates the alias node tree to find the membership bit,
    /// if there is one.  If more than one alias node provides a
    /// membership bit, only one will be used.  This value can only be
    /// found in alias files (volumes do not have a single internal
    /// membership bit).
    ///
    /// @param volset
    ///   The set of database volumes
    /// @return
    ///   The membership bit, or zero if none was found.
    int GetMembBit(const CSeqDBVolSet & volset) const;
    
    /// Check whether a db scan is need to compute correct totals.
    ///
    /// This traverses this node and its subnodes to determine whether
    /// accurate estimation of the total number of sequences and bases
    /// requires a linear time scan of the index files.
    ///
    /// @param volset
    ///   The set of database volumes.
    /// @return
    ///   True if the database scan is required.
    bool NeedTotalsScan(const CSeqDBVolSet & volset) const;
    
    /// Check if any volume filtering exists.
    ///
    /// This method computes and caches the sequence filtering for
    /// this node and any subnodes, and returns true if any filtering
    /// exists.  Subsequent calls will just return the cached value.
    ///
    /// @return True if any filtering exists.
    bool HasFilters()
    {
        x_ComputeMasks();
        return m_HasFilters;
    }
    
    /// Get filtering tree for all volumes.
    ///
    /// This method applies the filtering options found in the alias
    /// node tree to all associated volumes (iterating over the tree
    /// recursively).  The virtual OID lists are not built as a result
    /// of this process, but the data necessary for virtual OID
    /// construction is copied to the volume objects.
    ///
    /// @return A filter tree for all volumes.
    CRef<CSeqDB_FilterTree> GetFilterTree();
    
    /// Get Name/Value Data From Alias Files
    ///
    /// SeqDB treats each alias file as a map from a variable name to
    /// a value.  This method will return a map from the basename of
    /// the filename of each alias file, to a mapping from variable
    /// name to value for each entry in that file.  For example, the
    /// value of the "DBLIST" entry in the "wgs.nal" file would be
    /// values["wgs"]["DBLIST"].  The lines returned have been
    /// processed somewhat by SeqDB, including normalizing tabs to
    /// whitespace, trimming leading and trailing whitespace, and
    /// removal of comments and other non-value lines.  Care should be
    /// taken when using the values returned by this method.  SeqDB
    /// uses an internal "virtual" alias file entry to aggregate the
    /// values passed into SeqDB by the user.  This mapping uses a
    /// filename of "-" and contains a single entry mapping "DBLIST"
    /// to SeqDB's database name input.  This entry is the root of the
    /// alias file inclusion tree.  Also note that alias files that
    /// appear in several places in the alias file inclusion tree only
    /// have one entry in the returned map (and are only parsed once
    /// by SeqDB).
    /// 
    /// @param afv
    ///   The alias file values will be returned here.
    /// @param volset
    ///   The set of database volumes
    void GetAliasFileValues(TAliasFileValues   & afv,
                            const CSeqDBVolSet & volset);

    /// Is the top node alias file associated with Gi based masks?
    ///
    /// This will return true if the MASKLIST field of the top alias
    /// node is set.
    ///
    /// @return TRUE if MASKLIST field is present
    bool HasGiMask() const
    {
        return (m_Node->HasGiMask());
    }

    /// Get Gi-based Mask Names From Alias Files
    ///
    /// This will return the MASKLIST field of the top alias node.
    ///
    /// @param mask_list
    ///   The mask names will be returned here.
    void GetMaskList(vector <string> &mask_list)
    {
        m_Node->GetMaskList(mask_list);
    }
    
private:
    /// Compute filtering options for all volumes.
    ///
    /// This method applies the filtering options found in the alias
    /// node tree to all associated volumes (iterating over the tree
    /// recursively).  The virtual OID lists are not built as a result
    /// of this process, but the data necessary for virtual OID
    /// construction is copied to the volume objects.
    void x_ComputeMasks()
    {
        m_Node->ComputeMasks(m_HasFilters);
    }
    
    /// Combined alias files.
    CSeqDBAliasSets m_AliasSets;

    /// This is the alias node tree's "artificial" topmost node, which
    /// aggregates the user provided database names.
    CRef<CSeqDBAliasNode> m_Node;
    
    /// The cached output of the topmost node's FindVolumePaths(recursive).
    vector<string> m_VolumeNames;

    /// The cached output of the topmost node's FindVolumePaths(recursive).
    vector<string> m_AliasNames;
    
    /// True if this is a protein database.
    bool m_IsProtein;

    /// Shortest sequence length
    mutable Int4 m_MinLength;
    
    /// Number of sequences.
    mutable Int8 m_NumSeqs;
    
    /// Number of sequences for statistics purposes.
    mutable int m_NumSeqsStats;
    
    /// Number of OIDs.
    mutable Int8 m_NumOIDs;
    
    /// Total length.
    mutable Int8 m_TotalLength;
    
    /// Total length for statistics purposes.
    mutable Int8 m_TotalLengthStats;
    
    /// Total length ignoring filtering.
    mutable Int8 m_VolumeLength;
    
    /// Membership bit.
    mutable int m_MembBit;
    
    /// True if we have the database title.
    mutable bool m_HasTitle;
    
    /// Database title.
    mutable string m_Title;
    
    /// 1 if we need a totals scan, 0 if not, -1 if not known.
    mutable int m_NeedTotalsScan;

    /// Filter tree representing all alias file filtering.
    CRef<CSeqDB_FilterTree> m_TopTree;
    
    /// Are there filters for this database?
    bool m_HasFilters;
    
    /// Disable copy operator.
    CSeqDBAliasFile & operator =(const CSeqDBAliasFile &);
    
    /// Disable copy constructor.
    CSeqDBAliasFile(const CSeqDBAliasFile &);
};


END_NCBI_SCOPE

#endif // OBJTOOLS_READERS_SEQDB__SEQDBALIAS_HPP


