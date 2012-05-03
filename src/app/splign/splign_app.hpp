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

#include <objtools/blast/seqdb_reader/seqdb.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>

#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqloc/Seq_id.hpp>


BEGIN_NCBI_SCOPE

class CSplignApp: public CNcbiApplication
{
public:

    CSplignApp(void);

    virtual void Init();
    virtual int  Run();

protected:

    typedef CSplign::THit     THit;
    typedef CSplign::THitRef  THitRef;
    typedef CSplign::THitRefs THitRefs;


    void x_RunSplign(bool raw_hits, THitRefs* phitrefs, 
                     THit::TCoord smin, THit::TCoord smax,
                     CSplign::TResults * psplign_results);

    void x_ProcessPair(THitRefs& hitrefs, const CArgs& args,
                       THit::TCoord smin = 0,
                       THit::TCoord smax = 0);

    blast::EProgram                  m_BlastProgram;
    CRef<blast::CBlastOptionsHandle> m_BlastOptionsHandle;
    CRef<CSplign>                    m_Splign;
    CRef<CSplignFormatter>           m_Formatter;

    CRef<blast::CBlastOptionsHandle> x_SetupBlastOptions(bool cross);

    void x_GetBl2SeqHits(CRef<objects::CSeq_id> seqid_query,
                         CRef<objects::CSeq_id> seqid_subj, 
                         CRef<objects::CScope>  scope,
                         THitRefs* phitrefs);

    static THitRef s_ReadBlastHit(const string& m8);

    CNcbiOstream*                    m_AsnOut;
    CNcbiOstream*                    m_AlnOut;

    CNcbiOstream*    m_logstream;
    void x_LogCompartmentStatus(const THit::TId & query, 
                                const THit::TId & subj, 
                                const CSplign::SAlignedCompartment & ac);

    bool x_GetNextPair(istream& ifs, THitRefs* hitrefs);
    bool x_GetNextPair(const THitRefs& hitrefs, THitRefs* hitrefs_pair);

    bool x_GetNextComp(istream& ifs, THitRefs* hitrefs,
                       THit::TCoord* psubj_min,
                       THit::TCoord* psubj_max);


    CRef<objects::CSeq_id> x_ReadFastaSetId(const CArgValue& argval,
                                            CRef<objects::CScope>);

    typedef map<int, CRef<CSeq_id> > TOidToSeqId;
    TOidToSeqId                         m_Oid2SeqId;

    string                              m_AppName;

    string                              m_firstline;
    THitRefs                            m_PendingHits;

    size_t                              m_CurHitRef;

    CRef<objects::CObjectManager>       m_ObjMgr;
};

END_NCBI_SCOPE

#endif
