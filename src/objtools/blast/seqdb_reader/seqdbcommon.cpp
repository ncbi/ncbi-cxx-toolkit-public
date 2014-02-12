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

/// @file seqdbcommon.cpp
/// Definitions of various helper functions for SeqDB.

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include <corelib/metareg.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbifile.hpp>
#include <objtools/blast/seqdb_reader/seqdbcommon.hpp>
#include <util/sequtil/sequtil.hpp>
#include <util/sequtil/sequtil_convert.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/general/general__.hpp>
#include <objtools/blast/seqdb_reader/impl/seqdbgeneral.hpp>
#include <objtools/blast/seqdb_reader/impl/seqdbatlas.hpp>
#include <algorithm>

BEGIN_NCBI_SCOPE

const string kSeqDBGroupAliasFileName("index.alx");

CSeqDB_Substring SeqDB_RemoveDirName(CSeqDB_Substring s)
{
    int off = s.FindLastOf(CFile::GetPathSeparator());
    
    if (off != -1) {
        s.EraseFront(off + 1);
    }
    
    return s;
}


CSeqDB_Substring SeqDB_RemoveFileName(CSeqDB_Substring s)
{
    int off = s.FindLastOf(CFile::GetPathSeparator());
    
    if (off != -1) {
        s.Resize(off);
    } else {
        s.Clear();
    }
    
    return s;
}


CSeqDB_Substring SeqDB_RemoveExtn(CSeqDB_Substring s)
{
    // This used to remove anything after the last "." it could find.
    // Then it was changed to only remove the part after the ".", if
    // it did not contain a "/" character.
    
    // Now it has been made even stricter, it looks for something like
    // "(.*)([.][a-zA-Z]{3})" and removes the second sub-expression if
    // there is a match.  This is because of mismatches like "1234.00"
    // that are not real "file extensions" in the way that SeqDB wants
    // to process them.
    
    int slen = s.Size();
    
    if (slen > 4) {
        string extn(s.GetEnd()-4, s.GetEnd());
        string extn2(extn, 2, 4);
        // Of course, nal and pal are not the only valid
        // extensions, but this code is only used with these two,
        // as far as I know, at this moment in time.
            
        if (extn[0] == '.'     &&
            (extn[1] == 'n' || extn[1] == 'p') &&
            (extn2 == "al"  || extn2 == "in")) {
            /*
            isalpha( s[slen-3] ) &&
            isalpha( s[slen-2] ) &&
            isalpha( s[slen-1] )) {*/
            
            s.Resize(slen - 4);
        }
    }
    
    return s;
}


bool SeqDB_SplitString(CSeqDB_Substring & buffer,
                       CSeqDB_Substring & front,
                       char               delim)
{
    for(int i = 0; i < buffer.Size(); i++) {
        if (buffer[i] == delim) {
            front = buffer;
            
            buffer.EraseFront(i + 1);
            front.Resize(i);
            
            return true;
        }
    }
    return false;
}


void SeqDB_CombinePath(const CSeqDB_Substring & one,
                       const CSeqDB_Substring & two,
                       const CSeqDB_Substring * extn,
                       string                 & outp)
{
    char delim = CFile::GetPathSeparator();
    
    int extn_amt = extn ? (extn->Size()+1) : 0;
    
    if (two.Empty()) {
        // We only use the extension if there is a filename.
        one.GetString(outp);
        return;
    }
    
    bool only_two = false;
    
    if (one.Empty() || two[0] == delim) {
        only_two = true;
    }

    // Drive letter test for CP/M derived systems
    if (delim == '\\'   &&
        two.Size() > 3  &&
        isalpha(two[0]) &&
        two[1] == ':'   &&
        two[2] == '\\') {
        
        only_two = true;
    }
    
    if (only_two) {
        outp.reserve(two.Size() + extn_amt);
        two.GetString(outp);
        
        if (extn) {
            outp.append(".");
            outp.append(extn->GetBegin(), extn->GetEnd());
        }
        return;
    }
    
    outp.reserve(one.Size() + two.Size() + 1 + extn_amt);
    
    one.GetString(outp);
    
    if (outp[outp.size() - 1] != delim) {
        outp += delim;
    }
    
    outp.append(two.GetBegin(), two.GetEnd());
    
    if (extn) {
        outp.append(".");
        outp.append(extn->GetBegin(), extn->GetEnd());
    }
}


bool SeqDB_CompareVolume(const string & s1, const string & s2)
{
    string x1, x2;
    CSeqDB_Path(s1).FindBaseName().GetString(x1);
    CSeqDB_Path(s2).FindBaseName().GetString(x2);
    if (x1 != x2) return (x1 < x2);
    else return (s1 < s2);
} 

/// File existence test interface.
class CSeqDB_FileExistence {
public:
    /// Destructor
    virtual ~CSeqDB_FileExistence()
    {
    }
    
    /// Check if file exists at fully qualified path.
    /// @param fname Filename.
    /// @return True if the file was found.
    virtual bool DoesFileExist(const string & fname) = 0;
};


/// Test whether an index or alias file exists
///
/// The provide filename is combined with both of the extensions
/// appropriate to the database sequence type, and the resulting
/// strings are checked for existence in the file system.  The
/// 'access' object defines how to check file existence.
///
/// @param dbname
///   Input path and filename
/// @param dbtype
///   Database type, either protein or nucleotide
/// @param access
///   The file access object.
/// @param linkoutdb_search
///   Determines whether linkoutdb files should be searched for
/// @return
///   true if either of the index or alias files is found

static bool s_SeqDB_DBExists(const string         & dbname,
                             char                   dbtype,
                             CSeqDB_FileExistence & access,
                             bool linkoutdb_search)
{
    string path;
    path.reserve(dbname.size() + 4);
    path.assign(dbname.data(), dbname.data() + dbname.size());
    
    if (linkoutdb_search) {
        _ASSERT(dbtype == 'p');
        path.append(".p");
        vector<string> extn;
        extn.reserve(4);
        extn.push_back("ni");
        extn.push_back("nd");
        extn.push_back("si");
        extn.push_back("sd");
        ITERATE(vector<string>, e, extn) {
            string candidate(path + *e);
            if (access.DoesFileExist(candidate)) {
                return true;
            }
        }
    } else {
        path.append(".-al");
        
        path[path.size()-3] = dbtype;
        
        if (access.DoesFileExist(path)) {
            return true;
        }
        
        path[path.size()-2] = 'i';
        path[path.size()-1] = 'n';
        
        if (access.DoesFileExist(path)) {
            return true;
        }
    }
    
    return false;
}


/// Returns the character used to seperate path components in the
/// current operating system or platform.
static string s_GetPathSplitter()
{
    const char * splitter = 0;
    
#if defined(NCBI_OS_UNIX)
    splitter = ":";
#else
    splitter = ";";
#endif
    
    return splitter;
}


void SeqDB_ConvertOSPath(string & dbs)
{
    // See also CDirEntry::ConvertToOSPath()
    
    char delim = CDirEntry::GetPathSeparator();
    
    for(size_t i = 0; i<dbs.size(); i++) {
        if (dbs[i] == '/' || dbs[i] == '\\') {
            dbs[i] = delim;
        }
    }
}


string SeqDB_MakeOSPath(const string & dbs)
{
    string cvt(dbs);
    SeqDB_ConvertOSPath(cvt);
    return cvt;
}


/// Search for a file in a provided set of paths
/// 
/// This function takes a search path as a ":" delimited set of path
/// names, and searches in those paths for the given database
/// component.  The component name may include path components.  If
/// the exact flag is set, the path is assumed to contain any required
/// extension; otherwise extensions for index and alias files will be
/// tried.  Each element of the search path is tried in sequential
/// order for both index or alias files (if exact is not set), before
/// moving to the next element of the search path.  The path returned
/// from this function will not contain a file extension unless the
/// provided filename did (in which case, exact is normally set).
/// 
/// @param blast_paths
///   List of filesystem paths seperated by ":".
/// @param dbname
///   Base name of the database index or alias file to search for.
/// @param dbtype
///   Type of database, either protein or nucleotide.
/// @param exact
///   Set to true if dbname already contains any needed extension.
/// @param linkoutdb_search
///   Determines whether linkoutdb files should be searched for
/// @return
///   Full pathname, minus extension, or empty string if none found.

