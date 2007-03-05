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
* Author:  Maxim Didenko
*
* File Description:
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_id_translator.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/tse_assigner.hpp>

#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/data_loaders/patcher/loaderpatcher.hpp>


// (BEGIN_NCBI_SCOPE must be followed by END_NCBI_SCOPE later in this file)
BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

class CDataPatcher : public IDataPatcher
{
public:
    CDataPatcher(const CSeq_id_Handle& orig_gi, 
                 const CSeq_id_Handle& patched_gi);
    ~CDataPatcher() {}
    
    virtual CRef<ITSE_Assigner> GetAssigner();
    virtual EPatchLevel IsPatchNeeded(const CTSE_Info& tse) { return ePartTSE ; }

private:

    CRef<ITSE_Assigner> m_Assigner;
    CSeq_id_Handle m_OrigId;
    CSeq_id_Handle m_PatchedId;
};

CDataPatcher::CDataPatcher(const CSeq_id_Handle& orig_gi, 
                           const CSeq_id_Handle& patched_gi)
    : m_OrigId( orig_gi ),
      m_PatchedId( patched_gi )
{
}

CRef<ITSE_Assigner> CDataPatcher::GetAssigner()
{
    if (!m_Assigner) {
        m_Assigner.Reset(new CTSE_Default_Assigner);
    }
    return m_Assigner;
}




class CLoaderPatcherTester : public CNcbiApplication
{
    virtual void Init(void);
    virtual int  Run(void);
};


void CLoaderPatcherTester::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "DataLoaderPather test", false);

    arg_desc->AddKey("ogi", "OrigGi", "GI id of the original sequence",
                     CArgDescriptions::eInteger);
    arg_desc->AddKey("pgi", "PatchedGi", "GI id of the patched sequence",
                     CArgDescriptions::eInteger);

    SetupArgDescriptions(arg_desc.release());
}


int CLoaderPatcherTester::Run(void)
{

    const CArgs&   args = GetArgs();

    CSeq_id_Handle orig_id = CSeq_id_Handle::GetGiHandle(args["ogi"].AsInteger());
    CSeq_id_Handle patched_id = CSeq_id_Handle::GetGiHandle(args["pgi"].AsInteger());

    CRef<IDataPatcher> patcher(new CDataPatcher(orig_id, 
                                                patched_id));

    CRef<CObjectManager> object_manager = CObjectManager::GetInstance();
    CRef<CDataLoader> loader(
                 CGBDataLoader::RegisterInObjectManager(*object_manager,
                                                        0,
                                                        CObjectManager::eDefault)
                 .GetLoader());

    CConstRef<CBioseq> bioseq1;
    {
        CScope scope(*object_manager);
        scope.AddDefaults();
        CBioseq_Handle bh = scope.GetBioseqHandle(*patched_id.GetSeqId());
        bioseq1 = bh.GetCompleteBioseq();
    }



    
    CDataLoaderPatcher::RegisterInObjectManager(*object_manager,
                                                loader,
                                                patcher,
                                                CRef<IEditSaver>(),
                                                CObjectManager::eDefault,
                                                88);
    CConstRef<CBioseq> bioseq2;
    {
        CScope scope(*object_manager);
        scope.AddDefaults();
        CBioseq_Handle bh = scope.GetBioseqHandle(*orig_id.GetSeqId());
        bioseq2 = bh.GetCompleteBioseq();
    }

    //    CRef<CBioseq> bioseq3 = PatchSeqId_Copy(*bioseq1, *patcher->GetSeqIdTranslator());

    if (bioseq3->Equals(*bioseq2) ){
        NcbiCout << "Passed" << NcbiEndl;
        return 0;
    } else {
        NcbiCout << "Failed" << NcbiEndl;
        return 1;
    }

      
    return 0;
}


// (END_NCBI_SCOPE must be preceded by BEGIN_NCBI_SCOPE)
END_NCBI_SCOPE

USING_NCBI_SCOPE;

int main(int argc, const char** argv)
{
    return CLoaderPatcherTester().AppMain(argc, argv);
}
