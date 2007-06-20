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
#include <iostream>

BEGIN_NCBI_SCOPE

using namespace std;

// Impl


// CWriteDB

CWriteDB::CWriteDB(const string       & dbname,
                   CWriteDB::ESeqType   seqtype,
                   const string       & title,
                   EIndexType           indices)
    : m_Impl(0)
{
    bool trace_index = indices == eFullWithTrace;
    
    m_Impl = new CWriteDB_Impl(dbname,
                               seqtype == CWriteDB::eProtein,
                               title,
                               indices,
                               trace_index);
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

END_NCBI_SCOPE