static string s_SeqDB_TryPaths(const string         & blast_paths,
                               const string         & dbname,
                               char                   dbtype,
                               bool                   exact,
                               CSeqDB_FileExistence & access,
                               bool                   linkoutdb_search = false)
{
    // 1. If this was a vector<CSeqDB_Substring>, the tokenize would
    //    not need to do any allocations (but would need rewriting).
    //
    // 2. If this was split into several functions, and/or a stateful
    //    class was used, this would perform better here, and would
    //    allow improvement of the search routine for combined group
    //    indices (see comments in CSeqDBAliasSets::FindAliasPath).
    
    vector<string> roads;
    NStr::Tokenize(blast_paths, s_GetPathSplitter(), roads, NStr::eMergeDelims);
    
    string result;
    string attempt;
    
    ITERATE(vector<string>, road, roads) {
        attempt.erase();
        
        SeqDB_CombinePath(CSeqDB_Substring(SeqDB_MakeOSPath(*road)),
                          CSeqDB_Substring(dbname),
                          0,
                          attempt);
        
        if (exact) {
            if (access.DoesFileExist(attempt)) {
                result = attempt;
                break;
            }
        } else {
            if (s_SeqDB_DBExists(attempt, dbtype, access, linkoutdb_search)) {
                result = attempt;
                break;
            }
        }
    }
    
    return result;
}

static string
s_SeqDB_FindBlastDBPath(const string         & dbname,
                        char                   dbtype,
                        string               * sp,
                        bool                   exact,
                        CSeqDB_FileExistence & access,
                        const string           path="")
{
    const string pathology = (path=="") ? CSeqDBAtlas::GenerateSearchPath() : path;
    
    if (sp) {
        *sp = pathology;
    }
    
    return s_SeqDB_TryPaths(pathology, dbname, dbtype, exact, access);
}

/// Check file existence using CSeqDBAtlas.
class CSeqDB_AtlasAccessor : public CSeqDB_FileExistence {
public:
    /// Constructor.
    CSeqDB_AtlasAccessor(CSeqDBAtlas    & atlas,
                         CSeqDBLockHold & locked)
        : m_Atlas  (atlas),
          m_Locked (locked)
    {
    }
    
    /// Test file existence.
    /// @param fname Fully qualified name of file for which to look.
    /// @return True iff file exists.
    virtual bool DoesFileExist(const string & fname)
    {
        return m_Atlas.DoesFileExist(fname, m_Locked);
    }
    
private:
    CSeqDBAtlas    & m_Atlas;
    CSeqDBLockHold & m_Locked;
};


string SeqDB_FindBlastDBPath(const string   & dbname,
                             char             dbtype,
                             string         * sp,
                             bool             exact,
                             CSeqDBAtlas    & atlas,
                             CSeqDBLockHold & locked)
{
    CSeqDB_AtlasAccessor access(atlas, locked);

    return s_SeqDB_FindBlastDBPath(dbname,
                                   dbtype,
                                   sp,
                                   exact,
                                   access,
                                   atlas.GetSearchPath());
}


/// Check file existence using CFile.
class CSeqDB_SimpleAccessor : public CSeqDB_FileExistence {
public:
    /// Constructor.
    CSeqDB_SimpleAccessor()
    {
    }
    
    /// Test file existence.
    /// @param fname Fully qualified name of file for which to look.
    /// @return True iff file exists.
    virtual bool DoesFileExist(const string & fname)
    {
        // Use the same criteria as the Atlas code would.
        CFile whole(SeqDB_MakeOSPath(fname));
        return whole.GetLength() != (Int8) -1;
    }
};


string SeqDB_ResolveDbPath(const string & filename)
{
    CSeqDB_SimpleAccessor access;
    
    return s_SeqDB_FindBlastDBPath(filename,
                                   '-',
                                   0,
                                   true,
                                   access);
}

string SeqDB_ResolveDbPathNoExtension(const string & filename, 
                                      char dbtype /* = '-' */)
{
    CSeqDB_SimpleAccessor access;
    
    return s_SeqDB_FindBlastDBPath(filename, dbtype, 0, false, access);
}

string SeqDB_ResolveDbPathForLinkoutDB(const string & filename)
{
    const char dbtype('p'); // this is determined by blastdb_links application
    CSeqDB_SimpleAccessor access;
    const string pathology = CSeqDBAtlas::GenerateSearchPath();
    return s_SeqDB_TryPaths(pathology, filename, dbtype, false, access, true);
}

void SeqDB_JoinDelim(string & a, const string & b, const string & delim)
{
    if (b.empty()) {
        return;
    }
    
    if (a.empty()) {
        // a has no size - but might have capacity
        s_SeqDB_QuickAssign(a, b);
        return;
    }
    
    size_t newlen = a.length() + b.length() + delim.length();
    
    if (a.capacity() < newlen) {
        size_t newcap = 16;
        
        while(newcap < newlen) {
            newcap <<= 1;
        }
        
        a.reserve(newcap);
    }
    
    a += delim;
    a += b;
}


CSeqDBGiList::CSeqDBGiList()
    : m_CurrentOrder(eNone)
{
}


/// Compare SGiOid structs by OID.
class CSeqDB_SortOidLessThan {
public:
    /// Test whether lhs is less than (occurs before) rhs.
    /// @param lhs Left hand side of less-than operator. [in]
    /// @param rhs Right hand side of less-than operator. [in]
    /// @return True if lhs has a lower OID than rhs.
    int operator()(const CSeqDBGiList::SGiOid & lhs,
                   const CSeqDBGiList::SGiOid & rhs)
    {
        return lhs.oid < rhs.oid;
    }
};


/// Compare SGiOid structs by GI.
class CSeqDB_SortGiLessThan {
public:
    /// Test whether lhs is less than (occurs before) rhs.
    /// @param lhs Left hand side of less-than operator. [in]
    /// @param rhs Right hand side of less-than operator. [in]
    /// @return True if lhs has a lower GI than rhs.
    int operator()(const CSeqDBGiList::SGiOid & lhs,
                   const CSeqDBGiList::SGiOid & rhs)
    {
        return lhs.gi < rhs.gi;
    }
};


/// Compare SGiOid structs by GI.
class CSeqDB_SortTiLessThan {
public:
    /// Test whether lhs is less than (occurs before) rhs.
    /// @param lhs Left hand side of less-than operator. [in]
    /// @param rhs Right hand side of less-than operator. [in]
    /// @return True if lhs has a lower GI than rhs.
    int operator()(const CSeqDBGiList::STiOid & lhs,
                   const CSeqDBGiList::STiOid & rhs)
    {
        return lhs.ti < rhs.ti;
    }
};


/// Compare SSeqIdOid structs by SeqId.
class CSeqDB_SortSiLessThan {
public:
    /// Test whether lhs is less than (occurs before) rhs.
    /// @param lhs Left hand side of less-than operator. [in]
    /// @param rhs Right hand side of less-than operator. [in]
    /// @return True if lhs sorts before rhs by Seq-id.
    int operator()(const CSeqDBGiList::SSiOid & lhs,
                   const CSeqDBGiList::SSiOid & rhs)
    {
        return lhs.si < rhs.si;
    }
};


template<class TCompare, class TVector>
void s_InsureOrder(TVector & v)
{
    bool already = true;
    
    TCompare compare_less;
    
    for(int i = 1; i < (int) v.size(); i++) {
        if (compare_less(v[i], v[i-1])) {
            already = false;
            break;
        }
    }
    
    if (! already) {
        sort(v.begin(), v.end(), compare_less);
    }
}


