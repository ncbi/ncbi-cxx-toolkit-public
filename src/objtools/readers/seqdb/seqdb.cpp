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

/// @file seqdb.cpp
/// Implementation for the CSeqDB class, the top level class for SeqDB.

#include <ncbi_pch.hpp>
#include <objtools/readers/seqdb/seqdb.hpp>
#include "seqdbimpl.hpp"

BEGIN_NCBI_SCOPE

/// Helper function to build private implementation object.
///
/// This method builds and returns the object which implements the
/// functionality for the CSeqDB API.  If this method is called with
/// '-' for the sequence data type, protein will be tried first, then
/// nucleotide.  The created object will be returned.  Either
/// kSeqTypeProt for a protein database, kSeqTypeNucl for nucleotide,
/// or kSeqTypeUnkn to less this function try one then the other.
/// 
/// @param dbname
///   A list of database or alias names, seperated by spaces.
/// @param prot_nucl
///   Specify whether to use protein, nucleotide, or either.
/// @param oid_begin
///   Iterator will skip OIDs less than this value.  Only OIDs
///   found in the OID lists (if any) will be returned.
/// @param oid_end
///   Iterator will return up to (but not including) this OID.
/// @param use_mmap
///   If kSeqDBMMap is specified (the default), memory mapping is
///   attempted.  If kSeqDBNoMMap is specified, or memory mapping
///   fails, this platform does not support it, the less efficient
///   read and write calls are used instead.
/// @return
///   The CSeqDBImpl object that was created.

static CSeqDBImpl *
s_SeqDBInit(const string & dbname,
            char           prot_nucl,
            Uint4          oid_begin,
            Uint4          oid_end,
            bool           use_mmap)
{
    CSeqDBImpl * impl = 0;
    
    if (prot_nucl == '-') {
        try {
            prot_nucl = 'p';
            impl = new CSeqDBImpl(dbname,
                                  prot_nucl,
                                  oid_begin,
                                  oid_end,
                                  use_mmap);
        }
        catch(CSeqDBException &) {
            prot_nucl = 'n';
        }
    }
    
    if (! impl) {
        impl = new CSeqDBImpl(dbname,
                              prot_nucl,
                              oid_begin,
                              oid_end,
                              use_mmap);
    }
    
    _ASSERT(impl);
    
    return impl;
}

CSeqDB::CSeqDB(const string & dbname, char prot_nucl)
{
    m_Impl = s_SeqDBInit(dbname,
                         prot_nucl,
                         0,
                         0,
                         true);
}

CSeqDB::CSeqDB(const string & dbname,
               char           prot_nucl,
               TOID           oid_begin,
               TOID           oid_end,
               bool           use_mmap)
{
    m_Impl = s_SeqDBInit(dbname,
                         prot_nucl,
                         oid_begin,
                         oid_end,
                         use_mmap);
}

Uint4 CSeqDB::GetSeqLength(TOID oid) const
{
    return m_Impl->GetSeqLength(oid);
}

Uint4 CSeqDB::GetSeqLengthApprox(TOID oid) const
{
    return m_Impl->GetSeqLengthApprox(oid);
}

CRef<CBlast_def_line_set> CSeqDB::GetHdr(TOID oid) const
{
    return m_Impl->GetHdr(oid);
}

char CSeqDB::GetSeqType() const
{
    return m_Impl->GetSeqType();
}

CRef<CBioseq>
CSeqDB::GetBioseq(TOID oid) const
{
    return m_Impl->GetBioseq(oid, 0);
}

