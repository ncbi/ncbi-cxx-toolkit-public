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

/// @file seqdbalias.cpp
/// Code which manages a hierarchical tree of alias file data.
/// 
/// Defines classes:
///     CSeqDB_TitleWalker 
///     CSeqDB_MaxLengthWalker 
///     CSeqDB_NSeqsWalker 
///     CSeqDB_NOIDsWalker 
///     CSeqDB_TotalLengthWalker 
///     CSeqDB_VolumeLengthWalker 
///     CSeqDB_MembBitWalker 
/// 
/// Implemented for: UNIX, MS-Windows

#include <ncbi_pch.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbifile.hpp>
#include <algorithm>
#include <sstream>

#include "seqdbalias.hpp"
#include "seqdbfile.hpp"

BEGIN_NCBI_SCOPE

/// Public Constructor
///
/// This is the user-visible constructor, which builds the top level
/// node in the dbalias node tree.  This design effectively treats the
/// user-input database list as if it were an alias file containing
/// only the DBLIST specification.

CSeqDBAliasNode::CSeqDBAliasNode(CSeqDBAtlas    & atlas,
                                 const string   & dbname_list,
                                 char             prot_nucl)
    : m_Atlas(atlas)
{
    CSeqDBLockHold locked(atlas);
    
    m_Values["DBLIST"] = dbname_list;
    
    NStr::Tokenize(dbname_list, " ", m_DBList, NStr::eMergeDelims);
    x_ResolveNames(prot_nucl, locked);
    
    CSeqDBAliasStack recurse;
    
    x_ExpandAliases("-", prot_nucl, recurse, locked);
    
    m_Atlas.Unlock(locked);
    
    _ASSERT(recurse.Size() == 0);
}

// Private Constructor
// 
// This is the constructor for nodes other than the top-level node.
// As such it is private and only called from this class.
// 
// This constructor constructs subnodes by calling x_ExpandAliases,
// which calls this constructor again with the subnode's arguments.
// But no node should be its own ancestor.  To prevent this kind of
// recursive loop, each file adds its full path to a stack of strings
// and will not create a subnode for any path already in that set.

CSeqDBAliasNode::CSeqDBAliasNode(CSeqDBAtlas      & atlas,
                                 const string     & dbpath,
                                 const string     & dbname,
                                 char               prot_nucl,
                                 CSeqDBAliasStack & recurse,
                                 CSeqDBLockHold   & locked)
    : m_Atlas(atlas),
      m_DBPath(dbpath)
{
    string full_filename( x_MkPath(m_DBPath, dbname, prot_nucl) );
    
    recurse.Push(full_filename);
    
    x_ReadValues(full_filename, locked);
    NStr::Tokenize(m_Values["DBLIST"], " ", m_DBList, NStr::eMergeDelims);
    x_ExpandAliases(dbname, prot_nucl, recurse, locked);
    
    recurse.Pop();
}

void CSeqDBAliasNode::x_ResolveNames(char prot_nucl, CSeqDBLockHold & locked)
{
    m_DBPath = ".";
    
    size_t i = 0;
    
    for(i = 0; i < m_DBList.size(); i++) {
        string resolved_name =
            SeqDB_FindBlastDBPath(m_DBList[i],
                                  prot_nucl,
                                  0,
                                  false,
                                  m_Atlas,
                                  locked);
        
        if (resolved_name.empty()) {
            // Do over (to get the search path)
            
            string search_path;
            SeqDB_FindBlastDBPath(m_DBList[i],
                                  prot_nucl,
                                  & search_path,
                                  false,
                                  m_Atlas,
                                  locked);
            
            string msg("No alias or index file found for component [");
            msg += m_DBList[i] + "] in search path [" + search_path + "]";
            
            NCBI_THROW(CSeqDBException,
                       eFileErr,
                       msg);
        } else {
            m_DBList[i].swap(resolved_name);
        }
    }
    
    // Everything from here depends on m_DBList[0] existing.
    if (m_DBList.empty())
        return;
    
    size_t common = m_DBList[0].size();
    
    // Reduce common length to length of min db path.
    
    for(i = 1; common && (i < m_DBList.size()); i++) {
        if (m_DBList[i].size() < common) {
            common = m_DBList.size();
        }
    }
    
    if (common) {
        --common;
    }
    
    // Reduce common length to largest universal prefix.
    
    string & first = m_DBList[0];
    
    for(i = 1; common && (i < m_DBList.size()); i++) {
        // Reduce common prefix length until match is found.
        
        while(memcmp(first.c_str(), m_DBList[i].c_str(), common)) {
            --common;
        }
    }
    
    // Adjust back to whole path component.
    
    while(common && (first[common] != CFile::GetPathSeparator())) {
        --common;
    }
    
    if (common) {
        // Factor out common path components.
        
        m_DBPath.assign(first, 0, common);
        
        for(i = 0; i < m_DBList.size(); i++) {
            m_DBList[i].erase(0, common+1);
        }
    }
}

