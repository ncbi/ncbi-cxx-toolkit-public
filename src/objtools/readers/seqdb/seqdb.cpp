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

#include <ncbi_pch.hpp>
#include <objtools/readers/seqdb/seqdb.hpp>
#include "seqdbimpl.hpp"

BEGIN_NCBI_SCOPE

CSeqDB::CSeqDB(const string & dbname, char prot_nucl)
{
    m_Impl = new CSeqDBImpl(dbname, prot_nucl, 0, 0, true);
}

CSeqDB::CSeqDB(const string & dbname,
               char           prot_nucl,
               Uint4          oid_begin,
               Uint4          oid_end,
               bool           use_mmap)
{
    m_Impl = new CSeqDBImpl(dbname, prot_nucl, oid_begin, oid_end, use_mmap);
}

Uint4 CSeqDB::GetSeqLength(TOID oid) const
{
    return m_Impl->GetSeqLength(oid);
}

Uint4 CSeqDB::GetSeqLengthApprox(TOID oid) const
{
    return m_Impl->GetSeqLengthApprox(oid);
}

CRef<CBlast_def_line_set> CSeqDB::GetHdr(Uint4 oid) const
{
    return m_Impl->GetHdr(oid);
}

char CSeqDB::GetSeqType(void) const
{
    return m_Impl->GetSeqType();
}

CRef<CBioseq>
CSeqDB::GetBioseq(TOID oid,
                  bool use_objmgr,
                  bool insert_ctrlA) const
{
    return m_Impl->GetBioseq(oid, use_objmgr, insert_ctrlA);
}

void CSeqDB::RetSequence(const char ** buffer) const
{
    m_Impl->RetSequence(buffer);
}

Uint4 CSeqDB::GetSequence(TOID oid, const char ** buffer) const
{
    return m_Impl->GetSequence(oid, buffer);
}

Uint4 CSeqDB::GetAmbigSeq(TOID oid, const char ** buffer, Uint4 nucl_code) const
{
    return m_Impl->GetAmbigSeq(oid, (char **)buffer, nucl_code, (ESeqDBAllocType) 0);
}

Uint4 CSeqDB::GetAmbigSeqAlloc(TOID            oid,
                              char         ** buffer,
                              Uint4           nucl_code,
                              ESeqDBAllocType strategy) const
{
    if ((strategy != eMalloc) && (strategy != eNew)) {
        NCBI_THROW(CSeqDBException,
                   eArgErr,
                   "Invalid allocation strategy specified.");
    }
    
    return m_Impl->GetAmbigSeq(oid, buffer, nucl_code, strategy);
}

string CSeqDB::GetTitle(void) const
{
    return m_Impl->GetTitle();
}

string CSeqDB::GetDate(void) const
{
    return m_Impl->GetDate();
}

Uint4 CSeqDB::GetNumSeqs(void) const
{
    return m_Impl->GetNumSeqs();
}

Uint8  CSeqDB::GetTotalLength(void) const
{
    return m_Impl->GetTotalLength();
}

Uint4  CSeqDB::GetMaxLength(void) const
{
    return m_Impl->GetMaxLength();
}

CSeqDB::~CSeqDB()
{
    if (m_Impl)
        delete m_Impl;
}

CSeqDBIter CSeqDB::Begin(void) const
{
    return CSeqDBIter(this, 0);
}

bool CSeqDB::CheckOrFindOID(TOID & oid) const
{
    return m_Impl->CheckOrFindOID(oid);
}

CSeqDB::EOidListType
CSeqDB::GetNextOIDChunk(TOID         & begin,
                        TOID         & end,
                        vector<TOID> & lst,
                        Uint4        * state)
{
    return m_Impl->GetNextOIDChunk(begin, end, lst, state);
}

const string & CSeqDB::GetDBNameList(void) const
{
    return m_Impl->GetDBNameList();
}

list< CRef<CSeq_id> > CSeqDB::GetSeqIDs(TOID oid) const
{
    return m_Impl->GetSeqIDs(oid);
}

CSeqDBIter::CSeqDBIter(const CSeqDB * db, TOID oid)
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

