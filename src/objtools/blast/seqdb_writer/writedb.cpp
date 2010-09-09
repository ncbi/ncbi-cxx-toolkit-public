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
/// @param gi_file_name File name of the GI file to apply as a filter to the
/// BLAST database, if empty, it's ignored [in]
/// @param dbsize (Approximate) number of letters in the BLAST DB [out]
/// @param num_seqs_found Number of sequences found in the dbname, or the
/// number of sequences in the intersection between the dbname and the GIs in
/// the gi_file_name (if applicable) [out]
/// @param num_seqs_in_gifile Number of sequences in the gi_file_name (if
/// applicable [out]
static void
s_ComputeNumSequencesAndDbLength(const string& dbname,
                                 bool is_prot,
                                 Uint8* dbsize,
                                 int* num_seqs_found,
                                 const string& gi_file_name = kEmptyStr,
                                 int* num_seqs_in_gifile = NULL)
{
    _ASSERT((dbsize != NULL));
    _ASSERT(num_seqs_found != NULL);
    *dbsize = 0u;
    *num_seqs_found = 0u;

    CSeqDB::ESeqType dbtype(is_prot ? CSeqDB::eProtein : CSeqDB::eNucleotide);
    CRef<CSeqDBFileGiList> gilist;
    if ( !gi_file_name.empty() ) {
        gilist.Reset(new CSeqDBFileGiList(gi_file_name));
        if (num_seqs_in_gifile) {
            *num_seqs_in_gifile = gilist->Size();
        }
    }

    CRef<CSeqDB> dbhandle(new CSeqDB(dbname, dbtype, gilist.GetPointer()));
    dbhandle->GetTotals(CSeqDB::eFilteredAll, num_seqs_found, dbsize, true);
    if (*num_seqs_found == 0) {
        string msg("No GIs in ");
        msg += gi_file_name + " were found in ";
        msg += (is_prot ? "protein " : "nucleotide ");
        msg += dbname + " BLAST database";
        NCBI_THROW(CSeqDBException, eArgErr, msg);
    }
}

static void
s_PrintAliasFileCreationLog(const string& dbname,
                            bool is_protein,
                            int num_seqs_found,
                            const string& gi_file_name = kEmptyStr,
                            int num_seqs_in_gifile = 0)
{
    if ( !gi_file_name.empty() ) {
        LOG_POST("Created " << (is_protein ? "protein " : "nucleotide ") <<
            dbname << " BLAST (alias) database with " << num_seqs_found 
            << " sequences (out of " << num_seqs_in_gifile << " in " 
            << gi_file_name << ", " << setprecision(0) << fixed << 
            (num_seqs_found*100.0/num_seqs_in_gifile) << "% found)");
    } else {
        LOG_POST("Created " << (is_protein ? "protein " : "nucleotide ") <<
            "BLAST (alias) database over " << dbname << " with " << 
            num_seqs_found << " sequences");
    }
}

void CWriteDB_CreateAliasFile(const string& file_name,
                              const string& db_name,
                              CWriteDB::ESeqType seq_type,
                              const string& gi_file_name,
                              const string& title)
{
    bool is_prot(seq_type == CWriteDB::eProtein ? true : false);
    s_DoesBlastDbExist(db_name, is_prot);
    Uint8 dbsize = 0;
    int num_seqs = 0;
    int num_gis = 0;
    s_ComputeNumSequencesAndDbLength(db_name, is_prot, &dbsize, &num_seqs, 
                                     gi_file_name, &num_gis);


    CNcbiOstrstream fname;
    fname << file_name << (is_prot ? ".pal" : ".nal");

    ofstream out(((string)CNcbiOstrstreamToString(fname)).c_str());
    out << "#\n# Alias file created " << CTime(CTime::eCurrent).AsString() 
        << "\n#\n";

    if ( !title.empty() ) {
        out << "TITLE " << title << "\n";
    }
    out << "DBLIST " << db_name << "\n";
    if ( !gi_file_name.empty() ){
        out << "GILIST " << gi_file_name << "\n";
    }
    out << "NSEQ " << num_seqs << "\n";
    out << "LENGTH " << dbsize << "\n";
    out.close();
    s_PrintAliasFileCreationLog(file_name, is_prot, num_seqs, gi_file_name,
                                num_gis);
}

void CWriteDB_CreateAliasFile(const string& file_name,
                              const vector<string>& databases,
                              CWriteDB::ESeqType seq_type,
                              const string& gi_file_name,
                              const string& title)
{
    string concatenated_blastdb_name;
    bool is_prot(seq_type == CWriteDB::eProtein ? true : false);
    ITERATE(vector<string>, itr, databases) {
        concatenated_blastdb_name += *itr + " ";
        s_DoesBlastDbExist(*itr, is_prot);
    }

    Uint8 dbsize = 0;
    int num_seqs = 0;
    int num_gis = 0;
    s_ComputeNumSequencesAndDbLength(concatenated_blastdb_name, is_prot,
                                     &dbsize, &num_seqs, 
                                     gi_file_name, &num_gis);
    CNcbiOstrstream fname;
    fname << file_name << (is_prot ? ".pal" : ".nal");

    ofstream out(((string)CNcbiOstrstreamToString(fname)).c_str());
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
        out << "GILIST " << gi_file_name << "\n";
    }
    out << "NSEQ " << num_seqs << "\n";
    out << "LENGTH " << dbsize << "\n";
    out.close();
    s_PrintAliasFileCreationLog(file_name, is_prot, num_seqs, gi_file_name,
                                num_gis);
}

void CWriteDB_CreateAliasFile(const string& file_name,
                              unsigned int num_volumes,
                              CWriteDB::ESeqType seq_type,
                              const string& title)
{
    bool is_prot(seq_type == CWriteDB::eProtein ? true : false);
    string concatenated_blastdb_name;
    if (num_volumes >= 101) {
        NCBI_THROW(CWriteDBException,
                   eArgErr,
                   "No more than 100 volumes are supported");
    }
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
        out << *itr << " ";
    }
    out << "\n";
    out << "NSEQ " << num_seqs << "\n";
    out << "LENGTH " << dbsize << "\n";
    out.close();
    s_PrintAliasFileCreationLog(concatenated_blastdb_name, is_prot, num_seqs);
}

END_NCBI_SCOPE