void CSeqDBGiList::InsureOrder(ESortOrder order)
{
    // Code depends on OID order after translation, because various
    // methods of SeqDB use this class for filtering purposes.
    
    if ((order < m_CurrentOrder) || (order == eNone)) {
        NCBI_THROW(CSeqDBException,
                   eFileErr,
                   "Out of sequence sort order requested.");
    }
    
    // Input is usually sorted by GI, so we first test for sortedness.
    // If it will fail it will probably do so almost immediately.
    
    if (order != m_CurrentOrder) {
        switch(order) {
        case eNone:
            break;
            
        case eGi:
            s_InsureOrder<CSeqDB_SortGiLessThan>(m_GisOids);
            s_InsureOrder<CSeqDB_SortTiLessThan>(m_TisOids);
            s_InsureOrder<CSeqDB_SortSiLessThan>(m_SisOids);
            break;
            
        default:
            NCBI_THROW(CSeqDBException,
                       eFileErr,
                       "Unrecognized sort order requested.");
        }
        
        m_CurrentOrder = order;
    }
}


bool CSeqDBGiList::FindGi(int gi) const
{
    int oid(0), index(0);
    return (const_cast<CSeqDBGiList *>(this))->GiToOid(gi, oid, index);
}


bool CSeqDBGiList::GiToOid(int gi, int & oid)
{
    int index(0);
    return GiToOid(gi, oid, index);
}


bool CSeqDBGiList::GiToOid(int gi, int & oid, int & index)
{
    InsureOrder(eGi);  // would assert be better?
    
    int b(0), e((int)m_GisOids.size());
    
    while(b < e) {
        int m = (b + e)/2;
        int m_gi = m_GisOids[m].gi;
        
        if (m_gi < gi) {
            b = m + 1;
        } else if (m_gi > gi) {
            e = m;
        } else {
            oid = m_GisOids[m].oid;
            index = m;
            return true;
        }
    }
    
    oid = index = -1;
    return false;
}


bool CSeqDBGiList::FindTi(Int8 ti) const
{
    int oid(0), index(0);
    return (const_cast<CSeqDBGiList *>(this))->TiToOid(ti, oid, index);
}


bool CSeqDBGiList::TiToOid(Int8 ti, int & oid)
{
    int index(0);
    return TiToOid(ti, oid, index);
}


bool CSeqDBGiList::TiToOid(Int8 ti, int & oid, int & index)
{
    InsureOrder(eGi);  // would assert be better?
    
    int b(0), e((int)m_TisOids.size());
    
    while(b < e) {
        int m = (b + e)/2;
        Int8 m_ti = m_TisOids[m].ti;
        
        if (m_ti < ti) {
            b = m + 1;
        } else if (m_ti > ti) {
            e = m;
        } else {
            oid = m_TisOids[m].oid;
            index = m;
            return true;
        }
    }
    
    oid = index = -1;
    return false;
}

bool CSeqDBGiList::FindSi(const string &si) const
{
    int oid(0), index(0);
    return (const_cast<CSeqDBGiList *>(this))->SiToOid(si, oid, index);
}

bool CSeqDBGiList::SiToOid(const string &si, int & oid)
{
    int index(0);
    return SiToOid(si, oid, index);
}

bool CSeqDBGiList::SiToOid(const string &si, int & oid, int & index)
{
    InsureOrder(eGi);

    int b(0), e((int)m_SisOids.size());

    while(b < e) {
        int m = (b + e)/2;
        const string & m_si = m_SisOids[m].si;
        
        if (m_si < si) {
            b = m + 1;
        } else if (si < m_si) {
            e = m;
        } else {
            oid = m_SisOids[m].oid;
            index = m;
            return true;
        }
    }
    
    oid = index = -1;
    return false;
}

void
CSeqDBGiList::GetGiList(vector<int>& gis) const
{
    gis.clear();
    gis.reserve(GetNumGis());

    ITERATE(vector<SGiOid>, itr, m_GisOids) {
        gis.push_back(itr->gi);
    }
}


void
CSeqDBGiList::GetTiList(vector<Int8>& tis) const
{
    tis.clear();
    tis.reserve(GetNumTis());

    ITERATE(vector<STiOid>, itr, m_TisOids) {
        tis.push_back(itr->ti);
    }
}


void SeqDB_ReadBinaryGiList(const string & fname, vector<int> & gis)
{
    CMemoryFile mfile(SeqDB_MakeOSPath(fname));
    
    Int4 * beginp = (Int4*) mfile.GetPtr();
    Int4 * endp   = (Int4*) (((char*)mfile.GetPtr()) + mfile.GetSize());
    
    Int4 num_gis = (int)(endp-beginp-2);
    
    gis.clear();
    
    if (((endp - beginp) < 2) ||
        (beginp[0] != -1) ||
        (SeqDB_GetStdOrd(beginp + 1) != num_gis)) {
        NCBI_THROW(CSeqDBException,
                   eFileErr,
                   "Specified file is not a valid binary GI file.");
    }
    
    gis.reserve(num_gis);
    
    for(Int4 * elem = beginp + 2; elem < endp; elem ++) {
        gis.push_back((int) SeqDB_GetStdOrd(elem));
    }
}

/// This function determines whether a file is a valid binary GI/TI file.
/// @param fbeginp pointer to start of file [in]
/// @param fendp pointer to end of file [in]
/// @param has_long_ids will be set to true if the gi file contains long IDs [out]
/// @param has_tis will be set to true if the input file contains Trace IDs,
/// otherwise the file contains GIs [out]
/// @returns true if file is binary
/// @throws CSeqDBException if file is empty or invalid gi file
static 
bool s_SeqDB_IsBinaryNumericList(const char* fbeginp, const char* fendp, 
                                 bool& has_long_ids, bool* has_tis = NULL)
{
    bool retval = false;
    has_long_ids = false;
    if (has_tis) 
        *has_tis = false;
    Int8 file_size = fendp - fbeginp;
    
    if (file_size == 0) {
        NCBI_THROW(CSeqDBException,
                   eFileErr,
                   "Specified file is empty.");
    } else if (isdigit((unsigned char)(*((char*) fbeginp))) ||
               ((unsigned char)(*((char*) fbeginp)) == '#')) {
        retval = false;
    } else if ((file_size >= 8) && ((*fbeginp & 0xFF) == 0xFF)) {
        retval = true;

        int marker = fbeginp[3] & 0xFF;
        
        if (marker == 0xFE || marker == 0xFC) {
            has_long_ids = true;
        }
        if (has_tis && (marker == 0xFD || marker == 0xFC)) {
            *has_tis = true;
        }
    } else {
        NCBI_THROW(CSeqDBException,
                   eFileErr,
                   "Specified file is not a valid GI/TI list.");
    }
    return retval;
}


