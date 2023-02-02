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
#include <objtools/writers/async_writers.hpp>
#include <objmgr/seq_entry_handle.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/util/sequence.hpp>
#include <objects/seqset/seqset_macros.hpp>
#include <objects/seq/seq_macros.hpp>

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
    void x_ReadAndWritePull(TAppContext& context, const std::list<CConstRef<CSeq_id>>& idlist, CObjectOStream* output) const;
    void x_ReadAndWritePush(TAppContext& context, const std::list<CConstRef<CSeq_id>>& idlist, CObjectOStream* output) const;
    CRef<CScope> x_PopulateScope(TAppContext& context) const;
    void x_ReadTwoScopes(TAppContext& context, const std::list<CConstRef<CSeq_id>>& idlist) const;
    void x_TestRW1848(CBioseq_Handle bh) const;

    int x_ProcessFileTraditionally() const;

    void ProcessHugeSeqEntry(CSeq_entry_Handle seh) const;

    CConstRef<CSerialObject> x_PopulateTopObject() const;

    CConstRef<CSeq_entry> m_top_entry;
};

void CHugeFileDemoApp::Init()
{
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Demonstration of huge file readers",
                              false);
    arg_desc->AddKey("i", "InputFile", "Input ASN.1 file to read", CArgDescriptions::eInputFile);
    arg_desc->AddOptionalKey("o", "OutputFile", "Output modified ASN.1", CArgDescriptions::eOutputFile);
    arg_desc->AddFlag("rw1848", "Testing RW-1848");

    arg_desc->AddFlag("traditional", "Open file in traditional mode");

    SetupArgDescriptions(arg_desc.release());
}

int CHugeFileDemoApp::Run()
{
    string filename = GetArgs()["i"].AsString();
    std::cerr << "Opening " << filename << std::endl;

    if (GetArgs()["traditional"]) {
        return x_ProcessFileTraditionally();
    }

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
        m_top_entry = reader->GetTopEntry();
        if (!m_top_entry && reader->IsMultiSequence()) {
            auto entry = Ref(new CSeq_entry);
            entry->SetSet().SetClass(CBioseq_set::eClass_genbank);
            entry->SetSet().SetSeq_set();
            m_top_entry = entry;
        }

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


CRef<CScope> CHugeFileDemoApp::x_PopulateScope(TAppContext& context) const
{
    auto scope = Ref(new CScope(*context.m_ObjMgr));
    scope->AddDataLoader(context.m_loader_name);
    return scope;
}

void CHugeFileDemoApp::x_TestRW1848(CBioseq_Handle bh) const
{
    std::cerr << "Parent seq id:" << bh.GetAccessSeq_id_Handle().AsString() << "\n";

    auto top_seh = edit::CHugeFileProcess::GetTopLevelEntry(bh);
    auto entry = top_seh.GetCompleteSeq_entry();

    FOR_EACH_SEQANNOT_ON_SEQSET(annot_it, *entry) {
        FOR_EACH_FEATURE_ON_ANNOT(feat, **annot_it) {
            //std::cerr << MSerial_AsnText << *feat;
            auto location_id = (**feat).GetLocation().GetId();
            if (location_id) {
                bool good_id = false;
                for (auto seqid : bh.GetId()) {
                    if (location_id->Compare(*seqid.GetSeqId()) == CSeq_id::e_YES) {
                        good_id = true;
                        break;
                    }
                }
                if (!good_id)
                    std::cerr << "  misplaced feature:" << location_id->AsFastaString() << "\n";
            }
            else
                std::cerr << "  mixed location\n";
        }
    }
}

void CHugeFileDemoApp::x_ReadTwoScopes(TAppContext& context, const std::list<CConstRef<CSeq_id>>& idlist) const
{
    if (idlist.size() < 2)
        throw std::runtime_error("Too little dataset");

    auto scope1 = x_PopulateScope(context);
    auto scope2 = x_PopulateScope(context);

    auto& id1 = **(idlist.begin());
    auto& id2 = **(++idlist.begin());

    x_TestRW1848(scope1->GetBioseqHandle(id1));
    x_TestRW1848(scope2->GetBioseqHandle(id2));
}

void CHugeFileDemoApp::x_RunDemo(TAppContext& context, const std::list<CConstRef<CSeq_id>>& idlist) const
{
    std::cout << "Reading " << idlist.size() << " records" << std::endl;

    if (GetArgs()["rw1848"]) {
        x_ReadTwoScopes(context, idlist);
    } else
    if (GetArgs()["o"]) {
        //x_AddUserObjects(context, idlist, GetArgs()["o"].AsString());
        std::ofstream outfile;
        outfile.exceptions(ios::failbit | ios::badbit);
        outfile.open(GetArgs()["o"].AsString());
        unique_ptr<CObjectOStream> objstr{ CObjectOStream::Open( false ? eSerial_AsnBinary : eSerial_AsnText, outfile) };

        //x_ReadAndWritePull(context, idlist, objstr.get());
        x_ReadAndWritePush(context, idlist, objstr.get());
    } else {
        //x_ShowIds(context, idlist);
        x_ShowSeqSizes(context, idlist);
    }
}

void CHugeFileDemoApp::x_ShowIds(TAppContext& /*context*/, const std::list<CConstRef<CSeq_id>>& idlist) const
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

    auto scope = x_PopulateScope(context);
    size_t i=0;
    for (auto it: idlist)
    {
        CBioseq_Handle bh = scope->GetBioseqHandle(*it);
        if (bh) {
            std::cout
                << i << ":"
                << (*it).AsFastaString()
                << ":" << scope->GetSequenceLength(*it)
                << ":" << scope->GetSequenceType(*it)
                << ":" << scope->GetTaxId(*it)
                << ":" << scope->GetLabel(*it);


            CSeq_entry_Handle seh = bh.GetParentEntry();
            if (seh) {
                auto entry = seh.GetCompleteSeq_entry();
                if (entry) {
                    entry->IsSeq();
                }
            }
            std::cout << "\n";

            i++;

        }


    // copied from CScope
    /// Clean all unused TSEs from the scope's cache and release the memory.
    /// TSEs referenced by any handles are not removed.
    /// Explicitly added entries are not removed.
    /// Edited entries retrieved from a data loader are removed,
    /// so that their unmodified version will be visible.

        scope->ResetHistory();
    }
}

