#ifndef OBJTOOLS_READERS_SEQDB__SEQDBIMPL_HPP
#define OBJTOOLS_READERS_SEQDB__SEQDBIMPL_HPP

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

/// CSeqDBImpl class
/// 
/// This is the main implementation layer of the CSeqDB class.  This
/// is seperated from that class so that various implementation
/// details of CSeqDB are kept from the public interface.

#include "seqdbvolset.hpp"
#include "seqdbalias.hpp"
#include "seqdboidlist.hpp"

BEGIN_NCBI_SCOPE

using namespace ncbi::objects;

class CSeqDBImplFlush : public CSeqDBFlushCB {
public:
    CSeqDBImplFlush(void)
        : m_Impl(0)
    {
    }
    
    virtual void SetImpl(class CSeqDBImpl * impl)
    {
        m_Impl = impl;
    }
    
    virtual void operator()(void);
    
private:
    CSeqDBImpl * m_Impl;
};

class CSeqDBImpl {
public:
    typedef Uint4 TOID;
    
    CSeqDBImpl(const string & db_name_list,
               char           prot_nucl,
               Uint4          oid_begin,
               Uint4          oid_end,
               bool           use_mmap);
    
    ~CSeqDBImpl();
    
    Uint4 GetSeqLength(Uint4 oid) const;
    
    Uint4 GetSeqLengthApprox(Uint4 oid) const;
    
    CRef<CBlast_def_line_set> GetHdr(Uint4 oid) const;
    
    char GetSeqType() const;
    
    CRef<CBioseq> GetBioseq(Uint4 oid,
                            bool  use_objmgr,
                            bool  insert_ctrlA) const;
    
    Uint4 GetSequence(Uint4 oid, const char ** buffer) const;
    
    Uint4 GetAmbigSeq(Uint4           oid,
                      char         ** buffer,
                      Uint4           nucl_code,
                      ESeqDBAllocType strategy) const;
    
    void RetSequence(const char ** buffer) const;
    
    list< CRef<CSeq_id> > GetSeqIDs(Uint4 oid) const;
    
    string GetTitle() const;
    
    string GetDate() const;
    
    Uint4 GetNumSeqs() const;
    
    Uint8 GetTotalLength() const;
    
    Uint4 GetMaxLength() const;
    
    bool CheckOrFindOID(Uint4 & next_oid) const;
    
    CSeqDB::EOidListType
    GetNextOIDChunk(TOID         & begin_chunk,
                    TOID         & end_chunk,
                    vector<TOID> & oid_list,
                    Uint4        * oid_state);
    
    const string & GetDBNameList() const;
    
    void SetMemoryBound(Uint8 membound, Uint8 slicesize)
    {
        m_Atlas.SetMemoryBound(membound, slicesize);
    }
    
    void FlushSeqMemory(void);
    
    /// Translate a PIG to an OID.
    bool PigToOid(Uint4 pig, Uint4 & oid) const;
    
    /// Translate a PIG to an OID.
    bool OidToPig(Uint4 oid, Uint4 & pig) const;
    
    /// Translate a GI to an OID.
    bool GiToOid(Uint4 gi, Uint4 & oid) const;
    
    /// Translate a GI to an OID.
    bool OidToGi(Uint4 oid, Uint4 & gi) const;
    
private:
    string x_FixString(const string &) const;
    
    Uint4 x_GetNumSeqs() const;
    
    Uint8 x_GetTotalLength() const;
    
    CSeqDBImplFlush       m_FlushCB;
    mutable CSeqDBAtlas   m_Atlas;
    string                m_DBNames;
    CSeqDBAliasFile       m_Aliases;
    CSeqDBVolSet          m_VolSet;
    CRef<CSeqDBOIDList>   m_OIDList;
    Uint4                 m_RestrictBegin;
    Uint4                 m_RestrictEnd;
    CFastMutex            m_OIDLock;
    Uint4                 m_NextChunkOID;
    
    Uint4                 m_NumSeqs;
    Uint8                 m_TotalLength;
    char                  m_SeqType;
};

END_NCBI_SCOPE

#endif // OBJTOOLS_READERS_SEQDB__SEQDBIMPL_HPP