void CSeqDBAliasNode::x_ReadLine(const char * bp,
                                 const char * ep)
{
    const char * p = bp;
    
    // If first nonspace char is '#', line is a comment, so skip.
    if (*p == '#') {
        return;
    }
    
    // Find name
    const char * spacep = p;
    
    while((spacep < ep) && ((*spacep != ' ') && (*spacep != '\t')))
        spacep ++;
    
    string name(p, spacep);
    
    // Skip spaces, tabs, to find value
    while((spacep < ep) && ((*spacep == ' ') || (*spacep == '\t')))
        spacep ++;
    
    // Strip spaces, tabs from end
    while((spacep < ep) && ((ep[-1] == ' ') || (ep[-1] == '\t')))
        ep --;
    
    string value(spacep, ep);
    
    for(size_t i = 0; i<value.size(); i++) {
        if (value[i] == '\t') {
            value[i] = ' ';
        }
    }
    
    // Store in this nodes' dictionary.
    m_Values[name] = value;
}

void CSeqDBAliasNode::x_ReadValues(const string   & fname,
                                   CSeqDBLockHold & locked)
{
    m_Atlas.Lock(locked);
    
    CSeqDBAtlas::TIndx file_length(0);
    
    const char * bp = m_Atlas.GetFile(fname, file_length, locked);
    const char * ep = bp + file_length;
    const char * p  = bp;
    
    // Existence should already be verified.
    _ASSERT(bp);
    
    while(p < ep) {
        // Skip spaces
        while((p < ep) && (*p == ' ')) {
            p++;
        }
        
        const char * eolp = p;
        
        while((eolp < ep) && (*eolp != '\n')) {
            eolp++;
        }
        
        // Non-empty line, so read it.
        if (eolp != p) {
            x_ReadLine(p, eolp);
        }
        
        p = eolp + 1;
    }
    
    m_Atlas.RetRegion(bp);
}

void CSeqDBAliasNode::x_ExpandAliases(const string     & this_name,
                                      char               prot_nucl,
                                      CSeqDBAliasStack & recurse,
                                      CSeqDBLockHold   & locked)
{
    if (m_DBList.empty()) {
        string situation;
        
        if (this_name == "-") {
            situation = "passed to CSeqDB::CSeqDB().";
        } else {
            situation = string("found in alias file [") + this_name + "].";
        }
        
        NCBI_THROW(CSeqDBException,
                   eArgErr,
                   string("No database names were ") + situation);
    }
    
    for(size_t i = 0; i<m_DBList.size(); i++) {
        if (m_DBList[i] == SeqDB_GetBaseName(this_name)) {
            // If the base name of the alias file is also listed in
            // "dblist", it is assumed to refer to a volume instead of
            // to itself.
            
            m_VolNames.push_back(this_name);
            continue;
        }
        
        string new_db_loc( x_MkPath(m_DBPath, m_DBList[i], prot_nucl) );
        
        if (recurse.Exists(new_db_loc)) {
            NCBI_THROW(CSeqDBException,
                       eFileErr,
                       "Illegal configuration: DB alias files are mutually recursive.");
        }
        
        if ( m_Atlas.DoesFileExist(new_db_loc, locked) ) {
            string newpath = SeqDB_GetDirName(new_db_loc);
            string newfile = SeqDB_GetBaseName(new_db_loc);
            
            CRef<CSeqDBAliasNode>
                subnode( new CSeqDBAliasNode(m_Atlas,
                                             newpath,
                                             newfile,
                                             prot_nucl,
                                             recurse,
                                             locked) );
            
            m_SubNodes.push_back(subnode);
        } else {
            // If the name is not found as an alias file, it is
            // considered to be a volume.
            
            m_VolNames.push_back( SeqDB_GetBasePath(new_db_loc) );
        }
    }
}

