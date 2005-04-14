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

/// Helper function to translate enumerated type to character.
///
/// @param seqtype
///   The sequence type (eProtein, eNucleotide, or eUnknown).
/// @return
///   The sequence type as a char ('p', 'n', or '-').

static char s_GetSeqTypeChar(CSeqDB::ESeqType seqtype)
{
    switch(seqtype) {
    case CSeqDB::eProtein:
        return 'p';
    case CSeqDB::eNucleotide:
        return 'n';
    case CSeqDB::eUnknown:
        return '-';
    }
    
    NCBI_THROW(CSeqDBException,
               eArgErr,
               "Invalid sequence type specified.");
}

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
            int            oid_begin,
            int            oid_end,
            bool           use_mmap,
            CSeqDBGiList * gi_list)
{
    CSeqDBImpl * impl = 0;
    
    if (prot_nucl == '-') {
        try {
            prot_nucl = 'p';
            impl = new CSeqDBImpl(dbname,
                                  prot_nucl,
                                  oid_begin,
                                  oid_end,
                                  use_mmap,
                                  gi_list);
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
                              use_mmap,
                              gi_list);
    }
    
    _ASSERT(impl);
    
    return impl;
}

CSeqDB::CSeqDB(const string & dbname, char seqtype)
{
    m_Impl = s_SeqDBInit(dbname,
                         seqtype,
                         0,
                         0,
                         true,
                         0);
}

CSeqDB::CSeqDB(const string & dbname, ESeqType seqtype, CSeqDBGiList * gi_list)
{
    m_Impl = s_SeqDBInit(dbname,
                         s_GetSeqTypeChar(seqtype),
                         0,
                         0,
                         true,
                         gi_list);
}

CSeqDB::CSeqDB(const string & dbname,
               char           seqtype,
               int            oid_begin,
               int            oid_end,
               bool           use_mmap)
{
    m_Impl = s_SeqDBInit(dbname,
                         seqtype,
                         oid_begin,
                         oid_end,
                         use_mmap,
                         0);
}

CSeqDB::CSeqDB(const string & dbname,
               ESeqType       seqtype,
               int            oid_begin,
               int            oid_end,
               bool           use_mmap,
               CSeqDBGiList * gi_list)
{
    m_Impl = s_SeqDBInit(dbname,
                         s_GetSeqTypeChar(seqtype),
                         oid_begin,
                         oid_end,
                         use_mmap,
                         gi_list);
}

int CSeqDB::GetSeqLength(int oid) const
{
    return m_Impl->GetSeqLength(oid);
}

int CSeqDB::GetSeqLengthApprox(int oid) const
{
    return m_Impl->GetSeqLengthApprox(oid);
}

CRef<CBlast_def_line_set> CSeqDB::GetHdr(int oid) const
{
    return m_Impl->GetHdr(oid);
}

char CSeqDB::GetSeqType() const
{
    return m_Impl->GetSeqType();
}

CSeqDB::ESeqType CSeqDB::GetSequenceType() const
{
    switch(m_Impl->GetSeqType()) {
    case 'p':
        return eProtein;
    case 'n':
        return eNucleotide;
    }
    
    NCBI_THROW(CSeqDBException,
               eArgErr,
               "Internal sequence type is not valid.");
}

CRef<CBioseq>
CSeqDB::GetBioseq(int oid) const
{
    return m_Impl->GetBioseq(oid, 0);
}

CRef<CBioseq>
CSeqDB::GetBioseq(int oid, int target_gi) const
{
    return m_Impl->GetBioseq(oid, target_gi);
}

void CSeqDB::RetSequence(const char ** buffer) const
{
    m_Impl->RetSequence(buffer);
}

int CSeqDB::GetSequence(int oid, const char ** buffer) const
{
    return m_Impl->GetSequence(oid, buffer);
}

int CSeqDB::GetAmbigSeq(int oid, const char ** buffer, int nucl_code) const
{
    return m_Impl->GetAmbigSeq(oid,
                               (char **)buffer,
                               nucl_code,
                               (ESeqDBAllocType) 0);
}

