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
* File Description:
*     Code to build a database given various sources of sequence data.
*/

#ifndef _MULTISOURCE__BUILD_DB_HPP_
#define _MULTISOURCE__BUILD_DB_HPP_

#include <corelib/ncbistd.hpp>

// Blast databases
#include <objtools/readers/seqdb/seqdbexpert.hpp>

// ObjMgr
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>

// Local
#include "taxid_set.hpp"
#include "multisource_util.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);

class CBuildDatabase : public CObject {
public:
    CBuildDatabase(const string & dbname,
                   const string & title,
                   bool           is_protein,
                   ostream      * logfile);
    
    void SetTaxids(CTaxIdSet & taxids);
    
    void SetMaskLetters(const string & mask_letters);
    
    void SetSourceDb(const string & src_db_name);
    
    void SetSourceDb(CRef<CSeqDBExpert> src_db);
    
    void SetKeepTaxIds(bool keep_taxids);
    
    void SetLinkouts(const TLinkoutMap & linkouts,
                     bool                keep_links);
    
    void SetMembBits(const TLinkoutMap & membbits,
                     bool                keep_mbits);
    
    bool Build(const vector<string> & ids,
               CNcbiIstream         * fasta_file);
    
    void SetUseRemote(bool use_remote)
    {
        m_UseRemote = use_remote;
    }
    
    void SetVerbosity(bool v)
    {
        m_Verbose = v;
    }
    
private:
    CScope & x_GetScope();
    
    void x_DupLocal();
    
    bool x_GetGiLocal(const string & acc,
                      int          & oid,
                      int          & gi);
    
    void x_ResolveRemoteId(CRef<CSeq_id> & seqid, int & gi);
    
    CRef<CInputGiList> x_ResolveGis(const vector<string> & ids);
    
    void x_EditAndAddBioseq(CConstRef<CBioseq>   bs,
                            CSeqVector         * sv);
    
    bool x_AddRemoteSequences(CInputGiList & gi_list);
    
    bool x_AddFastaSequences(CNcbiIstream & fasta_file);
    
    bool x_ReportUnresolvedIds(const CInputGiList & gi_list) const;
    
    void x_SetLinkAndMbit(CRef<CBlast_def_line_set> headers);
    
    void x_AddOneRemoteSequence(const CSeq_id & seqid,
                                bool          & found_all,
                                bool          & error);
    
    bool x_ResolveFromSource(const string & acc, CRef<CSeq_id> & id);
    
    bool m_IsProtein;
    
    bool m_KeepTaxIds;
    
    bool      m_KeepLinks;
    TIdToBits m_Id2Links;
    
    bool      m_KeepMbits;
    TIdToBits m_Id2Mbits;
    
    CRef<CObjectManager> m_ObjMgr;
    CRef<CScope>         m_Scope;
    CRef<CTaxIdSet>      m_Taxids;
    CRef<CWriteDB>       m_OutputDb;
    CRef<CSeqDBExpert>   m_SourceDb;
    
    ostream & m_LogFile;
    
    bool m_UseRemote;
    
    int m_DeflineCount;
    int m_OIDCount;
    bool m_Verbose;
};


#endif // _MULTISOURCE__BUILD_DB_HPP_