void CSeqDBAliasNode::GetVolumeNames(vector<string> & vols) const
{
    set<string> volset;
    x_GetVolumeNames(volset);
    
    vols.clear();
    ITERATE(set<string>, iter, volset) {
        vols.push_back(*iter);

    }
    
    // Sort to insure deterministic order.
    sort(vols.begin(), vols.end());
}

void CSeqDBAliasNode::x_GetVolumeNames(set<string> & vols) const
{
    vols.insert(m_VolNames.begin(), m_VolNames.end());
    
    ITERATE(TSubNodeList, iter, m_SubNodes) {
        (*iter)->x_GetVolumeNames(vols);
    }
}


/// Walker for TITLE field of alias file
///
/// The TITLE field of the alias file is a string describing the set
/// of sequences collected by that file.  The title is reported via
/// the "CSeqDB::GetTitle()" method.

class CSeqDB_TitleWalker : public CSeqDB_AliasWalker {
public:
    /// This provides the alias file key used for this field.
    virtual const char * GetFileKey() const
    {
        return "TITLE";
    }
    
    /// Collect data from a volume
    ///
    /// If the TITLE field is not specified in an alias file, we can
    /// use the title(s) in the database volume(s).  Values from alias
    /// node tree siblings are concatenated with "; " used as a
    /// delimiter.
    ///
    /// @param vol
    ///   A database volume
    virtual void Accumulate(const CSeqDBVol & vol)
    {
        AddString( vol.GetTitle() );
    }
    
    /// Collect data from an alias file
    ///
    /// If the TITLE field is specified in an alias file, it will be
    /// used unmodified.  Values from alias node tree siblings are
    /// concatenated with "; " used as a delimiter.
    ///
    /// @param value
    ///   A database volume
    virtual void AddString(const string & value)
    {
        SeqDB_JoinDelim(m_Value, value, "; ");
    }
    
    /// Returns the database title string.
    string GetTitle()
    {
        return m_Value;
    }
    
private:
    /// The title string we are accumulating.
    string m_Value;
};


// A slightly more clever approach (might) track the contributions
// from each volume or alias file and trim the final total by the
// amount of provable overcounting detected.
// 
// Since this is probably impossible in most cases, it is not done.
// The working assumption then is that the specified databases are
// disjoint.  This design should prevent undercounting but allows
// overcounting in some cases.


/// Walker for MAX_SEQ_LENGTH field of alias file
///
/// This functor encapsulates the specifics of the MAX_SEQ_LENGTH
/// field of the alias file.  The NSEQ fields specifies the number of
/// sequences to use when reporting information via the
/// "CSeqDB::GetNumSeqs()" method.  It is not the same as the number
/// of OIDs unless there are no filtering mechanisms in use.

class CSeqDB_MaxLengthWalker : public CSeqDB_AliasWalker {
public:
    /// Constructor
    CSeqDB_MaxLengthWalker()
    {
        m_Value = 0;
    }
    
    /// This provides the alias file key used for this field.
    virtual const char * GetFileKey() const
    {
        return "MAX_SEQ_LENGTH";
    }
    
    /// Collect data from the volume
    ///
    /// If the MAX_SEQ_LENGTH field is not specified in an alias file,
    /// the maximum values of all contributing volumes is used.
    ///
    /// @param vol
    ///   A database volume
    virtual void Accumulate(const CSeqDBVol & vol)
    {
        int new_max = vol.GetMaxLength();
        
        if (new_max > m_Value)
            m_Value = new_max;
    }
    
    /// Collect data from an alias file
    ///
    /// Values from alias node tree siblings are compared, and the
    /// maximum value is used as the result.
    ///
    /// @param value
    ///   A database volume
    virtual void AddString(const string & value)
    {
        int new_max = NStr::StringToUInt(value);
        
        if (new_max > m_Value)
            m_Value = new_max;
    }
    