int CSeqDB::GetAmbigSeqAlloc(int             oid,
                               char         ** buffer,
                               int             nucl_code,
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

int CSeqDB::GetNumSeqs() const
{
    return m_Impl->GetNumSeqs();
}

int CSeqDB::GetNumOIDs() const
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

int CSeqDB::GetMaxLength() const
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

bool CSeqDB::CheckOrFindOID(int & oid) const
{
    return m_Impl->CheckOrFindOID(oid);
}

CSeqDB::EOidListType
CSeqDB::GetNextOIDChunk(int         & begin,
                        int         & end,
                        vector<int> & lst,
                        int         * state)
{
    return m_Impl->GetNextOIDChunk(begin, end, lst, state);
}

const string & CSeqDB::GetDBNameList() const
{
    return m_Impl->GetDBNameList();
}

list< CRef<CSeq_id> > CSeqDB::GetSeqIDs(int oid) const
{
    return m_Impl->GetSeqIDs(oid);
}

bool CSeqDB::PigToOid(int pig, int & oid) const
{
    return m_Impl->PigToOid(pig, oid);
}

bool CSeqDB::OidToPig(int oid, int & pig) const
{
    return m_Impl->OidToPig(oid, pig);
}

bool CSeqDB::GiToOid(int gi, int & oid) const
{
    return m_Impl->GiToOid(gi, oid);
}

bool CSeqDB::OidToGi(int oid, int & gi) const
{
    return m_Impl->OidToGi(oid, gi);
}

bool CSeqDB::PigToGi(int pig, int & gi) const
{
    int oid(0);
    
    if (m_Impl->PigToOid(pig, oid)) {
        return m_Impl->OidToGi(oid, gi);
    }
    
    return false;
}

bool CSeqDB::GiToPig(int gi, int & pig) const
{
    int oid(0);
    
    if (m_Impl->GiToOid(gi, oid)) {
        return m_Impl->OidToPig(oid, pig);
    }
    
    return false;
}

void CSeqDB::AccessionToOids(const string & acc, vector<int> & oids) const
{
    m_Impl->AccessionToOids(acc, oids);
}

void CSeqDB::SeqidToOids(const CSeq_id & seqid, vector<int> & oids) const
{
    m_Impl->SeqidToOids(seqid, oids);
}

void CSeqDB::SetMemoryBound(Uint8 membound, Uint8 slice_size)
{
    m_Impl->SetMemoryBound(membound, slice_size);
}

int CSeqDB::GetOidAtOffset(int first_seq, Uint8 residue) const
{
    return m_Impl->GetOidAtOffset(first_seq, residue);
}

CSeqDBIter::CSeqDBIter(const CSeqDB * db, int oid)
    : m_DB    (db),
      m_OID   (oid),
      m_Data  (0),
      m_Length((int) -1)
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
        m_Length = -1;
    }
    
    return *this;
}

CRef<CBioseq>
CSeqDB::GiToBioseq(int gi) const
{
    int oid(0);
    CRef<CBioseq> bs;
    
    if (m_Impl->GiToOid(gi, oid)) {
        bs = m_Impl->GetBioseq(oid, 0);
    }
    
    return bs;
}

CRef<CBioseq>
CSeqDB::PigToBioseq(int pig) const
{
    int oid(0);
    CRef<CBioseq> bs;
    
    if (m_Impl->PigToOid(pig, oid)) {
        bs = m_Impl->GetBioseq(oid, 0);
    }
    
    return bs;
}

CRef<CBioseq>
CSeqDB::SeqidToBioseq(const CSeq_id & seqid) const
{
    vector<int> oids;
    CRef<CBioseq> bs;
    
    m_Impl->SeqidToOids(seqid, oids);
    
    if (! oids.empty()) {
        bs = m_Impl->GetBioseq(oids[0], 0);
    }
    
    return bs;
}

void
CSeqDB::FindVolumePaths(const string   & dbname,
                        char             seqtype,
                        vector<string> & paths)
{
    bool done = false;
    
    if ((seqtype == 'p') || (seqtype == 'n')) {
        CSeqDBImpl::FindVolumePaths(dbname, seqtype, paths);
    } else {
        try {
            CSeqDBImpl::FindVolumePaths(dbname, 'p', paths);
            done = true;
        }
        catch(...) {
            done = false;
        }
        
        if (! done) {
            CSeqDBImpl::FindVolumePaths(dbname, 'n', paths);
        }
    }
}

void
CSeqDB::FindVolumePaths(const string   & dbname,
                        ESeqType         seqtype,
                        vector<string> & paths)
{
    bool done = false;
    
    if (seqtype == CSeqDB::eProtein) {
        CSeqDBImpl::FindVolumePaths(dbname, 'p', paths);
    } else if (seqtype == CSeqDB::eNucleotide) {
        CSeqDBImpl::FindVolumePaths(dbname, 'n', paths);
    } else {
        try {
            CSeqDBImpl::FindVolumePaths(dbname, 'p', paths);
            done = true;
        }
        catch(...) {
            done = false;
        }
        
        if (! done) {
            CSeqDBImpl::FindVolumePaths(dbname, 'n', paths);
        }
    }
}

void
CSeqDB::FindVolumePaths(vector<string> & paths) const
{
    m_Impl->FindVolumePaths(paths);
}

void
CSeqDB::GetGis(int oid, vector<int> & gis, bool append) const
{
    // This could be done a little faster at a lower level, but not
    // necessarily by too much.  If this operation is important to
    // performance, that decision can be revisited.
    
    list< CRef<CSeq_id> > seqids = GetSeqIDs(oid);
    
    if (! append) {
        gis.clear();
    }
    
    ITERATE(list< CRef<CSeq_id> >, seqid, seqids) {
        if ((**seqid).IsGi()) {
            gis.push_back((**seqid).GetGi());
        }
    }
}

void CSeqDB::SetIterationRange(int oid_begin, int oid_end)
{
    m_Impl->SetIterationRange(oid_begin, oid_end);
}

END_NCBI_SCOPE