void SeqDB_ReadMemoryGiList(const char * fbeginp,
                            const char * fendp,
                            vector<CSeqDBGiList::SGiOid> & gis,
                            bool * in_order)
{
    bool long_ids = false;
    Int8 file_size = fendp - fbeginp;
    
    if (s_SeqDB_IsBinaryNumericList(fbeginp, fendp, long_ids)) {
        _ASSERT(long_ids == false);
        Int4 * bbeginp = (Int4*) fbeginp;
        Int4 * bendp = (Int4*) fendp;
        
        Int4 num_gis = (int)(bendp-bbeginp-2);
        
        gis.clear();
        
        if (((bendp - bbeginp) < 2) ||
            (bbeginp[0] != -1) ||
            (SeqDB_GetStdOrd(bbeginp + 1) != num_gis)) {
            NCBI_THROW(CSeqDBException,
                       eFileErr,
                       "Specified file is not a valid binary GI file.");
        }
        
        gis.reserve(num_gis);
        
        if (in_order) {
            int prev_gi =0;
            bool in_gi_order = true;
            
            Int4 * elem = bbeginp + 2;
            while(elem < bendp) {
                int this_gi = (int) SeqDB_GetStdOrd(elem);
                gis.push_back(this_gi);
            
                if (prev_gi > this_gi) {
                    in_gi_order = false;
                    break;
                }
                prev_gi = this_gi;
                elem ++;
            }
            
            while(elem < bendp) {
                gis.push_back((int) SeqDB_GetStdOrd(elem++));
            }
            
            *in_order = in_gi_order;
        } else {
            for(Int4 * elem = bbeginp + 2; elem < bendp; elem ++) {
                gis.push_back((int) SeqDB_GetStdOrd(elem));
            }
        }
    } else {
        _ASSERT(long_ids == false);
        // We would prefer to do only one allocation, so assume
        // average gi is 6 digits plus newline.  A few extra will be
        // allocated, but this is preferable to letting the vector
        // double itself (which it still will do if needed).
        
        gis.reserve(int(file_size / 7));
        
        Uint4 elem(0);
        
        for(const char * p = fbeginp; p < fendp; p ++) {
            Uint4 dig = 0;
            
            switch(*p) {
            case '0':
                dig = 0;
                break;
            
            case '1':
                dig = 1;
                break;
            
            case '2':
                dig = 2;
                break;
            
            case '3':
                dig = 3;
                break;
            
            case '4':
                dig = 4;
                break;
            
            case '5':
                dig = 5;
                break;
            
            case '6':
                dig = 6;
                break;
            
            case '7':
                dig = 7;
                break;
            
            case '8':
                dig = 8;
                break;
            
            case '9':
                dig = 9;
                break;
            
            case '#':
            case '\n':
            case '\r':
                // Skip blank lines or comments by ignoring zero.
                if (elem != 0) {
                    gis.push_back(elem);
                }
                elem = 0;
                continue;
                
            default:
                {
                    string msg = string("Invalid byte in text GI list [") +
                        NStr::UIntToString((unsigned char)(*p)) + " at location " +
                        NStr::NumericToString(p-fbeginp) + "].";
                    NCBI_THROW(CSeqDBException, eFileErr, msg);
                }
            }
            
            elem *= 10;
            elem += dig;
        }
    }
}

// [ NOTE: The 8 byte versions described here are not yet
// implemented. ]
//
// FF..FF = -1 -> GI list <32 bit>
// FF..FE = -2 -> GI list <64 bit>
// FF..FD = -3 -> TI list <32 bit>
// FF..FC = -4 -> TI list <64 bit>
//
// Format of the 8 byte TI list; note that we are still limited to
// 2^32-1 TIs, which would involve an 32 GB identifier list file; this
// code (in its current form) will not work at all on a 32 bit system
// for GI files with more than about 500 megasequences, or TI files
// with more than about 256 megasequences, assuming the current 16
// bytes per vector element.  This is larger than the current total
// number of GI sequences, but not larger than the number of TIs, so a
// TI query for all TIs everywhere will most likely choke on a 32 bit
// system because the data will simply not fit into memory (there are
// nearly that many active TIs and the program will have other memory
// expenditures.)
//
// 4 bytes: FF FF FF F?
// 4 bytes: <number of TIs>
// 8 bytes: TI#0
// 8 bytes: TI#1
// ...

void SeqDB_ReadMemoryTiList(const char * fbeginp,
                            const char * fendp,
                            vector<CSeqDBGiList::STiOid> & tis,
                            bool * in_order)
{
    bool long_ids = false;
    Int8 file_size = fendp - fbeginp;
    
    if (s_SeqDB_IsBinaryNumericList(fbeginp, fendp, long_ids)) {
        Int4 * bbeginp = (Int4*) fbeginp;
        Int4 * bendp = (Int4*) fendp;
        Int4 * bdatap = bbeginp + 2;
        
        Uint4 num_tis = (int)(bendp-bdatap);
        
        int remainder = num_tis % 2;
        
        if (long_ids) {
            num_tis /= 2;
        }
        
        tis.clear();
        
        bool bad_fmt = false;
        
        if (bendp < bdatap) {
            bad_fmt = true;
        } else {
            int marker = SeqDB_GetStdOrd(bbeginp);
            unsigned num_ids = SeqDB_GetStdOrd(bbeginp+1);
            
            if ((marker != -3 && marker != -4) ||
                (num_ids != num_tis) ||
                (remainder && long_ids)) {
                
                bad_fmt = true;
            }
        }
        
        if (bad_fmt) {
            NCBI_THROW(CSeqDBException,
                       eFileErr,
                       "Specified file is not a valid binary GI or TI file.");
        }
        
        tis.reserve(num_tis);
        
        if (long_ids) {
            Int8 * bdatap8 = (Int8*) bdatap;
            Int8 * bendp8 = (Int8*) bendp;
            
            if (in_order) {
                Int8 prev_ti =0;
                bool in_ti_order = true;
                
                Int8 * elem = bdatap8;
                
                while(elem < bendp8) {
                    Int8 this_ti = (Int8) SeqDB_GetStdOrd(elem);
                    tis.push_back(this_ti);
                    
                    if (prev_ti > this_ti) {
                        in_ti_order = false;
                        break;
                    }
                    prev_ti = this_ti;
                    elem ++;
                }
                
                while(elem < bendp8) {
                    tis.push_back((Int8) SeqDB_GetStdOrd(elem++));
                }
                
                *in_order = in_ti_order;
            } else {
                for(Int8 * elem = bdatap8; elem < bendp8; elem ++) {
                    tis.push_back((Int8) SeqDB_GetStdOrd(elem));
                }
            }
        } else {
            if (in_order) {
                int prev_ti =0;
                bool in_ti_order = true;
                
                Int4 * elem = bdatap;
                
                while(elem < bendp) {
                    int this_ti = (int) SeqDB_GetStdOrd(elem);
                    tis.push_back(this_ti);
                    
                    if (prev_ti > this_ti) {
                        in_ti_order = false;
                        break;
                    }
                    prev_ti = this_ti;
                    elem ++;
                }
                
                while(elem < bendp) {
                    tis.push_back((int) SeqDB_GetStdOrd(elem++));
                }
                
                *in_order = in_ti_order;
            } else {
                for(Int4 * elem = bdatap; elem < bendp; elem ++) {
                    tis.push_back((int) SeqDB_GetStdOrd(elem));
                }
            }
        }
    } else {
        // We would prefer to do only one allocation, so assume
        // average gi is 6 digits plus newline.  A few extra will be
        // allocated, but this is preferable to letting the vector
        // double itself (which it still will do if needed).
        
        tis.reserve(int(file_size / 7));
        
        Int8 elem(0);
        
        for(const char * p = fbeginp; p < fendp; p ++) {
            Uint4 dig = 0;
            
            switch(*p) {
            case '0':
                dig = 0;
                break;
            
            case '1':
                dig = 1;
                break;
            
            case '2':
                dig = 2;
                break;
            
            case '3':
                dig = 3;
                break;
            
            case '4':
                dig = 4;
                break;
            
            case '5':
                dig = 5;
                break;
            
            case '6':
                dig = 6;
                break;
            
            case '7':
                dig = 7;
                break;
            
            case '8':
                dig = 8;
                break;
            
            case '9':
                dig = 9;
                break;
            
            case '#':
            case '\n':
            case '\r':
                // Skip blank lines and comments by ignoring zero.
                if (elem != 0) {
                    tis.push_back(elem);
                }
                elem = 0;
                continue;
                
            default:
                {
                    string msg = string("Invalid byte in text TI list [") +
                        NStr::UIntToString((unsigned char)(*p)) + " at location " +
                        NStr::NumericToString(p-fbeginp) + "].";
                    
                    NCBI_THROW(CSeqDBException, eFileErr, msg);
                }
            }
            
            elem *= 10;
            elem += dig;
        }
    }
}