bool CHugeFileDemoApp::x_ModifyBioSeq(CBioseq_EditHandle /*beh*/) const
{
    //auto& descr = beh.SetDescr();
    return true;
}

void CHugeFileDemoApp::x_AddUserObjects(TAppContext& context, const std::list<CConstRef<CSeq_id>>& idlist, const string /*filename*/) const
{
    auto scope = x_PopulateScope(context);

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

CConstRef<CSerialObject> CHugeFileDemoApp::x_PopulateTopObject() const
{
    return m_top_entry;
}

static CSeq_entry_Handle s_GetParentEntry(CSeq_entry_Handle seh)
{
    if (!seh)
        return seh;

    CSeq_entry_Handle parent = seh.GetParentEntry();
    while (parent) {
        CSeq_entry_Handle tmp = parent.GetParentEntry();
        if (!tmp) {
            return parent;
        }
        else {
            parent = tmp;

        }
    }
    return seh;
}

void CHugeFileDemoApp::x_ReadAndWritePush(TAppContext& context, const std::list<CConstRef<CSeq_id>>& idlist, CObjectOStream* output) const
{
    auto scope = x_PopulateScope(context);

    auto top_object = x_PopulateTopObject();

    CGenBankAsyncWriter writer(output);

    writer.StartWriter(top_object);
    //size_t index = 0;
    for (auto id: idlist)
    {
        CBioseq_Handle bsh = scope->GetBioseqHandle(*id);
        if (bsh)
        {
           /*if (50 <= index && index < 100) {
                // modify second fifty records
                CBioseq_EditHandle beh = bh.GetEditHandle();
                x_ModifyBioSeq(beh);
            }
            */

            CSeq_entry_Handle seh;
            auto bssh = bsh.GetParentBioseq_set();
            if (!bssh) { // processing a single bioseq
                seh = bsh.GetParentEntry();
            }
            else {
                seh = s_GetParentEntry(bssh.GetParentEntry());
            }
            
            ProcessHugeSeqEntry(seh);

            if (seh) {
                writer.PushNextEntry(seh.GetCompleteSeq_entry());
            }

        }
        // cleanup leftovers
        scope->ResetHistory();
    }

    writer.FinishWriter();
}

void CHugeFileDemoApp::ProcessHugeSeqEntry(CSeq_entry_Handle seh) const
{
    CScope& scope = seh.GetScope();
    CConstRef<CSeq_feat> orig_feat;
    for (CFeat_CI cds_it(seh, SAnnotSelector(CSeqFeatData::eSubtype_cdregion)); cds_it; ++cds_it) {
        orig_feat.Reset(&(*cds_it->GetSeq_feat()));

        CSeq_feat_Handle fh = scope.GetSeq_featHandle(*(orig_feat.GetPointerOrNull()));
        if (fh) {
            cout << "Orig seqfeat handle was found" << endl;
        }

        CConstRef<CSeq_feat> old_gene = sequence::GetOverlappingGene(orig_feat->GetLocation(), scope);
        if (old_gene) {
            CRef<CSeq_feat> new_gene(new CSeq_feat);
            new_gene->Assign(*old_gene);
            new_gene->SetComment("NEW GENE COMMENT");

            CSeq_feat_Handle old_geneh = scope.GetSeq_featHandle(*old_gene);
            const CSeq_annot_Handle& annot_handle = old_geneh.GetAnnot();  
            // if this line is missing, it throws an exception at the next one:
            // \src\objmgr\seq_annot_handle.cpp(273) : CObjMgrException::eInvalidHandle : "object is not in editing mode"
            CSeq_entry_EditHandle eh = annot_handle.GetParentEntry().GetEditHandle();

            // Now actually edit the feature
            CSeq_feat_EditHandle feh(old_geneh);
            CSeq_entry_Handle parent_entry = feh.GetAnnot().GetParentEntry();

            feh.Replace(*new_gene);
            cout << "Replaced the old gene with the new one " << endl;
        }

        {
            auto ffh = cds_it->GetSeq_feat_Handle();
            if (ffh) {
                cout << "Works if handle is obtained from the iterator!" << endl;
            }
        }
        {
            // this will only work if annot_handle from line 438 is not in editing mode
            CSeq_feat_Handle new_fh = scope.GetSeq_featHandle(*(orig_feat.GetPointerOrNull()));
            if (new_fh) {
                cout << "New fh was found!" << endl;
            }
        }
    }
}

void CHugeFileDemoApp::x_ReadAndWritePull(TAppContext& context, const std::list<CConstRef<CSeq_id>>& idlist, CObjectOStream* output) const
{
    auto scope = x_PopulateScope(context);

    auto top_object = x_PopulateTopObject();

    auto it = idlist.begin();
    size_t index = 0;

    struct processing_token
    {
        CBioseq_Handle m_bh;
        CSeq_entry_Handle m_seh;
        size_t m_index = 0;
        operator CConstRef<CSeq_entry>() const
        {
            if (m_seh)
                return m_seh.GetCompleteSeq_entry();
            return {};
        }
    };

    auto pull_next = [this, &scope, &index, &it, &idlist] (processing_token& token) -> bool
    {
        // clean up leftovers
        scope->ResetHistory();

        if (it == idlist.end())
            return false; // no more data

        // just to show how to pass some data along m_seh
        token.m_index = index;
        // load entry
        if (*it)
            token.m_bh = scope->GetBioseqHandle(**it);

        if (!token.m_seh)
            return false;

        index++;
        it++;
        return true;
    };

    auto process_func = [this](processing_token& token)
    {
        if (token.m_bh) {
            if (50 <= token.m_index && token.m_index < 100) {
                // modify second fifty records
                CBioseq_EditHandle beh = token.m_bh.GetEditHandle();
                x_ModifyBioSeq(beh);
            }
        }

        // let the writer the whole nuc-prot-set
        if (token.m_bh) {
            token.m_seh = token.m_bh.GetParentEntry();
        }

    };

    CGenBankAsyncWriterEx<processing_token> writer(output);

    writer.WriteAsyncST(top_object, pull_next, process_func);
    //writer.WriteAsync2T(top_object, pull_next, process_func);
    //writer.WriteAsyncMT(top_object, pull_next, process_func);
}


int CHugeFileDemoApp::x_ProcessFileTraditionally() const
{
    CRef<CSeq_entry> entry(new CSeq_entry);

    try {
        CNcbiIfstream istr(GetArgs()["i"].AsString().c_str());
        unique_ptr<CObjectIStream> os(CObjectIStream::Open(eSerial_AsnText, istr));
        *os >> *entry;
    }
    catch (const CException& e) {
        LOG_POST(Error << e.ReportAll());
        return 1;
    }

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));
    scope->AddDefaults();

    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);

    CConstRef<CSeq_feat> orig_feat;
    for (CFeat_CI cds_it(seh, SAnnotSelector(CSeqFeatData::eSubtype_cdregion)); cds_it; ++cds_it) {
        orig_feat.Reset(&(*cds_it->GetSeq_feat()));

        CSeq_feat_Handle fh = scope->GetSeq_featHandle(*(orig_feat.GetPointerOrNull()));
        if (fh) {
            cout << "Orig seqfeat handle was found" << endl;
        }

        CConstRef<CSeq_feat> old_gene = sequence::GetOverlappingGene(orig_feat->GetLocation(), *scope);
        if (old_gene) {
            CRef<CSeq_feat> new_gene(new CSeq_feat);
            new_gene->Assign(*old_gene);
            new_gene->SetComment("NEW GENE COMMENT");

            CSeq_feat_Handle old_geneh = scope->GetSeq_featHandle(*old_gene);
            const CSeq_annot_Handle& annot_handle = old_geneh.GetAnnot();
            CSeq_entry_EditHandle eh = annot_handle.GetParentEntry().GetEditHandle();

            // Now actually edit the feature
            CSeq_feat_EditHandle feh(old_geneh);
            CSeq_entry_Handle parent_entry = feh.GetAnnot().GetParentEntry();

            feh.Replace(*new_gene);
            cout << "Replaced the old gene with the new one! " << endl;

        }

        {
            auto ffh = cds_it->GetSeq_feat_Handle();
            if (ffh) {
                cout << "It works if handle is obtained from the iterator!" << endl;
            }
        }
        {
            CSeq_feat_Handle new_fh = scope->GetSeq_featHandle(*(orig_feat.GetPointerOrNull()));
            if (new_fh) {
                cout << "New fh was found!" << endl;
            }
        }
    }
    return 0;
}

int main(int argc, const char* argv[])
{
    // Execute main application function
    return CHugeFileDemoApp().AppMain(argc, argv); //, 0, eDS_Default, 0);
}

// TODO: need to expose Annotations
// TODO: we expose the callback for renaming conflicting seqids
// TODO: follow https://jira.ncbi.nlm.nih.gov/browse/CXX-12650
