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


/// @file seqdbimpl.hpp
/// The top level of the private implementation layer for SeqDB.
/// 
/// Defines classes:
///     CSeqDBImplFlush
///     CSeqDBImpl
/// 
/// Implemented for: UNIX, MS-Windows

#include "seqdbvolset.hpp"
#include "seqdbalias.hpp"
#include "seqdboidlist.hpp"

BEGIN_NCBI_SCOPE

using namespace ncbi::objects;


/// CSeqDBImplFlush class
/// 
/// This functor like object provides a call back mechanism to return
/// memory holds to the atlas, from lease objects in the other objects
/// under CSeqDBImpl.  Without this class, CSeqDBAtlas and CSeqDBImpl
/// would be codependant, which is somewhat bad style, and creates
/// annoying cyclical dependencies among the include files.

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


/// CSeqDBImpl class
/// 
/// This is the main implementation layer of the CSeqDB class.  This
/// is seperated from that class so that various implementation
/// details of CSeqDB are kept from the public interface.

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
    
    CRef<CBioseq> GetBioseq(Uint4 oid, Uint4 target_gi) const;
    
    Uint4 GetSequence(Uint4 oid, const char ** buffer) const;
    
    Uint4 GetAmbigSeq(Uint4           oid,
                      char         ** buffer,
                      Uint4           nucl_code,
                      ESeqDBAllocType strategy) const;
    
    void RetSequence(const char ** buffer) const;
    
    list< CRef<CSeq_id> > GetSeqIDs(Uint4 oid) const;
    
    string GetTitle() const;
    
    string GetDate() const;
    
    /// Returns the number of sequences available.
    Uint4 GetNumSeqs() const;
    
    /// Returns the size of the (possibly sparse) OID range.
    Uint4 GetNumOIDs(void) const;
    
    /// Returns the sum of the lengths of all available sequences.
    ///
    /// This uses summary information stored in the database volumes
    /// or alias files.  It provides an exact value, without iterating
    /// over individual sequences.
    Uint8 GetTotalLength() const;
    
    /// Returns the sum of the lengths of all volumes.
    ///
    /// This uses summary information stored in the database volumes
    /// (but not the alias files).  It provides an exact value,
    /// without iterating over individual sequences.  It includes all
    /// OIDs regardless of inclusion by the filtering mechanisms of
    /// the alias files.
    Uint8 GetVolumeLength(void) const;
    
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
    
    /// Find OIDs matching the specified string.
    void AccessionToOids(const string & acc, vector<TOID> & oids) const;
    
    /// Translate a CSeq-id to a list of OIDs.
    void SeqidToOids(const CSeq_id & seqid, vector<TOID> & oids) const;
    
    /// Find the OID corresponding to the offset given in residues,
    /// into the database as a whole.
    Uint4 GetOidAtOffset(Uint4 first_seq, Uint8 residue) const;
    
private:
    string x_FixString(const string &) const;
    
    Uint4 x_GetNumSeqs() const;
    
    Uint4 x_GetNumOIDs() const;
    
    Uint8 x_GetTotalLength() const;
    
    Uint8 x_GetVolumeLength() const;
    
    CSeqDBImplFlush       m_FlushCB;
    mutable CSeqDBAtlas   m_Atlas;
    string                m_DBNames;
    CSeqDBAliasFile       m_Aliases;
    CSeqDBVolSet          m_VolSet;
    CRef<CSeqDBOIDList>   m_OIDList;
    CRef<CSeqDBTaxInfo>   m_TaxInfo;
    Uint4                 m_RestrictBegin;
    Uint4                 m_RestrictEnd;
    CFastMutex            m_OIDLock;
    Uint4                 m_NextChunkOID;
    
    Uint4                 m_NumSeqs;
    Uint4                 m_NumOIDs;
    Uint8                 m_TotalLength;
    Uint8                 m_VolumeLength;
    char                  m_SeqType;
};

END_NCBI_SCOPE

#endif // OBJTOOLS_READERS_SEQDB__SEQDBIMPL_HPP