void SeqDB_ReadMemorySiList(const char * fbeginp,
                            const char * fendp,
                            vector<CSeqDBGiList::SSiOid> & sis,
                            bool * in_order)
{
    Int8 file_size = fendp - fbeginp;
    
    // We would prefer to do only one allocation, so assume
    // average seqid is 6 digits plus newline.  A few extra will be
    // allocated, but this is preferable to letting the vector
    // double itself (which it still will do if needed).
        
    sis.reserve(sis.size() + int(file_size / 7));
       
    const char * p = fbeginp;
    const char * head;
    while ( p < fendp) {
        // find the head of the seqid
        while (p< fendp && (*p=='>' || *p==' ' || *p=='\t' || *p=='\n' || *p=='\r')) ++p;
        if (p< fendp && *p == '#') {
            // anything beyond this point in the line is a comment
            while (p< fendp && *p!='\n') ++p;
            continue;
        }
        head = p;
        while (p< fendp && *p!=' ' && *p!='\t' && *p!='\n' && *p!='\r') ++p;
        if (p > head) {
            string acc(head, p);
            string str_id = SeqDB_SimplifyAccession(acc);
            if (str_id != "") {
                sis.push_back(NStr::ToLower(str_id));
            } else {
                cerr << "WARNING:  " << acc
                     << " is not a valid seqid string." << endl;
            } 
        }
    }
    if (in_order) *in_order = false;
}

static bool s_ContainsBinaryNumericIdList(const string& fname, CSeqDBFileGiList::EIdType type)
{
    CMemoryFile mfile(SeqDB_MakeOSPath(fname));

    Int8 file_size = mfile.GetSize();
    const char * fbeginp = (char*) mfile.GetPtr();
    const char * fendp   = fbeginp + (int)file_size;
    
    bool ignore = false;
    bool has_tis = false;
    bool retval = s_SeqDB_IsBinaryNumericList(fbeginp, fendp, ignore, &has_tis);
    if (type == CSeqDBFileGiList::eGiList) {
        return retval;
    } else {
        retval = has_tis && retval;
    }
    return retval;
}

bool SeqDB_IsBinaryTiList(const string  & fname)
{
    return s_ContainsBinaryNumericIdList(fname, CSeqDBFileGiList::eTiList);
}

bool SeqDB_IsBinaryGiList(const string  & fname)
{
    return s_ContainsBinaryNumericIdList(fname, CSeqDBFileGiList::eGiList);
}

void SeqDB_ReadGiList(const string & fname, vector<CSeqDBGiList::SGiOid> & gis, bool * in_order)
{
    CMemoryFile mfile(SeqDB_MakeOSPath(fname));
    
    Int8 file_size = mfile.GetSize();
    const char * fbeginp = (char*) mfile.GetPtr();
    const char * fendp   = fbeginp + (int)file_size;
    
    SeqDB_ReadMemoryGiList(fbeginp, fendp, gis, in_order);
}


void SeqDB_ReadTiList(const string & fname, vector<CSeqDBGiList::STiOid> & tis, bool * in_order)
{
    CMemoryFile mfile(SeqDB_MakeOSPath(fname));
    
    Int8 file_size = mfile.GetSize();
    const char * fbeginp = (char*) mfile.GetPtr();
    const char * fendp   = fbeginp + (int)file_size;
    
    SeqDB_ReadMemoryTiList(fbeginp, fendp, tis, in_order);
}

void SeqDB_ReadSiList(const string & fname, vector<CSeqDBGiList::SSiOid> & sis, bool * in_order) 
{
    CMemoryFile mfile(SeqDB_MakeOSPath(fname));

    Int8 file_size = mfile.GetSize();
    const char *fbeginp = (char*) mfile.GetPtr();
    const char *fendp   = fbeginp + (int) file_size;

    SeqDB_ReadMemorySiList(fbeginp, fendp, sis, in_order);
}

void SeqDB_ReadGiList(const string & fname, vector<int> & gis, bool * in_order)
{
    typedef vector<CSeqDBGiList::SGiOid> TPairList;
    
    TPairList pairs;
    SeqDB_ReadGiList(fname, pairs, in_order);
    
    gis.reserve(pairs.size());
    
    ITERATE(TPairList, iter, pairs) {
        gis.push_back(iter->gi);
    }
}

bool CSeqDBNegativeList::FindGi(int gi)
{
    InsureOrder();
    
    int b(0), e((int)m_Gis.size());
    
    while(b < e) {
        int m = (b + e)/2;
        int m_gi = m_Gis[m];
        
        if (m_gi < gi) {
            b = m + 1;
        } else if (m_gi > gi) {
            e = m;
        } else {
            return true;
        }
    }
    
    return false;
}


bool CSeqDBNegativeList::FindTi(Int8 ti)
{
    InsureOrder();
    
    int b(0), e((int)m_Tis.size());
    
    while(b < e) {
        int m = (b + e)/2;
        Int8 m_ti = m_Tis[m];
        
        if (m_ti < ti) {
            b = m + 1;
        } else if (m_ti > ti) {
            e = m;
        } else {
            return true;
        }
    }
    
    return false;
}

bool CSeqDBNegativeList::FindId(const CSeq_id & id)
{
    bool match_type = false;
    return FindId(id, match_type);
}


bool CSeqDBNegativeList::FindId(const CSeq_id & id, bool & match_type)
{
    if (id.IsGi()) {
        match_type = true;
        return FindGi(GI_TO(int, id.GetGi()));
    } else if (id.IsGeneral() && id.GetGeneral().GetDb() == "ti") {
        match_type = true;
        const CObject_id & obj = id.GetGeneral().GetTag();
        
        Int8 ti = (obj.IsId()
                   ? obj.GetId()
                   : NStr::StringToInt8(obj.GetStr()));
        
        return FindTi(ti);
    } else {
        match_type = false;
        return false;
    }
}


bool CSeqDBGiList::FindId(const CSeq_id & id)
{
    if (id.IsGi()) {
        return FindGi(GI_TO(int, id.GetGi()));
    } else if (id.IsGeneral() && id.GetGeneral().GetDb() == "ti") {
        const CObject_id & obj = id.GetGeneral().GetTag();
        
        Int8 ti = (obj.IsId()
                   ? obj.GetId()
                   : NStr::StringToInt8(obj.GetStr()));
        
        return FindTi(ti);
    } else {
        Int8 num_id;
        string str_id;
        bool simpler;
        SeqDB_SimplifySeqid(*(const_cast<CSeq_id *>(&id)), 0, num_id, str_id, simpler);
        if (FindSi(str_id)) return true;

        // We may have to strip the version to find it...
        size_t pos = str_id.find(".");
        if (pos != str_id.npos) {
            string nover(str_id, 0, pos);
            return FindSi(nover);
        }
    }
    return false;
}


CSeqDBFileGiList::CSeqDBFileGiList(const string & fname, EIdType idtype)
{
    bool in_order = false;
    switch(idtype) {
        case eGiList:
            SeqDB_ReadGiList(fname, m_GisOids, & in_order);
            break;
        case eTiList:
            SeqDB_ReadTiList(fname, m_TisOids, & in_order);
            break;
        case eSiList:
            SeqDB_ReadSiList(fname, m_SisOids, & in_order);
            break;
    }
    m_CurrentOrder = in_order ? eGi : eNone;
}

CSeqDBFileGiList::CSeqDBFileGiList(vector<string> fnames, EIdType idtype) 
{
    bool in_order = false;
    switch(idtype) {
        case eGiList:
        case eTiList:
            NCBI_THROW(CSeqDBException,
                   eArgErr,
                   "Only multiple seqid list is supported.");
        case eSiList:
            ITERATE(vector<string>, iter, fnames) {
                SeqDB_ReadSiList(*iter, m_SisOids, & in_order);
            }
            break;
    }
    m_CurrentOrder = in_order ? eGi : eNone;
}

