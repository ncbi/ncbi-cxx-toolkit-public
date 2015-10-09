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

/// @file seqdbexpert.cpp
/// Implementation for the CSeqDBExpert class.

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include <objtools/blast/seqdb_reader/seqdbexpert.hpp>
#include "seqdbimpl.hpp"

BEGIN_NCBI_SCOPE

CSeqDBExpert::CSeqDBExpert(const string & dbname,
                           ESeqType seqtype,
                           CSeqDBGiList * gi_list)
    : CSeqDB(dbname, seqtype, gi_list)
{
}

CSeqDBExpert::CSeqDBExpert(const string & dbname,
                               ESeqType       seqtype,
                               int            oid_begin,
                               int            oid_end,
                               bool           use_mmap,
                               CSeqDBGiList * gi_list)
    : CSeqDB(dbname, seqtype, oid_begin, oid_end, use_mmap, gi_list)
{
}

CSeqDBExpert::CSeqDBExpert()
    : CSeqDB()
{
}

CSeqDBExpert::~CSeqDBExpert()
{
}

void CSeqDBExpert::GetRawSeqAndAmbig(int           oid,
                                     const char ** buf,
                                     int         * seq_length,
                                     int         * ambig_length) const
{
    m_Impl->Verify();
    m_Impl->GetRawSeqAndAmbig(oid, buf, seq_length, ambig_length);
    m_Impl->Verify();
}

void CSeqDBExpert::GetGiBounds(TGi * low_id,
                               TGi * high_id,
                               int * count)
{
    m_Impl->Verify();
    m_Impl->GetGiBounds(low_id, high_id, count);
    m_Impl->Verify();
}

void CSeqDBExpert::GetPigBounds(int * low_id,
                                int * high_id,
                                int * count)
{
    m_Impl->Verify();
    m_Impl->GetPigBounds(low_id, high_id, count);
    m_Impl->Verify();
}

void CSeqDBExpert::GetStringBounds(string * low_id,
                                   string * high_id,
                                   int * count)
{
    m_Impl->Verify();
    m_Impl->GetStringBounds(low_id, high_id, count);
    m_Impl->Verify();
}

void CSeqDBExpert::Verify()
{
    m_Impl->Verify();
}

unsigned CSeqDBExpert::GetSequenceHash(int oid)
{
    m_Impl->Verify();
    unsigned h = m_Impl->GetSequenceHash(oid);
    m_Impl->Verify();

    return h;
}

void CSeqDBExpert::HashToOids(unsigned hash, vector<int> & oids)
{
    m_Impl->Verify();
    m_Impl->HashToOids(hash, oids);
    m_Impl->Verify();
}

END_NCBI_SCOPE

