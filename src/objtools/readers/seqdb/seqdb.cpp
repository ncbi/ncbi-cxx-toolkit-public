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

/// CSeqDB class
/// 
/// This class defines access to the database component by calling
/// methods on objects which represent the various database files,
/// such as the index file, the header file, and the sequence file.

#include <objtools/readers/seqdb/seqdb.hpp>
#include "seqdbimpl.hpp"

BEGIN_NCBI_SCOPE

CSeqDB::CSeqDB(const string & dbname, char prot_nucl, bool use_mmap)
{
    m_Impl = new CSeqDBImpl(dbname, prot_nucl, use_mmap);
}

Int4 CSeqDB::GetSeqLength(Uint4 oid)
{
    return m_Impl->GetSeqLength(oid);
}

Int4 CSeqDB::GetSeqLengthApprox(Uint4 oid)
{
    return m_Impl->GetSeqLengthApprox(oid);
}

CRef<CBlast_def_line_set> CSeqDB::GetHdr(Uint4 oid)
{
    return m_Impl->GetHdr(oid);
}

char CSeqDB::GetSeqType(void)
{
    return m_Impl->GetSeqType();
}

CRef<CBioseq>
CSeqDB::GetBioseq(TOID oid,
                  bool use_objmgr,
                  bool insert_ctrlA)
{
    return m_Impl->GetBioseq(oid, use_objmgr, insert_ctrlA);
}

void CSeqDB::RetSequence(const char ** buffer)
{
    m_Impl->RetSequence(buffer);
}

Int4 CSeqDB::GetSequence(TOID oid, const char ** buffer)
{
    return m_Impl->GetSequence(oid, buffer);
}

Int4 CSeqDB::GetAmbigSeq(TOID oid, const char ** buffer, Uint4 nucl_code)
{
    return m_Impl->GetAmbigSeq(oid, buffer, nucl_code);
}

string CSeqDB::GetTitle(void)
{
    return m_Impl->GetTitle();
}

string CSeqDB::GetDate(void)
{
    return m_Impl->GetDate();
}

Uint4 CSeqDB::GetNumSeqs(void)
{
    return m_Impl->GetNumSeqs();
}

Uint8  CSeqDB::GetTotalLength(void)
{
    return m_Impl->GetTotalLength();
}

Uint4  CSeqDB::GetMaxLength(void)
{
    return m_Impl->GetMaxLength();
}

CSeqDB::~CSeqDB()
{
    if (m_Impl)
        delete m_Impl;
}

CSeqDBIter CSeqDB::Begin(void)
{
    return CSeqDBIter(this, 0);
}

bool CSeqDB::CheckOrFindOID(TOID & oid)
{
    return m_Impl->CheckOrFindOID(oid);
}

list< CRef<CSeq_id> > CSeqDB::GetSeqIDs(TOID oid)
{
    return m_Impl->GetSeqIDs(oid);
}

CSeqDBIter::CSeqDBIter(CSeqDB * db, TOID oid)
    : m_DB    (db),
      m_OID   (oid),
      m_Data  (0),
      m_Length((TOID) -1)
{
    if (m_DB->CheckOrFindOID(m_OID)) {
        x_GetSeq();
    }
}

CSeqDBIter & CSeqDBIter::operator++(void)
{
    x_RetSeq();
    
    ++m_OID;
    
    if (m_DB->CheckOrFindOID(m_OID)) {
        x_GetSeq();
    } else {
        m_Length = (Uint4)-1;
    }
    
    return *this;
}

END_NCBI_SCOPE