CRef<CBioseq>
CSeqDB::GetBioseq(TOID oid, TGI target_gi) const
{
    return m_Impl->GetBioseq(oid, target_gi);
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
    return m_Impl->GetAmbigSeq(oid,
                               (char **)buffer,
                               nucl_code,
                               (ESeqDBAllocType) 0);
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

string CSeqDB::GetTitle() const
{
    return m_Impl->GetTitle();
}

string CSeqDB::GetDate() const
{
    return m_Impl->GetDate();
}

Uint4 CSeqDB::GetNumSeqs() const
{
    return m_Impl->GetNumSeqs();
}

Uint4 CSeqDB::GetNumOIDs() const
{
    return m_Impl->GetNumOIDs();
}

Uint8 CSeqDB::GetTotalLength() const
{
    return m_Impl->GetTotalLength();
}

Uint8 CSeqDB::GetVolumeLength() const
{
    return m_Impl->GetVolumeLength();
}

Uint4 CSeqDB::GetMaxLength() const
{
    return m_Impl->GetMaxLength();
}

CSeqDB::~CSeqDB()
{
    if (m_Impl)
        delete m_Impl;
}

CSeqDBIter CSeqDB::Begin() const
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

const string & CSeqDB::GetDBNameList() const
{
    return m_Impl->GetDBNameList();
}

list< CRef<CSeq_id> > CSeqDB::GetSeqIDs(TOID oid) const
{
    return m_Impl->GetSeqIDs(oid);
}

bool CSeqDB::PigToOid(TPIG pig, TOID & oid) const
{
    return m_Impl->PigToOid(pig, oid);
}

bool CSeqDB::OidToPig(TOID oid, TPIG & pig) const
{
    return m_Impl->OidToPig(oid, pig);
}

bool CSeqDB::GiToOid(TGI gi, TOID & oid) const
{
    return m_Impl->GiToOid(gi, oid);
}

bool CSeqDB::OidToGi(TOID oid, TGI & gi) const
{
    return m_Impl->OidToGi(oid, gi);
}

bool CSeqDB::PigToGi(TPIG pig, TGI & gi) const
{
    Uint4 oid(0);
    
    if (m_Impl->PigToOid(pig, oid)) {
        return m_Impl->OidToGi(oid, gi);
    }
    
    return false;
}

bool CSeqDB::GiToPig(TGI gi, TPIG & pig) const
{
    Uint4 oid(0);
    
    if (m_Impl->GiToOid(gi, oid)) {
        return m_Impl->OidToPig(oid, pig);
    }
    
    return false;
}

void CSeqDB::AccessionToOids(const string & acc, vector<TOID> & oids) const
{
    m_Impl->AccessionToOids(acc, oids);
}

void CSeqDB::SeqidToOids(const CSeq_id & seqid, vector<TOID> & oids) const
{
    m_Impl->SeqidToOids(seqid, oids);
}

void CSeqDB::SetMemoryBound(Uint8 membound, Uint8 slice_size)
{
    m_Impl->SetMemoryBound(membound, slice_size);
}

Uint4 CSeqDB::GetOidAtOffset(Uint4 first_seq, Uint8 residue) const
{
    return m_Impl->GetOidAtOffset(first_seq, residue);
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

CSeqDBIter & CSeqDBIter::operator++()
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

CRef<CBioseq>
CSeqDB::GiToBioseq(TGI gi) const
{
    TOID oid(0);
    CRef<CBioseq> bs;
    
    if (m_Impl->GiToOid(gi, oid)) {
        bs = m_Impl->GetBioseq(oid, 0);
    }
    
    return bs;
}

CRef<CBioseq>
CSeqDB::PigToBioseq(TPIG pig) const
{
    TOID oid(0);
    CRef<CBioseq> bs;
    
    if (m_Impl->PigToOid(pig, oid)) {
        bs = m_Impl->GetBioseq(oid, 0);
    }
    
    return bs;
}

CRef<CBioseq>
CSeqDB::SeqidToBioseq(const CSeq_id & seqid) const
{
    vector<TOID> oids;
    CRef<CBioseq> bs;
    
    m_Impl->SeqidToOids(seqid, oids);
    
    if (! oids.empty()) {
        bs = m_Impl->GetBioseq(oids[0], 0);
    }
    
    return bs;
}

END_NCBI_SCOPE