    /// Returns the maximum sequence length.
    int GetMaxLength()
    {
        return m_Value;
    }
    
private:
    /// The maximum sequence length.
    int m_Value;
};


/// Walker for NSEQ field of alias file
///
/// The NSEQ field of the alias file specifies the number of sequences
/// to use when reporting information via the "CSeqDB::GetNumSeqs()"
/// method.  It is not the same as the number of OIDs unless there are
/// no filtering mechanisms in use.

class CSeqDB_NSeqsWalker : public CSeqDB_AliasWalker {
public:
    /// Constructor
    CSeqDB_NSeqsWalker()
    {
        m_Value = 0;
    }
    
    /// This provides the alias file key used for this field.
    virtual const char * GetFileKey() const
    {
        return "NSEQ";
    }
    
    /// Collect data from the volume
    ///
    /// If the NSEQ field is not specified in an alias file, the
    /// number of OIDs in the volume is used instead.
    ///
    /// @param vol
    ///   A database volume
    virtual void Accumulate(const CSeqDBVol & vol)
    {
        m_Value += vol.GetNumOIDs();
    }
    
    /// Collect data from an alias file
    ///
    /// If the NSEQ field is specified in an alias file, it will be
    /// used.  Values from alias node tree siblings are summed.
    ///
    /// @param value
    ///   A database volume
    virtual void AddString(const string & value)
    {
        m_Value += NStr::StringToUInt(value);
    }
    
    /// Returns the accumulated number of OIDs.
    int GetNum() const
    {
        return m_Value;
    }
    
private:
    /// The accumulated number of OIDs.
    int m_Value;
};


/// Walker for OID count accumulation.
///
/// The number of OIDs should be like the number of sequences, but
/// without the value adjustments made by alias files.  To preserve
/// this relationship, this class inherits from CSeqDB_NSeqsWalker.

class CSeqDB_NOIDsWalker : public CSeqDB_NSeqsWalker {
public:
    /// This disables the key; the spaces would not be preserved, so
    /// this is a non-matchable string in this context.
    virtual const char * GetFileKey() const
    {
        return " no key ";
    }
};


/// Walker for total length accumulation.
///
/// The total length of the database is the sum of the lengths of all
/// volumes of the database (measured in bases).

class CSeqDB_TotalLengthWalker : public CSeqDB_AliasWalker {
public:
    /// Constructor
    CSeqDB_TotalLengthWalker()
    {
        m_Value = 0;
    }
    
    /// This provides the alias file key used for this field.
    virtual const char * GetFileKey() const
    {
        return "LENGTH";
    }
    
    /// Collect data from the volume
    ///
    /// If the LENGTH field is not specified in an alias file, the
    /// sum of the volume lengths will be used.
    ///
    /// @param vol
    ///   A database volume
    virtual void Accumulate(const CSeqDBVol & vol)
    {
        m_Value += vol.GetVolumeLength();
    }
    
    /// Collect data from an alias file
    ///
    /// If the LENGTH field is specified in an alias file, it will be
    /// used.  Values from alias node tree siblings are summed.
    ///
    /// @param value
    ///   A database volume
    virtual void AddString(const string & value)
    {
        m_Value += NStr::StringToUInt8(value);
    }
    
    /// Returns the accumulated volume length.
    Uint8 GetLength() const
    {
        return m_Value;
    }
    
private:
    /// The accumulated volume length.
    Uint8 m_Value;
};


/// Walker for volume length accumulation.
///
/// The volume length should be like total length, but without the
/// value adjustments made by alias files.  To preserve this
/// relationship, this class inherits from CSeqDB_TotalLengthWalker.

class CSeqDB_VolumeLengthWalker : public CSeqDB_TotalLengthWalker {
public:
    /// This disables the key; the spaces would not be preserved, so
    /// this is a non-matchable string in this context.
    virtual const char * GetFileKey() const
    {
        return " no key ";
    }
};


/// Walker for membership bit
///
/// This just searches alias files for the membership bit if one is
/// specified.

class CSeqDB_MembBitWalker : public CSeqDB_AliasWalker {
public:
    /// Constructor
    CSeqDB_MembBitWalker()
    {
        m_Value = 0;
    }
    