void SeqDB_CombineAndQuote(const vector<string> & dbs,
                           string               & dbname)
{
    int sz = 0;
    
    for(unsigned i = 0; i < dbs.size(); i++) {
        sz += int(3 + dbs[i].size());
    }
    
    dbname.reserve(sz);
    
    for(unsigned i = 0; i < dbs.size(); i++) {
        if (dbname.size()) {
            dbname.append(" ");
        }
        
        if (dbs[i].find(" ") != string::npos) {
            dbname.append("\"");
            dbname.append(dbs[i]);
            dbname.append("\"");
        } else {
            dbname.append(dbs[i]);
        }
    }
}


void SeqDB_SplitQuoted(const string        & dbname,
                       vector<CTempString> & dbs)
{
    vector<CSeqDB_Substring> subs;
    
    SeqDB_SplitQuoted(dbname, subs);
    
    dbs.resize(0);
    dbs.reserve(subs.size());
    
    ITERATE(vector<CSeqDB_Substring>, iter, subs) {
        CTempString tmp(iter->GetBegin(), iter->Size());
        dbs.push_back(tmp);
    }
}


void SeqDB_SplitQuoted(const string             & dbname,
                       vector<CSeqDB_Substring> & dbs)
{
    // split names
    
    const char * sp = dbname.data();
    
    bool quoted = false;
    unsigned begin = 0;
    
    for(unsigned i = 0; i < dbname.size(); i++) {
        char ch = dbname[i];
        
        if (quoted) {
            // Quoted mode sees '"' as the only actionable token.
            if (ch == '"') {
                if (begin < i) {
                    dbs.push_back(CSeqDB_Substring(sp + begin, sp + i));
                }
                begin = i + 1;
                quoted = false;
            }
        } else {
            // Non-quote mode: Space or quote starts the next string.
            
            if (ch == ' ') {
                if (begin < i) {
                    dbs.push_back(CSeqDB_Substring(sp + begin, sp + i));
                }
                begin = i + 1;
            } else if (ch == '"') {
                if (begin < i) {
                    dbs.push_back(CSeqDB_Substring(sp + begin, sp + i));
                }
                begin = i + 1;
                quoted = true;
            }
        }
    }
    
    if (begin < dbname.size()) {
        dbs.push_back(CSeqDB_Substring(sp + begin, sp + dbname.size()));
    }
}


CIntersectionGiList::CIntersectionGiList(CSeqDBGiList & gilist, vector<int> & gis)
{
    _ASSERT(this != & gilist);
    
    gilist.InsureOrder(CSeqDBGiList::eGi);
    sort(gis.begin(), gis.end());
    
    int list_i = 0;
    int list_n = gilist.GetNumGis();
    int gis_i = 0;
    int gis_n = (int) gis.size();
    
    while(list_i < list_n && gis_i < gis_n) {
        int L = gilist.GetGiOid(list_i).gi;
        int G = gis[gis_i];
        
        if (L < G) {
            list_i ++;
            continue;
        }
        
        if (L > G) {
            gis_i ++;
            continue;
        }
        
        m_GisOids.push_back(gilist.GetGiOid(list_i));
        
        list_i++;
        gis_i++;
    }
    
    m_CurrentOrder = m_GisOids.size() ? eGi : eNone;
}


CIntersectionGiList::CIntersectionGiList(CSeqDBNegativeList & neg_gilist, vector<int> & gis)
{
    neg_gilist.InsureOrder();
    sort(gis.begin(), gis.end());
    
    int list_i = 0;
    int list_n = neg_gilist.GetNumGis();
    int gis_i = 0;
    int gis_n = (int) gis.size();
    
    while(list_i < list_n && gis_i < gis_n) {
        int L = neg_gilist.GetGi(list_i);
        int G = gis[gis_i];
        
        if (L < G) {
            list_i ++;
            continue;
        }
        
        if (L > G) {
            m_GisOids.push_back(gis[gis_i]);
            gis_i ++;
            continue;
        }
        
        list_i++;

        int last_gi = gis[gis_i];
        do { gis_i++; } while (gis_i < gis_n && gis[gis_i] == last_gi);
    }

    // push all the remaining vector gi's if any left
    while (gis_i < gis_n) {
        m_GisOids.push_back(gis[gis_i++]);
    }
    
    m_CurrentOrder = m_GisOids.size() ? eGi : eNone;
}


CSeqDBIdSet::CSeqDBIdSet(const vector<int> & ids, EIdType t, bool positive)
    : m_Positive(positive), m_IdType(t), m_Ids(new CSeqDBIdSet_Vector(ids))
{
    x_SortAndUnique(m_Ids->Set());
}

CSeqDBIdSet::CSeqDBIdSet(const vector<Int8> & ids, EIdType t, bool positive)
    : m_Positive(positive), m_IdType(t), m_Ids(new CSeqDBIdSet_Vector(ids))
{
    x_SortAndUnique(m_Ids->Set());
}

void CSeqDBIdSet::x_SortAndUnique(vector<Int8> & ids)
{
    sort(ids.begin(), ids.end());
    ids.erase(unique(ids.begin(), ids.end()), ids.end());
}

void CSeqDBIdSet::Negate()
{
    m_Positive = ! m_Positive;
}

void CSeqDBIdSet::
x_SummarizeBooleanOp(EOperation op,
                     bool       A_pos,
                     bool       B_pos,
                     bool     & result_pos,
                     bool     & incl_A,
                     bool     & incl_B,
                     bool     & incl_AB)
{
    typedef CSeqDBIdSet TIdList;
    
    incl_A = incl_B = incl_AB = false;
    
    // Each binary boolean function can be represented as a 4 bit
    // descriptor.  The four bits indicate whether the result is true
    // when it appears, respectively, in neither list, only the second
    // list, only the first list, or in both lists.  For example, the
    // operation (A AND B) can be represented as (0001), and (A OR !B)
    // can be written as (1011).  In a positive ID list, 1 means that
    // an ID should be included in database iteration.
    
    // But 4-bit descriptors starting with a '1' correspond to logical
    // operations that include all IDs not appearing in either input
    // set.  But of course we do not have access to the IDs that do
    // not appear, so we cannot (feasibly) compute such operations.
    
    // To solve this problem, De Morgan's Laws are used to transform
    // the operation into its inverse, the results of which can be
    // applied to SeqDB as a negative ID list.
    
    // For our purposes, these three transforms are needed:
    //
    //  1. (!X and !Y) becomes !(X or Y)
    //  2. (!X or Y) becomes !(X and !Y)
    //  3. (X or !Y) becomes !(!X and Y)
    
    result_pos = true;
    
    switch(op) {
    case eAnd:
        if ((! A_pos) && (! B_pos)) {
            op = TIdList::eOr;
            result_pos = false;
            A_pos = B_pos = true;
        }
        break;
        
    case eOr:
        if ((! A_pos) || (! B_pos)) {
            op = TIdList::eAnd;
            result_pos = false;
            A_pos = ! A_pos;
            B_pos = ! B_pos;
        }
        break;
        
    case eXor:
        result_pos = A_pos == B_pos;
        break;
        
    default:
        break;
    }
    
    // Once we have a legal operation, we construct these flags to
    // summarize the boolean operation.  (Each of these corresponds to
    // one of the bits in the 4-bit descriptor.)
    
    switch(op) {
    case eAnd:
        _ASSERT(A_pos || B_pos);
        incl_A = !B_pos;
        incl_B = !A_pos;
        incl_AB = A_pos && B_pos;
        break;
        
    case eOr:
        _ASSERT(A_pos || B_pos);
        incl_A = incl_B = incl_AB = true;
        break;
        
    case eXor:
        incl_AB = (A_pos != B_pos);
        incl_A = incl_B = ! incl_AB;
        break;
        
    default:
        break;
    }
}

