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

#include <objtools/readers/seqdb/seqdb.hpp>
#include "seqdboidlist.hpp"
#include "seqdbfile.hpp"
#include "seqdbvol.hpp"
#include "seqdbvolset.hpp"
#include "seqdbgeneral.hpp"

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
        m_NodeNames.resize(1);
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
    bool Exists(const string & name)
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
    void Push(const string & name)
    {
        // This design aims at efficiency (cycles, not memory).
        // Specifically, it tries to accomplish the following:
        //
        // 1. The m_NodeNames vector will be resized at most ln2(N)
        //    times where N is the maximal DEPTH of traversal.
        //
        // 2. Strings are not deallocated on return from lower depths,
        //    instead they are left in place as buffers for future
        //    allocations.
        //
        // 3. A particular element of the string array should be
        //    reallocated at most ln2(M/16) times, where M is the
        //    maximal length of the string, regardless of the number
        //    of traversals through that node-depth.
        //
        // The vector is always resize()d, whereas string data is
        // reserve()d.  We want vector.size == vector.capacity at all
        // times.  If vector.size fluctuated with each adding and
        // removing of an element, the strings between old-size and
        // new-size might be allocated and freed.  With strings, the
        // resize method might cause blanking of memory, but the
        // reserve method should not.  In either case, the string size
        // will be set by operator=.
        
        if (m_NodeNames.size() == m_Count) {
            m_NodeNames.resize(m_NodeNames.size() * 2);
        }
        
        string & cur_node = m_NodeNames[m_Count++];
        
        // The following should not affect results, but may serve to
        // accomplish efficiency goal #3 above.
        
        if (cur_node.size() < name.size()) {
            if (name.size() <= 8) {
                cur_node.reserve(16);
            } else {
                cur_node.reserve(name.size() * 2);
            }
        }
        
        // This should preserve capacity (and never allocate).
        
        cur_node.assign(name.data(), name.length());
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
    vector<string> m_NodeNames;
    
    /// Number of in-use node names.
    unsigned m_Count;
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
    CSeqDBAliasNode(CSeqDBAtlas    & atlas,
                    const string   & name_list,
                    char             prot_nucl);
    
    /// Get the list of volume names
    ///
    /// The alias node tree is iterated to produce a list of all
    /// volume names.  This list will be sorted and unique.
    ///
    /// @param vols
    ///   The returned set of volume names
    void GetVolumeNames(vector<string> & vols) const;
    
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
    /// the number of sequences available here.  Alias files may
    /// override this value (stopping traversal at that depth).  It is
    /// normally used to provide information on how many OIDs exist
    /// after filtering has been applied.
    ///
    /// @param volset
    ///   The set of database volumes
    /// @return
    ///   The number of included sequences
    int GetNumSeqs(const CSeqDBVolSet & volset) const;
    
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
    int GetNumOIDs(const CSeqDBVolSet & volset) const;
    
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
    
    /// Apply a visitor to each node of the alias node tree
    ///
    /// This iterates this node and possibly subnodes of it.  If the
    /// alias file provides a value, it will be applied to walker with
    /// the AddString() method.  If the alias file does not provide
    /// the value, the walker object will be applied to each subnode
    /// (by calling WalkNodes), and then to each volume of the tree by
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
    ///   The lock holder object for this thread
    CSeqDBAliasNode(CSeqDBAtlas      & atlas,
                    const string     & dbpath,
                    const string     & dbname,
                    char               prot_nucl,
                    CSeqDBAliasStack & recurse,
                    CSeqDBLockHold   & locked);
    
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
    ///   The lock holder object for this thread
    void x_ReadValues(const string & fn, CSeqDBLockHold & locked);
    
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
    void x_ReadLine(const char * bp, const char * ep);
    
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
    ///   Indicates whether this is a protein or nucleotide database
    /// @param recurse
    ///   Set of all ancestor nodes for this node.
    /// @param locked
    ///   The lock holder object for this thread
    void x_ExpandAliases(const string     & this_name,
                         char               prot_nucl,
                         CSeqDBAliasStack & recurse,
                         CSeqDBLockHold   & locked);
    
    /// Build a resolved path from components
    ///
    /// This takes a directory, base name, and indication of protein
    /// or nucleotide to build an alias file name.  It combines the
    /// filename and path using the standard delimiter for this OS.
    /// 
    /// @param dir
    ///   The directory name
    /// @param name
    ///   A filename, possibly including directory components
    /// @param prot_nucl
    ///   Indicates whether this is a protein or nucleotide database
    /// @return
    ///   The combined path
    static string x_MkPath(const string & dir, const string & name, char prot_nucl)
    {
        return SeqDB_CombinePath(dir, name) + "." + prot_nucl + "al";
    }
    
    /// Build a list of volume names used by the alias node tree
    /// 
    /// This adds the volume names used here to the specified set.
    /// The same method is called on all subnodes, so all volumes from
    /// this point of the tree down will be added by this call.
    ///
    /// @param vols
    ///   The set of strings to receive the volume names
    void x_GetVolumeNames(set<string> & vols) const;
    
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
    ///   The lock hold object for this thread.
    void x_ResolveNames(char prot_nucl, CSeqDBLockHold & locked);
    
    /// Add an OID list filter to a volume
    /// 
    /// This method stores the OID mask filename specified in this
    /// alias file in the volume associated with this alias file.  The
    /// specified begin and end will also be applied as an OID range
    /// filter (0 and ULONG_MAX are used to indicate nonexistance of
    /// an OID range).
    /// 
    /// @param volset
    ///   The set of database volumes.
    /// @param begin
    ///   The first OID in the OID range.
    /// @param end
    ///   The OID after the last OID in the range.
    /// @param oidfile
    ///   The name of the OID mask file.
    void x_SetOIDMask(CSeqDBVolSet & volset,
                      int            begin,
                      int            end,
                      const string & oidfile);

    /// Add a GI list filter to a volume
    /// 
    /// This method stores the GI list filename specified in this
    /// alias file in the volume associated with this alias file.  The
    /// specified begin and end will also be applied as an OID range
    /// filter (0 and ULONG_MAX are used to indicate nonexistance of
    /// an OID range).
    /// 
    /// @param volset
    ///   The set of database volumes.
    /// @param begin
    ///   The first OID in the OID range.
    /// @param end
    ///   The OID after the last OID in the range.
    /// @param gilist
    ///   The name of the GI list file.
    void x_SetGiListMask(CSeqDBVolSet & volset,
                         int            begin,
                         int            end,
                         const string & gilist);
    
    /// Add an OID range to a volume
    /// 
    /// This method applies the specified begin and end as an OID
    /// range filter (0 and ULONG_MAX are used to indicate
    /// nonexistance of an OID range).
    /// 
    /// @param volset
    ///   The set of database volumes.
    /// @param begin
    ///   The first OID in the OID range.
    /// @param end
    ///   The OID after the last OID in the range.
    void x_SetOIDRange(CSeqDBVolSet & volset, int begin, int end);
    
    
    /// Type of set used for KEY/VALUE pairs within each node
    typedef map<string, string> TVarList;
    
    /// Type used to store a set of volume names for each node
    typedef vector<string> TVolNames;
    
    /// Type used to store the set of subnodes for this node
    typedef vector< CRef<CSeqDBAliasNode> > TSubNodeList;
    
    
    /// The memory management layer for this SeqDB instance
    CSeqDBAtlas & m_Atlas;
    
    /// The common prefix for the DB paths.
    string m_DBPath;
    
    /// List of KEY/VALUE pairs from this alias file
    TVarList m_Values;
    
    /// Set of volume names associated with this node
    TVolNames m_VolNames;
    
    /// List of subnodes contained by this node
    TSubNodeList m_SubNodes;
    
    /// Tokenized version of DBLIST
    vector<string> m_DBList;
};