    /// This provides the alias file key used for this field.
    virtual const char * GetFileKey() const
    {
        return "MEMB_BIT";
    }
    
    /// Collect data from the volume
    ///
    /// If the MEMB_BIT field is not specified in an alias file, then
    /// it is not needed.  This field is intended to allow filtration
    /// of deflines by taxonomic category, which is only needed if an
    /// alias file reduces the taxonomic scope.
    virtual void Accumulate(const CSeqDBVol &)
    {
        // Volumes don't have this data, only alias files.
    }
    
    /// Collect data from an alias file
    ///
    /// If the MEMB_BIT field is specified in an alias file, it will
    /// be used unmodified.  No attempt is made to combine or collect
    /// bit values - currently, only one can be used at a time.
    ///
    /// @param value
    ///   A database volume
    virtual void AddString(const string & value)
    {
        m_Value = NStr::StringToUInt(value);
    }
    
    /// Returns the membership bit.
    int GetMembBit() const
    {
        return m_Value;
    }
    
private:
    /// The membership bit.
    int m_Value;
};


void
CSeqDBAliasNode::WalkNodes(CSeqDB_AliasWalker * walker,
                           const CSeqDBVolSet & volset) const
{
    TVarList::const_iterator value =
        m_Values.find(walker->GetFileKey());
    
    if (value != m_Values.end()) {
        walker->AddString( (*value).second );
        return;
    }
    
    ITERATE(TSubNodeList, node, m_SubNodes) {
        (*node)->WalkNodes( walker, volset );
    }
    
    ITERATE(TVolNames, volname, m_VolNames) {
        if (const CSeqDBVol * vptr = volset.GetVol(*volname)) {
            walker->Accumulate( *vptr );
        }
    }
}

void CSeqDBAliasNode::x_SetOIDMask(CSeqDBVolSet & volset,
                                   int            begin,
                                   int            end,
                                   const string & oidfile)
{
    if (m_DBList.size() != 1) {
        ostringstream oss;
        
        oss << "Alias file (" << m_DBPath << ") uses oid list ("
            << m_Values["OIDLIST"] << ") but has " << m_DBList.size()
            << " volumes (" << NStr::Join(m_DBList, " ") << ").";
        
        NCBI_THROW(CSeqDBException, eFileErr, oss.str());
    }
    
    string vol_path(SeqDB_CombinePath(m_DBPath, m_DBList[0]));
    string mask_path(SeqDB_CombinePath(m_DBPath, oidfile));
    
    volset.AddMaskedVolume(vol_path, mask_path, begin, end);
}

void CSeqDBAliasNode::x_SetGiListMask(CSeqDBVolSet & volset,
                                      int            begin,
                                      int            end,
                                      const string & gilist)
{
    string resolved_gilist;
    
    vector<string> gils;
    NStr::Tokenize(gilist, " ", gils, NStr::eMergeDelims);
    
    // This enforces our restriction that only one GILIST may be
    // applied to any particular volume.  We do not check if the
    // existing one is the same as what we have...
    
    if (gils.size() != 1) {
        string msg =
            string("Alias file (") + m_DBPath +
            ") has multiple GI lists (" + m_Values["GILIST"] + ").";
        
        NCBI_THROW(CSeqDBException, eFileErr, msg);
    }
    
    ITERATE(vector<string>, iter, gils) {
        SeqDB_JoinDelim(resolved_gilist,
                        SeqDB_CombinePath(m_DBPath, *iter),
                        " ");
    }
    
    ITERATE(TVolNames, vn, m_VolNames) {
        volset.AddGiListVolume(*vn, resolved_gilist, begin, end);
    }
    
    // This "join" should not be needed - an assignment would be just
    // as good - as long as the multi-gilist exception above is
    // in force.
    
    NON_CONST_ITERATE(TSubNodeList, an, m_SubNodes) {
        SeqDB_JoinDelim((**an).m_Values["GILIST"],
                        resolved_gilist,
                        " ");
    }
    
    NON_CONST_ITERATE(TSubNodeList, an, m_SubNodes) {
        (**an).SetMasks( volset );
    }
}

