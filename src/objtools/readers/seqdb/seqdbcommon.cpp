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

BEGIN_NCBI_SCOPE

// debug tricks/tools

int seqdb_debug_class = 0; // debug_mvol | debug_alias;

string SeqDB_GetFileName(string s)
{
    size_t off = s.find_last_of("/");
    
    if (off != s.npos) {
        s.erase(0, off + 1);
    }
    
    return s;
}

string SeqDB_GetDirName(string s)
{
    size_t off = s.find_last_of("/");
    
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

static bool s_SeqDB_DBExists(const string & dbname, char dbtype)
{
    return (CFile(dbname + "." + dbtype + "al").Exists() ||
            CFile(dbname + "." + dbtype + "in").Exists());
}

static string s_SeqDB_TryPaths(const string & blast_paths,
                               const string & dbname,
                               char           dbtype,
                               bool           exact_name)
{
    vector<string> roads;
    NStr::Tokenize(blast_paths, ":", roads, NStr::eMergeDelims);
    
    string result;
    
    for(Uint4 i = 0; i < roads.size(); i++) {
        string attempt = SeqDB_CombinePath(roads[i], dbname);
        
        if (exact_name) {
            if (CFile(attempt).Exists()) {
                result = attempt;
                break;
            }
        } else {
            if (s_SeqDB_DBExists(attempt, dbtype)) {
                result = attempt;
                break;
            }
        }
    }
    
    return result;
}

string SeqDB_FindBlastDBPath(const string & dbname,
                             char           dbtype,
                             string       * sp,
                             bool           exact_name)
{
    // Local directory first;
    
    string pathology(".:");
    
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
    
    return s_SeqDB_TryPaths(pathology, dbname, dbtype, exact_name);
}

END_NCBI_SCOPE

