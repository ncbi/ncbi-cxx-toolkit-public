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

class CSeqDBImpl {
public:
    typedef Uint4 TOID;
    
    CSeqDBImpl(const string & db_name_list,
               char           prot_nucl,
               Uint4          oid_begin,
               Uint4          oid_end,
               bool           use_mmap);
    
    ~CSeqDBImpl(void);
    
    Uint4 GetSeqLength(Uint4 oid) const;
    
    Uint4 GetSeqLengthApprox(Uint4 oid) const;
    
    CRef<CBlast_def_line_set> GetHdr(Uint4 oid) const;
    
    char GetSeqType(void) const;
    
    CRef<CBioseq> GetBioseq(Uint4 oid,
                            bool  use_objmgr,
                            bool  insert_ctrlA) const;
    
    Uint4 GetSequence(Uint4 oid, const char ** buffer) const;
    
    Uint4 GetAmbigSeq(Uint4            oid,
                      char         ** buffer,
                      Uint4           nucl_code,
                      ESeqDBAllocType strategy) const;
    
    void RetSequence(const char ** buffer) const;
    
    list< CRef<CSeq_id> > GetSeqIDs(Uint4 oid) const;
    
    string GetTitle(void) const;
    
    string GetDate(void) const;
    
    Uint4 GetNumSeqs(void) const;
    
    Uint8 GetTotalLength(void) const;
    
    Uint4 GetMaxLength(void) const;
    
    bool CheckOrFindOID(Uint4 & next_oid) const;
    
    CSeqDB::EOidListType
    GetNextOIDChunk(TOID         & begin_chunk,
                    TOID         & end_chunk,
                    vector<TOID> & oid_list,
                    Uint4        * oid_state);
    
    const string & GetDBNameList(void) const;
    
private:
    string x_FixString(const string &) const;
    
    Uint4 x_GetNumSeqs(void) const;
    
    Uint8 x_GetTotalLength(void) const;
    
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
};

END_NCBI_SCOPE

#endif // OBJTOOLS_READERS_SEQDB__SEQDBIMPL_HPP

