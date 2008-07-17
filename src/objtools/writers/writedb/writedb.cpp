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

#include <ncbi_pch.hpp>
#include <objtools/writers/writedb/writedb.hpp>
#include "writedb_impl.hpp"
#include "writedb_convert.hpp"
#include <iostream>

BEGIN_NCBI_SCOPE

using namespace std;

// Impl


// CWriteDB

CWriteDB::CWriteDB(const string       & dbname,
                   CWriteDB::ESeqType   seqtype,
                   const string       & title,
                   int                 indices)
    : m_Impl(0)
{
    m_Impl = new CWriteDB_Impl(dbname,
                               seqtype == eProtein,
                               title,
                               (EIndexType)indices);
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
CWriteDB::ExtractBioseqDeflines(const CBioseq & bs)
{
    return CWriteDB_Impl::ExtractBioseqDeflines(bs);
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
                      const string                & options)
{
    return m_Impl->RegisterMaskAlgorithm(program, options);
}

void CWriteDB::SetMaskData(const TMaskedRanges & ranges)
{
    m_Impl->SetMaskData(ranges);
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

void CWriteDB_CreateAliasFile(const string& file_name,
                              const string& db_name,
                              CWriteDB::ESeqType seq_type,
                              const string& gi_file_name,
                              const string& title /* = string() */)
{
    CNcbiOstrstream fname;
    fname << file_name << (seq_type == CWriteDB::eProtein ? ".pal" : ".nal");

    ofstream out(((string)CNcbiOstrstreamToString(fname)).c_str());
    out << "#\n# Alias file created " << CTime(CTime::eCurrent).AsString() 
        << "\n#\n";

    if ( !title.empty() ) {
        out << "TITLE " << title << "\n";
    }
    out << "DBLIST " << db_name << "\n";
    out << "GILIST " << gi_file_name << "\n";
    out.close();
}

END_NCBI_SCOPE

