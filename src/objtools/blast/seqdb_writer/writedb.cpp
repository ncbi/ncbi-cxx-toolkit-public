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

/// @file writedb.cpp
/// Implementation for the CWriteDB class, the top level class for WriteDB.

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include <objtools/blast/seqdb_writer/writedb.hpp>
#include <objtools/blast/seqdb_reader/seqdb.hpp>
#include "writedb_impl.hpp"
#include <objtools/blast/seqdb_writer/writedb_convert.hpp>
#include <iostream>

BEGIN_NCBI_SCOPE

using namespace std;

// Impl


// CWriteDB

CWriteDB::CWriteDB(const string       & dbname,
                   CWriteDB::ESeqType   seqtype,
                   const string       & title,
                   int                  indices,
                   bool                 parse_ids,
                   bool                 use_gi_mask)
    : m_Impl(0)
{
    m_Impl = new CWriteDB_Impl(dbname,
                               seqtype == eProtein,
                               title,
                               (EIndexType)indices,
                               parse_ids,
                               use_gi_mask);
}

CWriteDB::~CWriteDB()
{
    delete m_Impl;
}

void CWriteDB::AddSequence(const CBioseq & bs)
{
    m_Impl->AddSequence(bs);
}

void CWriteDB::AddSequence(const CBioseq_Handle & bsh)
{
    m_Impl->AddSequence(bsh);
}

void CWriteDB::AddSequence(const CBioseq & bs, CSeqVector & sv)
{
    m_Impl->AddSequence(bs, sv);
}

void CWriteDB::SetDeflines(const CBlast_def_line_set & deflines)
{
    m_Impl->SetDeflines(deflines);
}

void CWriteDB::SetPig(int pig)
{
    m_Impl->SetPig(pig);
}

void CWriteDB::Close()
{
    m_Impl->Close();
}

void CWriteDB::AddSequence(const CTempString & sequence,
                           const CTempString & ambig)
{
    string s(sequence.data(), sequence.length());
    string a(ambig.data(), ambig.length());
    
    m_Impl->AddSequence(s, a);
}

void CWriteDB::SetMaxFileSize(Uint8 sz)
{
    m_Impl->SetMaxFileSize(sz);
}

void CWriteDB::SetMaxVolumeLetters(Uint8 sz)
{
    m_Impl->SetMaxVolumeLetters(sz);
}

CRef<CBlast_def_line_set>
CWriteDB::ExtractBioseqDeflines(const CBioseq & bs, bool parse_ids)
{
    return CWriteDB_Impl::ExtractBioseqDeflines(bs, parse_ids);
}

void CWriteDB::SetMaskedLetters(const string & masked)
{
    m_Impl->SetMaskedLetters(masked);
}

void CWriteDB::ListVolumes(vector<string> & vols)
{
    m_Impl->ListVolumes(vols);
}

void CWriteDB::ListFiles(vector<string> & files)
{
    m_Impl->ListFiles(files);
}

#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
int CWriteDB::
RegisterMaskAlgorithm(EBlast_filter_program   program, 
                      const string          & options,
                      const string          & name)
{
    return m_Impl->RegisterMaskAlgorithm(program, options, name);
}

int CWriteDB::
RegisterMaskAlgorithm(const string & id,
                      const string & description,
                      const string & options)
{
    return m_Impl->RegisterMaskAlgorithm(id, description, options);
}

void CWriteDB::SetMaskData(const CMaskedRangesVector & ranges,
                           const vector<int>         & gis)
{
    m_Impl->SetMaskData(ranges, gis);
}

int CWriteDB::FindColumn(const string & title) const
{
    return m_Impl->FindColumn(title);
}

int CWriteDB::CreateUserColumn(const string & title)
{
    return m_Impl->CreateColumn(title);
}

void CWriteDB::AddColumnMetaData(int col_id, const string & key, const string & value)
{
    m_Impl->AddColumnMetaData(col_id, key, value);
}

CBlastDbBlob & CWriteDB::SetBlobData(int col_id)
{
    return m_Impl->SetBlobData(col_id);
}
#endif

