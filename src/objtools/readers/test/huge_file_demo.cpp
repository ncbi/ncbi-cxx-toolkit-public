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
#include <objmgr/util/sequence.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

struct TAppContext
{
    CRef<CObjectManager> m_ObjMgr = CObjectManager::GetInstance();
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
    void x_AddUserObjects(TAppContext& context, const std::list<CConstRef<CSeq_id>>& idlist, const string filename) const;
    bool x_ModifyBioSeq(CBioseq_EditHandle beh) const;
};

void CHugeFileDemoApp::Init()
{
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Demonstration of huge file readers",
                              false);
    arg_desc->AddKey("i", "InputFile", "Input ASN.1 file to read", CArgDescriptions::eInputFile);
    arg_desc->AddOptionalKey("o", "OutputFile", "Output modified ASN.1", CArgDescriptions::eOutputFile);

    SetupArgDescriptions(arg_desc.release());
}

int CHugeFileDemoApp::Run()
{
    string filename = GetArgs()["i"].AsString();
    std::cerr << "Opening " << filename << std::endl;

    edit::CHugeFileProcess huge;
    huge.Open(filename);

    // context doesn't have huge' classes anymore, existing implementations should use traditional OM classes and approaches
    // huge classes are used only for opening files and non-standard algorithms
    TAppContext context;

    // object manager need unique id's for loader name
    // so each huge file opened needs it's unique id
    context.m_loader_name = CDirEntry::ConvertToOSPath(filename);

    bool result = huge.Read([this, &context](edit::CHugeAsnReader* reader, const std::list<CConstRef<CSeq_id>>& idlist)->bool
    {
        // one can register as many huge loaders as needed but don't mess with loader's priorities up
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

    if (GetArgs()["o"]) {
        x_AddUserObjects(context, idlist, GetArgs()["o"].AsString());
    } else {
        x_ShowIds(context, idlist);
        x_ShowSeqSizes(context, idlist);
    }
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
    // Note, not CHugeAsnReader is used

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

    // copied from CScope
    /// Clean all unused TSEs from the scope's cache and release the memory.
    /// TSEs referenced by any handles are not removed.
    /// Explicitly added entries are not removed.
    /// Edited entries retrieved from a data loader are removed,
    /// so that their unmodified version will be visible.

        scope->ResetHistory();
    }
}

bool CHugeFileDemoApp::x_ModifyBioSeq(CBioseq_EditHandle beh) const
{
    auto& descr = beh.SetDescr();
    return true;
}

void CHugeFileDemoApp::x_AddUserObjects(TAppContext& context, const std::list<CConstRef<CSeq_id>>& idlist, const string filename) const
{
    auto scope = Ref(new CScope(*context.m_ObjMgr));
    scope->AddDataLoader(context.m_loader_name);

    // these are to just keep handles, this helps Object Manager to understand they're still needed
    std::list<CBioseq_Handle> modified_records;
    size_t i=0;
    for (auto id: idlist)
    {
        // this method should force object manager to load sequence from back end
        CBioseq_Handle bh = scope->GetBioseqHandle(*id);
        if (bh) {

            if (50 <= i && i < 100) {
                // modify second fifty records
                CBioseq_EditHandle beh = bh.GetEditHandle();
                if (x_ModifyBioSeq(beh))
                    modified_records.push_back(beh);
            }

            // report first 100 records
            if (i < 100) {
            std::cout
                << "  "
                << bh.GetSeqId()->AsFastaString()
                << ":" << bh.GetBioseqLength()
                << ":" << bh.GetSequenceType()
                << ":" << bh.GetBioseqMolType()
                << std::endl;
            }

            // other records are just iterated over and loaded
        }

    // copied from CScope
    /// Clean all unused TSEs from the scope's cache and release the memory.
    /// TSEs referenced by any handles are not removed.
    /// Explicitly added entries are not removed.
    /// Edited entries retrieved from a data loader are removed,
    /// so that their unmodified version will be visible.

        bh.Reset();
        // so only records that are references in 'modified_records' list
        // should be kept in object manager's memory
        //
        scope->ResetHistory();
        i++;
        if (i>200) {
            // just quickly stop looping re-reading unneeded sequences
            break;
        }
    }

    i=0;
    // iterate over the list again and report which are modified
    for (auto id: idlist)
    {
        CBioseq_Handle bh = scope->GetBioseqHandle(*id, CScope::eGetBioseq_Loaded);
        if (bh) {
            std::cout << "Sequence #" << i << ":" << bh.GetSeqId()->AsFastaString() << " was edited\n";
        }
        ++i;
    }
}

int main(int argc, const char* argv[])
{
    // Execute main application function
    return CHugeFileDemoApp().AppMain(argc, argv); //, 0, eDS_Default, 0);
}

// TODO: open, modify some of random records, read some other records and save
// TODO: need to expose Annotations
// TODO: we expose the callback for renaming conflicting seqids
// TODO: follow https://jira.ncbi.nlm.nih.gov/browse/CXX-12650
//
//