void CSeqDBIdSet::
x_BooleanSetOperation(EOperation           op,
                      const vector<Int8> & A,
                      bool                 A_pos,
                      const vector<Int8> & B,
                      bool                 B_pos,
                      vector<Int8>       & result,
                      bool               & result_pos)
{
    bool incl_A(false),
        incl_B(false),
        incl_AB(false);
    
    x_SummarizeBooleanOp(op,
                         A_pos,
                         B_pos,
                         result_pos,
                         incl_A,
                         incl_B,
                         incl_AB);
    
    size_t A_i(0), B_i(0);
    
    while((A_i < A.size()) && (B_i < B.size())) {
        Int8 Ax(A[A_i]), Bx(B[B_i]), target(-1);
        bool included(false);
        
        if (Ax < Bx) {
            ++ A_i;
            target = Ax;
            included = incl_A;
        } else if (Ax > Bx) {
            ++ B_i;
            target = Bx;
            included = incl_B;
        } else {
            ++ A_i;
            ++ B_i;
            target = Ax;
            included = incl_AB;
        }
        
        if (included) {
            result.push_back(target);
        }
    }
    
    if (incl_A) {
        while(A_i < A.size()) {
            result.push_back(A[A_i++]);
        }
    }
    
    if (incl_B) {
        while(B_i < B.size()) {
            result.push_back(B[B_i++]);
        }
    }
}

void CSeqDBIdSet::Compute(EOperation          op,
                          const vector<int> & ids,
                          bool                positive)
{
    CRef<CSeqDBIdSet_Vector> result(new CSeqDBIdSet_Vector);
    
    CRef<CSeqDBIdSet_Vector> B(new CSeqDBIdSet_Vector(ids));
    
    x_SortAndUnique(B->Set());
    
    bool result_pos(true);
    
    x_BooleanSetOperation(op,
                          m_Ids->Set(),
                          m_Positive,
                          B->Set(),
                          positive,
                          result->Set(),
                          result_pos);
    
    m_Positive = result_pos;
    m_Ids = result;
}

void CSeqDBIdSet::Compute(EOperation           op,
                          const vector<Int8> & ids,
                          bool                 positive)
{
    CRef<CSeqDBIdSet_Vector> result(new CSeqDBIdSet_Vector);
    
    CRef<CSeqDBIdSet_Vector> B(new CSeqDBIdSet_Vector(ids));
    x_SortAndUnique(B->Set());
    
    bool result_pos(true);
    
    x_BooleanSetOperation(op,
                          m_Ids->Set(),
                          m_Positive,
                          B->Set(),
                          positive,
                          result->Set(),
                          result_pos);
    
    m_Positive = result_pos;
    m_Ids = result;
}

void CSeqDBIdSet::Compute(EOperation op, const CSeqDBIdSet & ids)
{
    if (m_IdType != ids.m_IdType ) {
        NCBI_THROW(CSeqDBException,
                   eArgErr,
                   "Set operation requested but ID types don't match.");
    }
    
    CRef<CSeqDBIdSet_Vector> result(new CSeqDBIdSet_Vector);
    bool result_pos(true);
    
    x_BooleanSetOperation(op,
                          m_Ids->Set(),
                          m_Positive,
                          ids.m_Ids->Get(),
                          ids.m_Positive,
                          result->Set(),
                          result_pos);
    
    m_Positive = result_pos;
    m_Ids = result;
}

CRef<CSeqDBGiList> CSeqDBIdSet::GetPositiveList()
{
    CRef<CSeqDBGiList> ids(new CSeqDBGiList);
    
    if (! m_Positive) {
        NCBI_THROW(CSeqDBException,
                   eFileErr,
                   "Positive ID list requested but only negative exists.");
    }
    
    if (m_IdType == eTi) {
        ids->ReserveTis(m_Ids->Size());
        
        ITERATE(vector<Int8>, iter, m_Ids->Set()) {
            ids->AddTi(*iter);
        }
    } else {
        ids->ReserveGis(m_Ids->Size());
        
        ITERATE(vector<Int8>, iter, m_Ids->Set()) {
            _ASSERT(((*iter) >> 32) == 0);
            ids->AddGi((int)*iter);
        }
    }
    
    return ids;
}

CRef<CSeqDBNegativeList> CSeqDBIdSet::GetNegativeList()
{
    if (m_Positive) {
        NCBI_THROW(CSeqDBException,
                   eFileErr,
                   "Negative ID list requested but only positive exists.");
    }
    
    CRef<CSeqDBNegativeList> ids(new CSeqDBNegativeList);
    
    if (m_IdType == eTi) {
        ids->ReserveTis(m_Ids->Size());
        
        ITERATE(vector<Int8>, iter, m_Ids->Set()) {
            ids->AddTi(*iter);
        }
    } else {
        ids->ReserveGis(m_Ids->Size());
        
        ITERATE(vector<Int8>, iter, m_Ids->Set()) {
            _ASSERT(((*iter) >> 32) == 0);
            ids->AddGi((int)*iter);
        }
    }
    
    return ids;
}

CSeqDBIdSet::CSeqDBIdSet()
    : m_Positive (false),
      m_IdType   (eGi),
      m_Ids      (new CSeqDBIdSet_Vector)
{
}

bool CSeqDBIdSet::Blank() const
{
    return (! m_Positive) && (0 == m_Ids->Size());
}

void SeqDB_FileIntegrityAssert(const string & file,
                               int            line,
                               const string & text)
{
    string msg = "Validation failed: [" + text + "] at ";
    msg += file + ":" + NStr::IntToString(line);
    SeqDB_ThrowException(CSeqDBException::eFileErr, msg);
}

ESeqDBIdType SeqDB_SimplifySeqid(CSeq_id       & bestid,
                                 const string  * acc,
                                 Int8          & num_id,
                                 string        & str_id,
                                 bool          & simpler)
{
    ESeqDBIdType result = eStringId;
    
    const CTextseq_id * tsip = 0;
    
    bool matched = true;

    switch(bestid.Which()) {
    case CSeq_id::e_Gi:
        simpler = true;
        num_id = bestid.GetGi();
        result = eGiId;
        break;
        
    case CSeq_id::e_Gibbsq:    /* gibbseq */
        simpler = true;
        result = eStringId;
        str_id = NStr::UIntToString(bestid.GetGibbsq());
        break;
        
    case CSeq_id::e_General:
        {
            const CDbtag & dbt = bestid.GetGeneral();
            
            if (dbt.CanGetDb()) {
                if (dbt.GetDb() == "BL_ORD_ID") {
                    simpler = true;
                    num_id = dbt.GetTag().GetId();
                    result = eOID;
                    break;
                }
                
                if (dbt.GetDb() == "PIG") {
                    simpler = true;
                    num_id = dbt.GetTag().GetId();
                    result = ePigId;
                    break;
                }
                
                if (dbt.GetDb() == "ti") {
                    simpler = true;
                    num_id = (dbt.GetTag().IsStr()
                              ? NStr::StringToInt8(dbt.GetTag().GetStr())
                              : dbt.GetTag().GetId());
                    
                    result = eTiId;
                    break;
                }


                if (NStr::CompareNocase(dbt.GetDb(), "GNOMON") == 0) {
                    str_id = bestid.AsFastaString();
                    str_id = NStr::ToLower(str_id);
                    result = eStringId;
                    break;
                }
            }
            
            if (dbt.CanGetTag() && dbt.GetTag().IsStr()) {
                result = eStringId;
                str_id = dbt.GetTag().GetStr();
                str_id = NStr::ToLower(str_id);
            } else {
                // Use the default logic.
                matched = false;
            }
        }
        break;
        
    case CSeq_id::e_Local:     /* local */
        simpler = true;
        result = eStringId;
        {
            const CObject_id & objid = bestid.GetLocal();
            
            if (objid.IsStr()) {
                // sparse version will leave "lcl|" off.
                str_id = objid.GetStr();
                str_id = NStr::ToLower(str_id);
            } else {
                // Local numeric ids are stored as strings.
                str_id = "lcl|" + NStr::IntToString(objid.GetId());
            }
        }
        break;
        
        // tsip types
        
    case CSeq_id::e_Embl:      /* embl */
    case CSeq_id::e_Ddbj:      /* ddbj */
    case CSeq_id::e_Genbank:   /* genbank */
    case CSeq_id::e_Tpg:       /* Third Party Annot/Seq Genbank */
    case CSeq_id::e_Tpe:       /* Third Party Annot/Seq EMBL */
    case CSeq_id::e_Tpd:       /* Third Party Annot/Seq DDBJ */
    case CSeq_id::e_Other:     /* other */
    case CSeq_id::e_Swissprot: /* swissprot (now with versions) */
    case CSeq_id::e_Gpipe:     /* internal NCBI genome pipeline */
        tsip = bestid.GetTextseq_Id();
        break;
        
    case CSeq_id::e_Pir:       /* pir   */
    case CSeq_id::e_Prf:       /* prf   */
        tsip = bestid.GetTextseq_Id();
        break;
        
    default:
        matched = false;
    }
    
    // Default: if we have a string, use it; if we only have seqid,
    // create a string.  This should not happen if the seqid matches
    // one of the cases above, which currently correspond to all the
    // supported seqid types.
    
    CSeq_id::ELabelFlags label_flags = (CSeq_id::ELabelFlags)
        (CSeq_id::fLabel_GeneralDbIsContent | CSeq_id::fLabel_Version);
    
    if (! matched) {
        // (should not happen normally)
        
        simpler = false;
        result  = eStringId;
        
        if (acc) {
            str_id = *acc;
            str_id = NStr::ToLower(str_id);
        } else {
            bestid.GetLabel(& str_id, CSeq_id::eFasta, label_flags);
            str_id = NStr::ToLower(str_id);
        }
    }
    
    if (tsip) {
        bool found = false;
        
        if (tsip->CanGetAccession()) {
            str_id = tsip->GetAccession();
            str_id = NStr::ToLower(str_id);
            found = true;
            
            if (tsip->CanGetVersion()) {
                str_id += ".";
                str_id += NStr::UIntToString(tsip->GetVersion());
            }
        } else if (tsip->CanGetName()) {
            str_id = tsip->GetName();
            str_id = NStr::ToLower(str_id);
            found = true;
        }
        
        if (found) {
            simpler = true;
            result = eStringId;
        }
    }
    
    return result;
}