CBinaryListBuilder::CBinaryListBuilder(EIdType id_type)
    : m_IdType(id_type)
{
}

void CBinaryListBuilder::Write(const string & fname)
{
    // Create a binary stream.
    ofstream outp(fname.c_str(), ios::binary);
    Write(outp);
}

void CBinaryListBuilder::Write(CNcbiOstream& outp)
{ 
    // Header; first check for 8 byte ids.
    
    bool eight = false;
    
    ITERATE(vector<Int8>, iter, m_Ids) {
        Int8 id = *iter;
        _ASSERT(id > 0);
        
        if ((id >> 32) != 0) {
            eight = true;
            break;
        }
    }
    
    Int4 magic = 0;
    
    switch(m_IdType) {
    case eGi:
        magic = eight ? -2 : -1;
        break;
        
    case eTi:
        magic = eight ? -4 : -3;
        break;
        
    default:
        NCBI_THROW(CWriteDBException,
                   eArgErr,
                   "Error: Unsupported ID type specified.");
    }
    
    s_WriteInt4(outp, magic);
    s_WriteInt4(outp, (int)m_Ids.size());
    
    sort(m_Ids.begin(), m_Ids.end());
    
    if (eight) {
        ITERATE(vector<Int8>, iter, m_Ids) {
            s_WriteInt8BE(outp, *iter);
        }
    } else {
        ITERATE(vector<Int8>, iter, m_Ids) {
            s_WriteInt4(outp, (int)*iter);
        }
    }
}

/// Returns true if the BLAST DB exists, otherwise throws a CSeqDBException
/// @param dbname name of BLAST DB [in]
/// @param is_prot is the BLAST DB protein? [in]
static bool 
s_DoesBlastDbExist(const string& dbname, bool is_protein)
{
    char dbtype(is_protein ? 'p' : 'n');
    string path = SeqDB_ResolveDbPathNoExtension(dbname, dbtype);
    if (path.empty()) {
        string msg("Failed to find ");
        msg += (is_protein ? "protein " : "nucleotide ");
        msg += dbname + " BLAST database";
        NCBI_THROW(CSeqDBException, eFileErr, msg);
    }
    return true;
}

/// Computes the number of sequences and (alias) database length for alias
/// files
/// @param dbname Name of the BLAST database over which the alias file is being
/// created [in]
/// @param is_prot is the BLAST database protein? [in]
/// @param dbsize (Approximate) number of letters in the BLAST DB [out]
/// @param num_seqs_found Number of sequences found in the dbname, or the
/// number of sequences in the intersection between the dbname and the GIs in
/// the gi_file_name (if applicable) [out]
static bool
s_ComputeNumSequencesAndDbLength(const string& dbname,
                                 bool is_prot,
                                 Uint8* dbsize,
                                 int* num_seqs_found)
{
    _ASSERT((dbsize != NULL));
    _ASSERT(num_seqs_found != NULL);
    *dbsize = 0u;
    *num_seqs_found = 0u;

    CSeqDB::ESeqType dbtype(is_prot ? CSeqDB::eProtein : CSeqDB::eNucleotide);
    try {
        CRef<CSeqDB> dbhandle(new CSeqDB(dbname, dbtype));
        dbhandle->GetTotals(CSeqDB::eFilteredAll, num_seqs_found, dbsize, false);
    } catch(...) {
        return false;
    }
    return true;
}

static void
s_PrintAliasFileCreationLog(const string& dbname,
                            bool is_protein,
                            int num_seqs_found,
                            const string& gi_file_name = kEmptyStr,
                            int num_seqs_in_gifile = 0)
{
    if ( !gi_file_name.empty() ) {
        /* This won't work if the target directory is not the current working directory
        CRef<CSeqDBFileGiList> gilist;
        gilist.Reset(new CSeqDBFileGiList(gi_file_name));
        num_seqs_in_gifile = gilist->Size();
        } */
        LOG_POST("Created " << (is_protein ? "protein " : "nucleotide ") <<
            dbname << " BLAST (alias) database with " << num_seqs_found 
            << " sequences (out of " << num_seqs_in_gifile << " in " 
            << gi_file_name << ", " << setprecision(0) << fixed << 
            (num_seqs_found*100.0/num_seqs_in_gifile) << "% found)");
    } else {
        LOG_POST("Created " << (is_protein ? "protein " : "nucleotide ") <<
            "BLAST (alias) database " << dbname << " with " << 
            num_seqs_found << " sequences");
    }
}

