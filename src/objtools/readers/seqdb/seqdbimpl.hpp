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

#include "seqdbmempool.hpp"
#include "seqdbvolset.hpp"
#include "seqdbalias.hpp"
#include "seqdboidlist.hpp"

BEGIN_NCBI_SCOPE

using namespace ncbi;
using namespace ncbi::objects;

class CSeqDBImpl {
public:
    CSeqDBImpl(const string & dbpath, const string & db_name_list, char prot_nucl, bool use_mmap);
    
    ~CSeqDBImpl(void);
    
    Int4 GetSeqLength(Uint4 oid);
    
    Int4 GetSeqLengthApprox(Uint4 oid);
    
    CRef<CBlast_def_line_set> GetHdr(Uint4 oid);
    
    char GetSeqType(void);
    
    CRef<CBioseq> GetBioseq(Int4 oid,
                            bool use_objmgr,
                            bool insert_ctrlA);
    
    Int4 GetSequence(Int4 oid, const char ** buffer);
    
    Int4 GetAmbigSeq(Int4 oid, const char ** buffer, bool nucl_code);
    
    void RetSequence(const char ** buffer);
    
    list< CRef<CSeq_id> > GetSeqIDs(Uint4 oid);
    
    string GetTitle(void);
    
    string GetDate(void);
    
    Uint4  GetNumSeqs(void);
    
    Uint8  GetTotalLength(void);
    
    Uint4  GetMaxLength(void);
    
    bool   CheckOrFindOID(Uint4 & next_oid);
    
private:
    CSeqDBMemPool       m_MemPool;
    CSeqDBAliasFile     m_Aliases;
    CSeqDBVolSet        m_VolSet;
    CRef<CSeqDBOIDList> m_OIDList;
};

END_NCBI_SCOPE

#endif // OBJTOOLS_READERS_SEQDB__SEQDBIMPL_HPP