/// Find the end of a single element in a Seq-id set
/// 
/// Seq-id strings sometimes contain several Seq-ids.  This function
/// looks for the end of the first Seq-id, and will return its length.
/// Static methods of CSeq_id are used to evaluate tokens.
/// 
/// @param str
///   Seq-id string to search.
/// @param pos
///   Position at which to start search.
/// @return
///   End position of first fasta id, or string::npos in case of error.

static size_t
s_SeqDB_EndOfFastaID(const string & str, size_t pos)
{
    // (Derived from s_EndOfFastaID()).
    
    size_t vbar = str.find('|', pos);
    
    if (vbar == string::npos) {
        return string::npos; // bad
    }
    
    string portion(str, pos, vbar - pos);
    
    CSeq_id::E_Choice choice =
        CSeq_id::WhichInverseSeqId(portion.c_str());
    
    if (choice != CSeq_id::e_not_set) {
        size_t vbar_prev = vbar;
        int count;
        for (count=0; ; ++count, vbar_prev = vbar) {
            vbar = str.find('|', vbar_prev + 1);
            
            if (vbar == string::npos) {
                break;
            }
            
            int start_pt = int(vbar_prev + 1);
            string element(str, start_pt, vbar - start_pt);
            
            choice = CSeq_id::WhichInverseSeqId(element.c_str());
            
            if (choice != CSeq_id::e_not_set) {
                vbar = vbar_prev;
                break;
            }
        }
    } else {
        return string::npos; // bad
    }
    
    return (vbar == string::npos) ? str.size() : vbar;
}

/// Parse string into a sequence of Seq-id objects.
///
/// A string is broken down into Seq-ids and the set of Seq-ids is
/// returned.
///
/// @param line
///   The string to interpret.
/// @param seqids
///   The returned set of Seq-id objects.
/// @return
///   true if any Seq-id objects were found.

static bool
s_SeqDB_ParseSeqIDs(const string              & line,
                    vector< CRef< CSeq_id > > & seqids)
{
    // (Derived from s_ParseFastaDefline()).
    
    seqids.clear();
    size_t pos = 0;
    
    while (pos < line.size()) {
        size_t end = s_SeqDB_EndOfFastaID(line, pos);
        
        if (end == string::npos) {
            // We didn't get a clean parse -- ignore the data after
            // this point, and return what we have.
            break;
        }
        
        string element(line, pos, end - pos);
        
        CRef<CSeq_id> id;
        
        try {
            id = new CSeq_id(element);
        }
        catch(invalid_argument &) {
            // Maybe this should be done: "seqids.clear();"
            break;
        }
        
        seqids.push_back(id);
        pos = end + 1;
    }
    
    return ! seqids.empty();
}

ESeqDBIdType SeqDB_SimplifyAccession(const string & acc,
                                     Int8         & num_id,
                                     string       & str_id,
                                     bool         & simpler)
{
    ESeqDBIdType result = eStringId;
    num_id = (Uint4)-1;
    
    vector< CRef< CSeq_id > > seqid_set;
    
    if (s_SeqDB_ParseSeqIDs(acc, seqid_set)) {
        // Something like SeqIdFindBest()
        CRef<CSeq_id> bestid =
            FindBestChoice(seqid_set, CSeq_id::BestRank);
        
        result = SeqDB_SimplifySeqid(*bestid, & acc, num_id, str_id, simpler);
    } else {
        str_id = acc;
        result = eStringId;
        simpler = false;
    }
    
    return result;
}

const string SeqDB_SimplifyAccession(const string &acc)
{
    Int8 num_id;
    string str_id;
    bool simpler(false);
    ESeqDBIdType result = SeqDB_SimplifyAccession(acc, num_id, str_id, simpler);
    if (result == eStringId) return str_id;
    else return "";
}

void SeqDB_GetFileExtensions(bool db_is_protein, vector<string>& extn)
{
    // NOTE: If more extensions are added, please keep in sync with
    // updatedb.pl's DistributeBlastDbsToBackends
    // and Blast.pm's @blastdb_extensions
    extn.clear();

    const string kExtnMol(1, db_is_protein ? 'p' : 'n');

    extn.push_back(kExtnMol + "al");   // alias file
    extn.push_back(kExtnMol + "in");   // index file
    extn.push_back(kExtnMol + "hr");   // header file
    extn.push_back(kExtnMol + "sq");   // sequence file
    extn.push_back(kExtnMol + "ni");   // ISAM numeric index file
    extn.push_back(kExtnMol + "nd");   // ISAM numeric data file
    extn.push_back(kExtnMol + "si");   // ISAM string index file
    extn.push_back(kExtnMol + "sd");   // ISAM string data file
    extn.push_back(kExtnMol + "pi");   // ISAM PIG index file
    extn.push_back(kExtnMol + "pd");   // ISAM PIG data file
    extn.push_back(kExtnMol + "xc");   // gi-to-taxids cyclopedia file
    extn.push_back(kExtnMol + "xa");   // gi-to-taxids index file
    extn.push_back(kExtnMol + "xb");   // gi-to-taxids data file

    // Contain masking information
    extn.push_back(kExtnMol + "aa");   // ISAM mask index file
    extn.push_back(kExtnMol + "ab");   // ISAM mask data file (big-endian)
    extn.push_back(kExtnMol + "ac");   // ISAM mask data file (little-endian)
    extn.push_back(kExtnMol + "og");   // OID to GI file
    extn.push_back(kExtnMol + "hi");   // ISAM sequence hash index file
    extn.push_back(kExtnMol + "hd");   // ISAM sequence hash data file
    extn.push_back(kExtnMol + "ti");   // ISAM trace id index file
    extn.push_back(kExtnMol + "td");   // ISAM trace id data file
}

END_NCBI_SCOPE