void CSeqDBAliasNode::x_SetOIDRange(CSeqDBVolSet & volset, int begin, int end)
{
    if (m_DBList.size() != 1) {
        ostringstream oss;
        
        oss << "Alias file (" << m_DBPath
            << ") uses oid range (" << (begin + 1) << "," << end
            << ") but has " << m_DBList.size()
            << " volumes (" << NStr::Join(m_DBList, " ") << ").";
        
        NCBI_THROW(CSeqDBException, eFileErr, oss.str());
    }
    
    string vol_path(SeqDB_CombinePath(m_DBPath, m_DBList[0]));
    
    volset.AddRangedVolume(vol_path, begin, end);
}

void CSeqDBAliasNode::SetMasks(CSeqDBVolSet & volset)
{
    TVarList::iterator gil_iter   = m_Values.find(string("GILIST"));
    TVarList::iterator oid_iter   = m_Values.find(string("OIDLIST"));
    TVarList::iterator f_oid_iter = m_Values.find(string("FIRST_OID"));
    TVarList::iterator l_oid_iter = m_Values.find(string("LAST_OID"));
    
    bool filtered = false;
    
    if (! m_DBList.empty()) {
        if (oid_iter   != m_Values.end() ||
            gil_iter   != m_Values.end() ||
            f_oid_iter != m_Values.end() ||
            l_oid_iter != m_Values.end()) {
            
            int first_oid = 0;
            int last_oid  = INT_MAX;
            
            if (f_oid_iter != m_Values.end()) {
                first_oid = NStr::StringToUInt(f_oid_iter->second);
                
                // Starts at one, adjust to zero-indexed.
                if (first_oid)
                    first_oid--;
            }
            
            if (l_oid_iter != m_Values.end()) {
                // Zero indexing and post notation adjustments cancel.
                last_oid = NStr::StringToUInt(l_oid_iter->second);
            }
            
            if (oid_iter != m_Values.end()) {
                x_SetOIDMask(volset, first_oid, last_oid, oid_iter->second);
                filtered = true;
            }
            
            if (gil_iter != m_Values.end()) {
                x_SetGiListMask(volset, first_oid, last_oid, gil_iter->second);
                filtered = true;
            }
            
            if (! filtered) {
                x_SetOIDRange(volset, first_oid, last_oid);
                filtered = true;
            }
        }
    }
    
    if (filtered) {
        return;
    }
    
    NON_CONST_ITERATE(TSubNodeList, sn, m_SubNodes) {
        (**sn).SetMasks(volset);
    }
    
    ITERATE(TVolNames, vn, m_VolNames) {
        if (CSeqDBVol * vptr = volset.GetVol(*vn)) {
            // We did NOT find an OIDLIST entry; therefore, any db
            // volumes mentioned here are included unfiltered.
            
            volset.AddFullVolume(vptr->GetVolName());
        }
    }
}

string CSeqDBAliasNode::GetTitle(const CSeqDBVolSet & volset) const
{
    CSeqDB_TitleWalker walk;
    WalkNodes(& walk, volset);
    
    return walk.GetTitle();
}

int CSeqDBAliasNode::GetNumSeqs(const CSeqDBVolSet & vols) const
{
    CSeqDB_NSeqsWalker walk;
    WalkNodes(& walk, vols);
    
    return walk.GetNum();
}

int CSeqDBAliasNode::GetNumOIDs(const CSeqDBVolSet & vols) const
{
    CSeqDB_NOIDsWalker walk;
    WalkNodes(& walk, vols);
    
    return walk.GetNum();
}

Uint8 CSeqDBAliasNode::GetTotalLength(const CSeqDBVolSet & volset) const
{
    CSeqDB_TotalLengthWalker walk;
    WalkNodes(& walk, volset);
    
    return walk.GetLength();
}

Uint8 CSeqDBAliasNode::GetVolumeLength(const CSeqDBVolSet & volset) const
{
    CSeqDB_VolumeLengthWalker walk;
    WalkNodes(& walk, volset);
    
    return walk.GetLength();
}

int CSeqDBAliasNode::GetMembBit(const CSeqDBVolSet & volset) const
{
    CSeqDB_MembBitWalker walk;
    WalkNodes(& walk, volset);
    
    return walk.GetMembBit();
}

END_NCBI_SCOPE