void CWriteDB_CreateAliasFile(const string& file_name,
                              const string& db_name,
                              CWriteDB::ESeqType seq_type,
                              const string& gi_file_name,
                              const string& title,
                              EAliasFileFilterType alias_type /*= eGiList*/)
{
    vector<string> db(1, db_name);
    CWriteDB_CreateAliasFile(file_name, db, seq_type, gi_file_name, title,
                             alias_type);
}

/// Auxiliary function to convert the enumeration into a string
static string s_AliasFileFilterTypeToString(EAliasFileFilterType e)
{
    string retval;
    switch (e) {
    case eGiList:       retval = "GILIST"; break;
    case eTiList:       retval = "TILIST"; break;
    case eSeqIdList:    retval = "SEQIDLIST"; break;
    case eNoAliasFilterType: break;
    default:                _ASSERT(false); /* Need to add a type here? */
    }
    return retval;
}

static void
s_CreateAliasFilePriv(const string& file_name,
                      const vector<string>& databases,
                      CWriteDB::ESeqType seq_type,
                      const string& gi_file_name,
                      const string& title,
                      EAliasFileFilterType alias_type,
                      const TSeqRange* oid_range = NULL)
{
    bool is_prot(seq_type == CWriteDB::eProtein ? true : false);
    Uint8 dbsize = 0;
    int num_seqs = 0;
    CNcbiOstrstream fnamestr;
    fnamestr << file_name << (is_prot ? ".pal" : ".nal");
    string fname = CNcbiOstrstreamToString(fnamestr);

    ofstream out(fname.c_str());
    out << "#\n# Alias file created " << CTime(CTime::eCurrent).AsString() 
        << "\n#\n";

    if ( !title.empty() ) {
        out << "TITLE " << title << "\n";
    }
    out << "DBLIST ";
    ITERATE(vector< string >, iter, databases) {
        out << "\"" << *iter << "\" ";
    }
    out << "\n";
    if ( !gi_file_name.empty() ) {
        _ASSERT(alias_type != eNoAliasFilterType);
        out << s_AliasFileFilterTypeToString(alias_type) << " " 
            << gi_file_name << "\n";
    } else if (oid_range) {
        out << "FIRST_OID " << oid_range->GetFrom() << "\n"
            << "LAST_OID "  << oid_range->GetToOpen() << "\n";
    }
    out.close();

    if (!s_ComputeNumSequencesAndDbLength(file_name, is_prot, &dbsize, &num_seqs)){
        CDirEntry(fname).Remove();
        string msg("BLASTDB alias file creation failed.  Some referenced files may be missing");
        NCBI_THROW(CSeqDBException, eArgErr, msg);
    };
    if (num_seqs == 0) {
        CDirEntry(fname).Remove();
        CNcbiOstrstream oss;
        oss << "No " << (alias_type == eGiList ? "GI" : "TI") << "s were found"
            << " in BLAST database";
        string msg = CNcbiOstrstreamToString(oss);
        NCBI_THROW(CSeqDBException, eArgErr, msg);
    }

    out.open(fname.c_str(), ios::out|ios::app);
    out << "NSEQ " << num_seqs << "\n";
    out << "LENGTH " << dbsize << "\n";
    out.close();

    s_PrintAliasFileCreationLog(file_name, is_prot, num_seqs);
}

