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

string SeqDB_GetFileName(string s)
{
    size_t off = s.find_last_of(CFile::GetPathSeparator());
    
    if (off != s.npos) {
        s.erase(0, off + 1);
    }
    
    return s;
}

string SeqDB_GetDirName(string s)
{
    size_t off = s.find_last_of(CFile::GetPathSeparator());
    
    if (off != s.npos) {
        s.erase(off);
    }
    
    return s;
}

string SeqDB_GetBasePath(string s)
{
    size_t off = s.find_last_of(".");
    
    if (off != s.npos) {
        s.erase(off);
    }
    
    return s;
}

string SeqDB_GetBaseName(string s)
{
    return SeqDB_GetBasePath( SeqDB_GetFileName(s) );
}

string SeqDB_CombinePath(const string & one, const string & two)
{
    char delim = CFile::GetPathSeparator();
    
    if (two.empty()) {
        return one;
    }
    
    if (one.empty() || two[0] == delim) {
        return two;
    }
    
    string result;
    result.reserve(one.size() + two.size() + 1);
    
    result += one;
    
    if (result[one.size() - 1] != delim) {
        result += delim;
    }
    
    result += two;
    
    return result;
}

/// Test whether an index or alias file exists
///
/// The provide filename is combined with both of the extensions
/// appropriate to the database sequence type, and the resulting
/// strings are checked for existence in the file system.
///
/// @param dbname
///   Input path and filename
/// @param dbtype
///   Database type, either protein or nucleotide
/// @param atlas
///   The memory management layer.
/// @param locked
///   The lock holder object for this thread.
/// @return
///   true if either of the index or alias files is found

static bool s_SeqDB_DBExists(const string   & dbname,
                             char             dbtype,
                             CSeqDBAtlas    & atlas,
                             CSeqDBLockHold & locked)
{
    return (atlas.DoesFileExist(dbname + "." + dbtype + "al", locked) ||
            atlas.DoesFileExist(dbname + "." + dbtype + "in", locked));
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
static string s_SeqDB_TryPaths(const string   & blast_paths,
                               const string   & dbname,
                               char             dbtype,
                               bool             exact,
                               CSeqDBAtlas    & atlas,
                               CSeqDBLockHold & locked)
{
    vector<string> roads;
    NStr::Tokenize(blast_paths, ":", roads, NStr::eMergeDelims);
    
    string result;
    
    ITERATE(vector<string>, road, roads) {
        string attempt = SeqDB_CombinePath(*road, dbname);
        
        if (exact) {
            if (atlas.DoesFileExist(attempt, locked)) {
                result = attempt;
                break;
            }
        } else {
            if (s_SeqDB_DBExists(attempt, dbtype, atlas, locked)) {
                result = attempt;
                break;
            }
        }
    }
    
    return result;
}

string SeqDB_FindBlastDBPath(const string   & dbname,
                             char             dbtype,
                             string         * sp,
                             bool             exact,
                             CSeqDBAtlas    & atlas,
                             CSeqDBLockHold & locked)
{
    // Local directory first;
    
    string pathology(CDir::GetCwd() + ":");
    
    // Then, BLASTDB;
    
    CNcbiEnvironment env;
    pathology += env.Get("BLASTDB");
    pathology += ":";
    
    // Finally, the config file.
    
    CMetaRegistry::SEntry sentry =
        CMetaRegistry::Load("ncbi", CMetaRegistry::eName_RcOrIni);
    
    if (sentry.registry) {
        pathology += sentry.registry->Get("BLAST", "BLASTDB");
        pathology += ":";
    }
    
    // Time to field test this new and terrible weapon.
    
    if (sp) {
        *sp = pathology;
    }
    
    return s_SeqDB_TryPaths(pathology, dbname, dbtype, exact, atlas, locked);
}

void SeqDB_JoinDelim(string & a, const string & b, const string & delim)
{
    if (b.empty()) {
        return;
    }
    
    if (a.empty()) {
        a = b;
        return;
    }
    
    a.reserve(a.length() + b.length() + delim.length());
    a += delim;
    a += b;
}


CSeqDBGiList::CSeqDBGiList()
    : m_CurrentOrder(eNone)
{
}

class CSeqDB_SortOidLessThan {
public:
    int operator()(const CSeqDBGiList::SGiOid & lhs,
                   const CSeqDBGiList::SGiOid & rhs)
    {
        return lhs.oid < rhs.oid;
    }
};

class CSeqDB_SortGiLessThan {
public:
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
    
    int b(0), e(m_GisOids.size());
    
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

void SeqDB_ReadBinaryGiList(const string & fname, vector<int> & gis)
{
    CMemoryFile mfile(fname);
    
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
    } else if (isdigit(*((char*) fbeginp))) {
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
    CMemoryFile mfile(fname);
    
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

END_NCBI_SCOPE

