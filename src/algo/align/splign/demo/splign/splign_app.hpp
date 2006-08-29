#ifndef ALGO_ALIGN_DEMO_SPLIGN_APP_HPP
#define ALGO_ALIGN_DEMO_SPLIGN_APP_HPP

/* $Id$
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
* Author:  Yuri Kapustin
*
* File Description:  Splign application class declarations
*                   
* ===========================================================================
*/

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>

#include <algo/align/util/blast_tabular.hpp>
#include <algo/align/splign/splign.hpp>
#include <algo/align/splign/splign_formatter.hpp>

#include <algo/blast/api/blast_options_handle.hpp>
#include <algo/blast/api/sseqloc.hpp>

#include <objtools/readers/seqdb/seqdb.hpp>
#include <objtools/lds/lds.hpp>

#include <objmgr/scope.hpp>

#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqloc/Seq_id.hpp>


BEGIN_NCBI_SCOPE

class CSplignApp: public CNcbiApplication
{
public:

    virtual void Init();
    virtual int  Run();

protected:

    typedef CSplign::THit     THit;
    typedef CSplign::THitRef  THitRef;
    typedef CSplign::THitRefs THitRefs;

    void x_ProcessPair(THitRefs& hitrefs, const CArgs& args);

    blast::EProgram                  m_BlastProgram;
    CRef<blast::CBlastOptionsHandle> m_BlastOptionsHandle;
    CRef<CSplign>                    m_Splign;
    CRef<CSplignFormatter>           m_Formatter;

    CRef<blast::CBlastOptionsHandle> x_SetupBlastOptions(bool cross);

    void x_GetBl2SeqHits(CRef<objects::CSeq_id> seqid_query,
                         CRef<objects::CSeq_id> seqid_subj, 
                         CRef<objects::CScope>  scope,
                         THitRefs* phitrefs);

    void x_GetDbBlastHits(const string& dbname,
                          blast::TSeqLocVector& queries,
                          THitRefs* phitrefs,
                          size_t chunk,
                          size_t total_chunks);

    static THitRef s_ReadBlastHit(const string& m8);

    CNcbiOstream*                    m_AsnOut;
    CNcbiOstream*                    m_AlnOut;

    CNcbiOstream*    m_logstream;
    void x_LogStatus(size_t model_id,
                     bool query_strand,
                     const CSplign::THit::TId& query,
                     const CSplign::THit::TId& subj,
                     bool error,
                     const string& msg);

    bool x_GetNextPair(istream& ifs, THitRefs* hitrefs);
    bool x_GetNextPair(const THitRefs& hitrefs, THitRefs* hitrefs_pair);

    void x_DoIncremental(void);
    void x_DoBatch2(void);

    CRef<objects::CSeq_id> x_ReadFastaSetId(const CArgValue& argval,
                                            CRef<objects::CScope>);

    typedef map<int, CRef<CSeq_id> > TOidToSeqId;
    TOidToSeqId     m_Oid2SeqId;

    string                          m_firstline;
    THitRefs                        m_PendingHits;

    size_t                          m_CurHitRef;

    auto_ptr<objects::CLDS_Database>    m_LDS_db;
};

END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.22  2006/08/29 20:23:03  kapustin
 * Use CLocalBlast instead of discontinued CDbBlast
 *
 * Revision 1.21  2006/05/22 15:52:22  kapustin
 * Engage new FASTA reader in the pairwise mode
 *
 * Revision 1.20  2006/05/08 15:19:02  kapustin
 * Code and file cleanup
 *
 * Revision 1.19  2006/03/21 16:20:50  kapustin
 * Various changes, mainly adjust the code with  other libs
 *
 * Revision 1.18  2006/02/13 19:31:54  kapustin
 * Do not pre-load mRNA
 *
 * Revision 1.17  2005/12/07 15:51:34  kapustin
 * +CSplignApp::s_ReadBlastHit()
 *
 * Revision 1.16  2005/10/31 16:29:58  kapustin
 * Support traditional pairwise alignment text output
 *
 * Revision 1.15  2005/10/24 17:44:06  kapustin
 * Intermediate update
 *
 * Revision 1.14  2005/10/19 17:56:35  kapustin
 * Switch to using ObjMgr+LDS to load sequence data
 *
 * Revision 1.13  2005/09/12 16:24:01  kapustin
 * Move compartmentization to xalgoalignutil.
 *
 * Revision 1.12  2005/08/08 17:43:15  kapustin
 * Bug fix: keep external stream buf as long as the stream
 *
 * Revision 1.11  2005/04/28 19:17:13  kapustin
 * Add cross-species mode flag
 *
 * Revision 1.10  2004/06/21 18:16:45  kapustin
 * Support computation on both query strands
 *
 * Revision 1.9  2004/05/10 16:40:12  kapustin
 * Support a pairwise mode
 *
 * Revision 1.8  2004/05/04 15:23:45  ucko
 * Split splign code out of xalgoalign into new xalgosplign.
 *
 * Revision 1.7  2004/04/23 14:33:32  kapustin
 * *** empty log message ***
 *
 * Revision 1.5  2003/12/23 16:50:25  kapustin
 * Reorder includes to activate msvc pragmas
 *
 * Revision 1.4  2003/12/15 20:16:58  kapustin
 * GetNextQuery() ->GetNextPair()
 *
 * Revision 1.3  2003/11/20 14:38:10  kapustin
 * Add -nopolya flag to suppress Poly(A) detection.
 *
 * Revision 1.2  2003/11/05 20:32:11  kapustin
 * Include source information into the index
 *
 * Revision 1.1  2003/10/30 19:37:20  kapustin
 * Initial toolkit revision
 *
 * ===========================================================================
 */


#endif