void CWriteDB_CreateAliasFile(const string& file_name,
                              unsigned int num_volumes,
                              CWriteDB::ESeqType seq_type,
                              const string& title)
{
    bool is_prot(seq_type == CWriteDB::eProtein ? true : false);
    string concatenated_blastdb_name;
    vector<string> volume_names(num_volumes, kEmptyStr);
    for (unsigned int i = 0; i < num_volumes; i++) {
        CNcbiOstrstream oss;
        oss << file_name << "." << setfill('0') << setw(2) << i;
        const string vol_name((string)CNcbiOstrstreamToString(oss));
        s_DoesBlastDbExist(vol_name, is_prot);
        volume_names.push_back(vol_name);
        concatenated_blastdb_name += vol_name + " ";
    }

    Uint8 dbsize = 0;
    int num_seqs = 0;
    s_ComputeNumSequencesAndDbLength(concatenated_blastdb_name, is_prot,
                                     &dbsize, &num_seqs);
    CNcbiOstrstream fname;
    fname << file_name << (is_prot ? ".pal" : ".nal");

    ofstream out(((string)CNcbiOstrstreamToString(fname)).c_str());
    out << "#\n# Alias file created " << CTime(CTime::eCurrent).AsString() 
        << "\n#\n";

    if ( !title.empty() ) {
        out << "TITLE " << title << "\n";
    }

    out << "DBLIST ";
    ITERATE(vector<string>, itr, volume_names) {
        out << CDirEntry(*itr).GetName() << " ";
    }
    out << "\n";
    out << "NSEQ " << num_seqs << "\n";
    out << "LENGTH " << dbsize << "\n";
    out.close();
    s_PrintAliasFileCreationLog(concatenated_blastdb_name, is_prot, num_seqs);
}

void CWriteDB_CreateAliasFile(const string& file_name,
                              const vector<string>& databases,
                              CWriteDB::ESeqType seq_type,
                              const string& gi_file_name,
                              const string& title,
                              EAliasFileFilterType alias_type /*= eGiList*/)
{
    s_CreateAliasFilePriv(file_name, databases, seq_type, gi_file_name, title, alias_type);
}

void CWriteDB_CreateAliasFile(const string& file_name,
                              const vector<string>& db_names,
                              CWriteDB::ESeqType seq_type,
                              const TSeqRange& oid_range,
                              const string& title /*= string()*/)
{
    s_CreateAliasFilePriv(file_name, db_names, seq_type, kEmptyStr, title,
                          eNoAliasFilterType, &oid_range);
}

void 
CWriteDB_ConsolidateAliasFiles(const list<string>& alias_files, 
                               bool delete_source_alias_files /* = false */)
{
    if (alias_files.empty()) {
        NCBI_THROW(CWriteDBException,
                   eArgErr,
                   "No alias files available to create group alias file.");
    }

    ofstream out(kSeqDBGroupAliasFileName.c_str());
    out << "# Alias file index for " << CDir::GetCwd() << endl;
    out << "# Generated on " << CTime(CTime::eCurrent).AsString() << " by " 
        << NCBI_CURRENT_FUNCTION << endl;
    out << "#" << endl;

    ITERATE(list<string>, itr, alias_files) {
        ifstream in(itr->c_str());
        if ( !in ) {
            LOG_POST(Warning << *itr << " does not exist, omitting from group alias file");
            continue;
        }
        out << "ALIAS_FILE " << CFile(*itr).GetName() << endl;
        string line;
        while (getline(in, line)) {
            NStr::TruncateSpacesInPlace(line);
            if (line.empty() || NStr::StartsWith(line, "#")) {
                continue;
            }
            out << line << endl;
        }
        out << endl;
    }

    if (delete_source_alias_files) {
        ITERATE(list<string>, itr, alias_files) {
            CFile(*itr).Remove(); // ignore errors
        }
    }
}

void 
CWriteDB_ConsolidateAliasFiles(bool delete_source_alias_files /* = false */)
{
    list<string> alias_files;
    // Using "*.[pn]al" as pattern doesn't work
    FindFiles("*.nal", alias_files, fFF_File);
    FindFiles("*.pal", alias_files, fFF_File);
    CWriteDB_ConsolidateAliasFiles(alias_files, delete_source_alias_files);
}

END_NCBI_SCOPE

