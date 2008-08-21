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
*  Author: Kevin Bealer
*
*  File Description:
*   Data loader implementation that uses the CRemoteBlast API.
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <objtools/data_loaders/blastdb/remotedb.hpp>
#include <objmgr/impl/tse_loadlock.hpp>
#include <objects/seq/seq__.hpp>
#include <objmgr/impl/tse_split_info.hpp>
#include <corelib/plugin_manager_store.hpp>
#include <objmgr/data_loader_factory.hpp>
#include <corelib/plugin_manager_impl.hpp>
#include <algo/blast/api/remote_blast.hpp>  // for CRemoteBlast::GetSequenceInfo

//=======================================================================
// RemoteBlastDataLoader Public interface 
//

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

CRemoteDb::CRemoteDb(string dbname, CSeqDB::ESeqType seqtype)
    : m_NextOid(101), m_Dbname(dbname), m_Seqtype(seqtype)
{
}

CSeqDB::ESeqType CRemoteDb::GetSequenceType()
{
    return m_Seqtype;
}

int CRemoteDb::GetSeqLength(int oid)
{
    _ASSERT(m_SeqData[oid].found);
    return m_SeqData[oid].length;
}

CRef<CBioseq> CRemoteDb::GetBioseqNoData(int oid, const CSeq_id &)
{
    _ASSERT(m_SeqData[oid].found);
    return m_SeqData[oid].bioseq;
}

CRef<CSeq_data> CRemoteDb::GetSequence(int oid, int begin, int end)
{
    SRemoteDb_SeqData & sdata = m_SeqData[oid];
    
    _ASSERT(sdata.found);
    
    CRef<CSeq_data> slice = sdata.seqdata.GetSlice(begin, end);
    
    if (slice.Empty()) {
        slice = x_FetchData(sdata, begin, end);
    }
    
    return slice;
}

CRef<CSeq_data> CRemoteDb::x_FetchData(SRemoteDb_SeqData & sdata,
                                       int                 begin,
                                       int                 end)
{
    _ASSERT(sdata.found == 1);
    
    char seqtype = (m_Seqtype == CSeqDB::eProtein) ? 'p' : 'n';
    
    // Return types
    
    CRef<CBioseq> bioseq;
    string errors;   // not used
    string warnings; // not used
    
    // This doesn't get any sequence data from the server side.
    
    vector< CRef<objects::CSeq_id> > id_vec;
    int                              seq_length;
    CRef<objects::CSeq_data>         seq_data;
    
    ncbi::blast::CRemoteBlast::GetSequenceInfo(*sdata.id,
                                               m_Dbname,
                                               seqtype,
                                               false,
                                               begin,
                                               end,
                                               bioseq, // these wont..
                                               id_vec, // be filled
                                               seq_length,
                                               seq_data,
                                               errors,
                                               warnings);
    
    // probably should throw an exception, instead... and possibly the
    // caller should run this method two or three times if there is an
    // error here.
    
    _ASSERT(seq_data.NotEmpty());
    _ASSERT(sdata.length == seq_length);
    
    sdata.seqdata.SetSlice(begin, end) = seq_data;
    
    return seq_data;
}

bool CRemoteDb::SeqidToOid(const CSeq_id & id, int & oid, int slice_size)
{
    CRef<CSeq_id> seqid(const_cast<CSeq_id*>(& id));
    
    string idkey = seqid->AsFastaString();
    
    if (m_IdToOid.find(idkey) == m_IdToOid.end()) {
        oid = m_NextOid++;
        
        // Seqids
        
        char seqtype = (m_Seqtype == CSeqDB::eProtein) ? 'p' : 'n';
        
        // Return types
        
        CRef<CBioseq> bioseq;
        string errors;   // not used
        string warnings; // not used
        
        // This doesn't get any sequence data from the server side.
        
        vector< CRef<objects::CSeq_id> > id_vec;
        int                              seq_length;
        CRef<objects::CSeq_data>         seq_data;
        
        ncbi::blast::CRemoteBlast::GetSequenceInfo(*seqid,
                                                   m_Dbname,
                                                   seqtype,
                                                   true,
                                                   0, // no data..
                                                   0, // (for now)
                                                   bioseq,
                                                   id_vec,
                                                   seq_length,
                                                   seq_data,
                                                   errors,
                                                   warnings);
        
        _ASSERT(seq_data.Empty());
        
        if (bioseq.NotEmpty()) {
            SRemoteDb_SeqData & sdata = m_SeqData[oid];
            
            sdata.id     = seqid;
            sdata.bioseq = bioseq;
            sdata.oid    = oid;
            sdata.length = seq_length;
            sdata.idlist.assign(id_vec.begin(), id_vec.end());
            
            sdata.seqdata.SetLength(slice_size, seq_length);
            
            sdata.found  = 1;
            
            m_IdToOid[idkey] = oid;
        }
    } else {
        oid = m_IdToOid[idkey];
    }
    
    return m_SeqData[oid].found;
}

list< CRef<CSeq_id> > CRemoteDb::GetSeqIDs(int oid)
{
    _ASSERT(m_SeqData[oid].found);
    return m_SeqData[oid].idlist;
}

END_SCOPE(objects)
END_NCBI_SCOPE

