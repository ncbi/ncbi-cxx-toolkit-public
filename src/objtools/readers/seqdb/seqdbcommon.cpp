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

#include <ncbi_pch.hpp>
#include <corelib/metareg.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbifile.hpp>
#include <objtools/readers/seqdb/seqdbcommon.hpp>
#include "seqdbgeneral.hpp"
#include "seqdbatlas.hpp"
#include <algorithm>

BEGIN_NCBI_SCOPE

CSeqDB_Substring SeqDB_GetFileName(CSeqDB_Substring s)
{
    int off = s.FindLastOf(CFile::GetPathSeparator());
    
    if (off != -1) {
        s.EraseFront(off + 1);
    }
    
    return s;
}


CSeqDB_Substring SeqDB_GetDirName(CSeqDB_Substring s)
{
    int off = s.FindLastOf(CFile::GetPathSeparator());
    
    if (off != -1) {
        s.Resize(off);
    } else {
        s.Clear();
    }
    
    return s;
}


CSeqDB_Substring SeqDB_GetBasePath(CSeqDB_Substring s)
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
        if (s[slen-4] == '.'     &&
            isalpha( s[slen-3] ) &&
            isalpha( s[slen-2] ) &&
            isalpha( s[slen-1] )) {
            
#ifdef _DEBUG
            
            // Of course, nal and pal are not the only valid
            // extensions, but this code is only used with these two,
            // as far as I know, at this moment in time.
            
            string extn(s.GetEnd()-4, s.GetEnd());
            
            if ((extn != ".nal") &&
                (extn != ".nin") &&
                (extn != ".pal") &&
                (extn != ".pin")) {
                
                cout << "Extension trim error: " << extn << endl;
                
                _ASSERT((extn == ".nal") ||
                        (extn == ".nin") ||
                        (extn == ".pal") ||
                        (extn == ".pin"));
            }
#endif
            
            s.Resize(slen - 4);
        }
    }
    
    return s;
}


CSeqDB_Substring SeqDB_GetBaseName(CSeqDB_Substring s)
{
    return SeqDB_GetBasePath( SeqDB_GetFileName(s) );
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
/// @return
///   true if either of the index or alias files is found

static bool s_SeqDB_DBExists(const string         & dbname,
                             char                   dbtype,
                             CSeqDB_FileExistence & access)
{
    string path;
    path.reserve(dbname.size() + 4);
    path.assign(dbname.data(), dbname.data() + dbname.size());
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
/// @param atlas
///   The memory management layer.
/// @param locked
///   The lock holder object for this thread.
/// @return
///   Full pathname, minus extension, or empty string if none found.

static string s_SeqDB_TryPaths(const string         & blast_paths,
                               const string         & dbname,
                               char                   dbtype,
                               bool                   exact,
                               CSeqDB_FileExistence & access)
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
        
        string os_road = CDirEntry::ConvertToOSPath(*road);
        
        SeqDB_CombinePath(CSeqDB_Substring(os_road),
                          CSeqDB_Substring(dbname),
                          0,
                          attempt);
        
        if (exact) {
            if (access.DoesFileExist(attempt)) {
                result = attempt;
                break;
            }
        } else {
            if (s_SeqDB_DBExists(attempt, dbtype, access)) {
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
                        CSeqDB_FileExistence & access)
{
    // Local directory first;
    
    string pathology(CDir::GetCwd() + s_GetPathSplitter());
    
    // Then, BLASTDB;
    
    CNcbiEnvironment env;
    pathology += env.Get("BLASTDB");
    pathology += s_GetPathSplitter();
    
    // Finally, the config file.
    
    CMetaRegistry::SEntry sentry =
        CMetaRegistry::Load("ncbi", CMetaRegistry::eName_RcOrIni);
    
    if (sentry.registry) {
        pathology += sentry.registry->Get("BLAST", "BLASTDB");
        pathology += s_GetPathSplitter();
    }
    
    // Time to field test this new and terrible weapon.
    
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
                                   access);
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
        CFile whole(CDirEntry::ConvertToOSPath(fname));
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
            {
                bool already = true;
                
                for(int i = 1; i < (int) m_GisOids.size(); i++) {
                    if (m_GisOids[i].gi < m_GisOids[i-1].gi) {
                        already = false;
                        break;
                    }
                }
                
                if (! already) {
                    CSeqDB_SortGiLessThan sorter;
                    sort(m_GisOids.begin(), m_GisOids.end(), sorter);
                }
            }
            break;
            
        default:
            NCBI_THROW(CSeqDBException,
                       eFileErr,
                       "Unrecognized sort order requested.");
        }
        
        m_CurrentOrder = order;
    }
}


bool CSeqDBGiList::FindGi(int gi)
{
    int oid(0), index(0);
    return GiToOid(gi, oid, index);
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


void
CSeqDBGiList::GetGiList(vector<int>& gis) const
{
    gis.clear();
    gis.reserve(Size());

    ITERATE(vector<SGiOid>, itr, m_GisOids) {
        gis.push_back(itr->gi);
    }
}


void SeqDB_ReadBinaryGiList(const string & fname, vector<int> & gis)
{
    CMemoryFile mfile(CDirEntry::ConvertToOSPath(fname));
    
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


void SeqDB_ReadMemoryGiList(const char * fbeginp,
                            const char * fendp,
                            vector<CSeqDBGiList::SGiOid> & gis,
                            bool * in_order)
{
    bool is_binary = false;
    
    Int8 file_size = fendp - fbeginp;
    
    if (file_size == 0) {
        NCBI_THROW(CSeqDBException,
                   eFileErr,
                   "Specified file is empty.");
    } else if (isdigit((unsigned char)(*((char*) fbeginp)))) {
        is_binary = false;
    } else if ((file_size >= 8) && (*fbeginp == (Int4)-1)) {
        is_binary = true;
    } else {
        NCBI_THROW(CSeqDBException,
                   eFileErr,
                   "Specified file is not a valid GI list.");
    }
    
    if (is_binary) {
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
            
            case '\n':
            case '\r':
                // Skip blank lines by ignoring zero.
                if (elem != 0) {
                    gis.push_back(elem);
                }
                elem = 0;
                continue;
                
            default:
                {
                    string msg = string("Invalid byte in text GI list [") +
                        NStr::UIntToString(int(*p)) + " at location " +
                        NStr::UIntToString(p-fbeginp) + "].";
                    
                    NCBI_THROW(CSeqDBException, eFileErr, msg);
                }
            }
            
            elem *= 10;
            elem += dig;
        }
    }
}


void SeqDB_ReadGiList(const string & fname, vector<CSeqDBGiList::SGiOid> & gis, bool * in_order)
{
    CMemoryFile mfile(CDirEntry::ConvertToOSPath(fname));
    
    Int8 file_size = mfile.GetSize();
    const char * fbeginp = (char*) mfile.GetPtr();
    const char * fendp   = fbeginp + (int)file_size;
    
    SeqDB_ReadMemoryGiList(fbeginp, fendp, gis, in_order);
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

CSeqDBFileGiList::CSeqDBFileGiList(const string & fname)
{
    bool in_order = false;
    SeqDB_ReadGiList(fname, m_GisOids, & in_order);
    m_CurrentOrder = in_order ? eGi : eNone;
}

void SeqDB_CombineAndQuote(const vector<string> & dbs,
                           string               & dbname)
{
    int sz = 0;
    
    for(unsigned i = 0; i < dbs.size(); i++) {
        sz += 3 + dbs[i].size();
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


END_NCBI_SCOPE