/// CSeqDBAliasFile class
///
/// This class is an interface to the alias node tree.  It provides
/// functionality to classes like CSeqDBImpl (and others) that do not
/// need to understand alias walkers, nodes, and tree traversal.

class CSeqDBAliasFile {
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
    CSeqDBAliasFile(CSeqDBAtlas    & atlas,
                    const string   & name_list,
                    char             prot_nucl)
        : m_Node (atlas, name_list, prot_nucl)
    {
        m_Node.GetVolumeNames(m_VolumeNames);
    }
    
    /// Get the list of volume names.
    ///
    /// This method returns a reference to the vector of volume names.
    /// The vector will contain all volume names mentioned in any of
    /// the DBLIST lines in the of the alias node tree.  The volume
    /// names do not include an extension (such as .pin or .nin).
    ///
    /// @return
    ///   Reference to the internal vector of volume names.
    const vector<string> & GetVolumeNames() const
    {
        return m_VolumeNames;
    }
    
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
    string GetTitle(const CSeqDBVolSet & volset) const
    {
        return m_Node.GetTitle(volset);
    }
    
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
    int GetNumSeqs(const CSeqDBVolSet & volset) const
    {
        return m_Node.GetNumSeqs(volset);
    }
    
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
    int GetNumOIDs(const CSeqDBVolSet & volset) const
    {
        return m_Node.GetNumOIDs(volset);
    }
    
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
    Uint8 GetTotalLength(const CSeqDBVolSet & volset) const
    {
        return m_Node.GetTotalLength(volset);
    }
    
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
    Uint8 GetVolumeLength(const CSeqDBVolSet & volset) const
    {
        return m_Node.GetVolumeLength(volset);
    }
    
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
    int GetMembBit(const CSeqDBVolSet & volset) const
    {
        return m_Node.GetMembBit(volset);
    }
    
    /// Set filtering options for all volumes
    ///
    /// This method applies the filtering options found in the alias
    /// node tree to all associated volumes (iterating over the tree
    /// recursively).  The virtual OID lists are not built as a result
    /// of this process, but the data necessary for virtual OID
    /// construction is copied to the volume objects.
    ///
    /// @param volset
    ///   The database volume set
    void SetMasks(CSeqDBVolSet & volset)
    {
        m_Node.SetMasks(volset);
    }
    
private:
    /// This is the alias node tree's "artificial" topmost node, which
    /// aggregates the user provided database names.
    CSeqDBAliasNode m_Node;
    
    /// The cached output of the topmost node's GetVolumeNames().
    vector<string> m_VolumeNames;
};


END_NCBI_SCOPE

#endif // OBJTOOLS_READERS_SEQDB__SEQDBALIAS_HPP


