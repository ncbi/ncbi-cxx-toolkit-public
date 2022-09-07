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
* Author:  Sergiy Gotvyanskyy
*
* File Description:
*   Demo application for reading huge ASN.1 files
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbistl.hpp>

#include <objtools/edit/huge_file.hpp>
#include <objtools/edit/huge_asn_reader.hpp>
#include <objtools/edit/huge_file_process.hpp>
#include <objtools/edit/huge_asn_loader.hpp>
#include <objmgr/scope.hpp>
//include <objmgr/util/sequence.hpp>
//include <objtools/readers/fasta.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

struct TAppContext
{
    CRef<CObjectManager> m_ObjMgr = CObjectManager::GetInstance();
    edit::CHugeFileProcess m_huge;
    string m_loader_name;
};

class CHugeFileDemoApp : public CNcbiApplication
{
protected:
    void Init();
    int  Run();
    void x_RunDemo(TAppContext& context, const std::list<CConstRef<CSeq_id>>& idlist) const;
    void x_ShowIds(TAppContext& context, const std::list<CConstRef<CSeq_id>>& idlist) const;
    void x_ShowSeqSizes(TAppContext& context, const std::list<CConstRef<CSeq_id>>& idlist) const;

};

void CHugeFileDemoApp::Init()
{
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Demonstration of huge file readers",
                              false);
    arg_desc->AddKey("i", "InputFile", "Input ASN.1 file to read", CArgDescriptions::eInputFile);

    SetupArgDescriptions(arg_desc.release());
}

int CHugeFileDemoApp::Run()
{
    string filename = GetArgs()["i"].AsString();
    std::cerr << "Opening " << filename << std::endl;

    TAppContext context;
    context.m_huge.Open(filename);

    context.m_loader_name = CDirEntry::ConvertToOSPath(filename);

    bool result = context.m_huge.Read([this, &context](edit::CHugeAsnReader* reader, const std::list<CConstRef<CSeq_id>>& idlist)->bool
    {
        auto info = edit::CHugeAsnDataLoader::RegisterInObjectManager(
            *context.m_ObjMgr, context.m_loader_name, reader, CObjectManager::eDefault, 1); //CObjectManager::kPriority_Local);

        x_RunDemo(context, idlist);

        CObjectManager::GetInstance()->RevokeDataLoader(*info.GetLoader());

        return false;
    });

    if (result)
        return 1;

    return 0;
}

void CHugeFileDemoApp::x_RunDemo(TAppContext& context, const std::list<CConstRef<CSeq_id>>& idlist) const
{
    std::cout << "Reading " << idlist.size() << " records" << std::endl;

    x_ShowIds(context, idlist);
    x_ShowSeqSizes(context, idlist);
}

void CHugeFileDemoApp::x_ShowIds(TAppContext& context, const std::list<CConstRef<CSeq_id>>& idlist) const
{
    auto id_it = idlist.begin();
    for (size_t i=0; i<100 && i<idlist.size(); ++i)
    {
        std::cout << "  " << (**id_it).AsFastaString() << std::endl;
        id_it++;
    }
}

void CHugeFileDemoApp::x_ShowSeqSizes(TAppContext& context, const std::list<CConstRef<CSeq_id>>& idlist) const
{
    // this method demostrates shortcuts for fetching vital sequence data without reading the sequence into memory
    // this must be very fast and minimal memory consumption

    auto scope = Ref(new CScope(*context.m_ObjMgr));
    scope->AddDataLoader(context.m_loader_name);
    auto id_it = idlist.begin();
    for (size_t i=0; i<100 && i<idlist.size(); ++i)
    {
        std::cout
            << "  "
            << (**id_it).AsFastaString()
            << ":" << scope->GetSequenceLength(**id_it)
            << ":" << scope->GetSequenceType(**id_it)
            << ":" << scope->GetTaxId(**id_it)
            << ":" << scope->GetLabel(**id_it)
            << std::endl;

        id_it++;
    }

}

int main(int argc, const char* argv[])
{
    // Execute main application function
    return CHugeFileDemoApp().AppMain(argc, argv); //, 0, eDS_Default, 0);
}
