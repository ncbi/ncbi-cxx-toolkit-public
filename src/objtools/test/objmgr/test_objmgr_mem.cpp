#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbitime.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seq_descr_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/align_ci.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/NCBI2na.hpp>

#include <serial/objectio.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objcopy.hpp>
#include <serial/objectinfo.hpp>
#include <serial/iterator.hpp>
#include <serial/objectiter.hpp>
#include <serial/serial.hpp>

#include <memory>
#include <fstream>

#include <common/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;
using namespace objects;

class CMemTestApp : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run (void);
};


void CMemTestApp::Init(void)
{
    // Prepare command line descriptions
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->AddOptionalKey("gi", "SeqEntryID",
                             "GI id of the Seq-Entry to fetch",
                             CArgDescriptions::eInteger);
    arg_desc->AddOptionalKey("count", "Count", "Repeat count",
                             CArgDescriptions::eInteger);
    arg_desc->AddOptionalKey("file", "File", "File with Seq-entry",
                             CArgDescriptions::eInputFile);
    arg_desc->AddFlag("objmgr", "Add entries from file to object manager");
    arg_desc->AddFlag("iterate", "Run CTypeConstIterator<CSeq_feat>");

    string prog_description = "memtest";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              prog_description, false);

    SetupArgDescriptions(arg_desc.release());
}


int CMemTestApp::Run(void)
{
    const CArgs& args = GetArgs();
    TGi gi = GI_FROM(int, args["gi"]? args["gi"].AsInteger(): -1);
    string file = args["file"]? args["file"].AsString(): string();
    int repeat_count = args["count"]?args["count"].AsInteger():100;
    bool add_to_objmgr = args["objmgr"];
    bool run_iter = args["iterate"];

    vector< CRef<CSeq_entry> > entries;
    if ( file.size() ) {
        ifstream ifs(file.c_str());
        auto_ptr<CObjectIStream> is(CObjectIStream::Open(eSerial_AsnText,
                                                         ifs));
        const CClassTypeInfo *seqSetInfo =
            (const CClassTypeInfo*)CBioseq_set::GetTypeInfo();
        is->SkipFileHeader(seqSetInfo);
        is->BeginClass(seqSetInfo);
        while (TMemberIndex i = is->BeginClassMember(seqSetInfo)) {
            const CMemberInfo &mi = *seqSetInfo->GetMemberInfo(i);
            const string &miId = mi.GetId().GetName();
            if (miId.compare("seq-set") == 0) {
                int count = 0;
                for (CIStreamContainerIterator m(*is, mi.GetTypeInfo());
                     m; ++m) {
                    CRef<CSeq_entry> entry(new CSeq_entry);
                    {
                        NcbiCout << "Reading Seq-entry: " << &*entry << NcbiEndl;
                        m >> *entry;
                    }
                    if ( ++count <= repeat_count ) {
                        if ( add_to_objmgr ) {
                            CRef<CObjectManager> objMgr = CObjectManager::GetInstance();
                            CRef<CScope> scope(new CScope(*objMgr));
                            scope->AddTopLevelSeqEntry(*entry);
                            if ( run_iter ) {
                                for ( CTypeConstIterator<CSeq_feat> it=ConstBegin(*entry);
                                      it; ++it ) {
                                }
                            }
                        }
                        else {
                            if ( run_iter ) {
                                for ( CTypeConstIterator<CSeq_feat> it=ConstBegin(*entry);
                                      it; ++it ) {
                                }
                            }
                        }
                    }

                    if ( entry->ReferencedOnlyOnce() ) {
                        NcbiCout << "Unreferenced: " << &*entry << NcbiEndl;
                    }
                    else {
                        NcbiCout << "Still referenced: " << &*entry << NcbiEndl;
                        entries.push_back(entry);
                    }
                }
            }
            else {
                is->SkipObject(mi.GetTypeInfo());
            }
        }
    }
    else if ( gi > ZERO_GI ) {
        for ( int count = 0; count < repeat_count; ++count ) {
            typedef map<const CObject*, int> TCounterMap;
            TCounterMap cnt;
            {
                CRef<CObjectManager> objMgr = CObjectManager::GetInstance();
                CGBDataLoader::RegisterInObjectManager(*objMgr);
                CScope scope(*objMgr);
                scope.AddDefaults();
                CSeq_id id;
                id.SetGi(gi);
                CBioseq_Handle bh = scope.GetBioseqHandle(id);
                const CSeq_entry& entry =
                    *bh.GetTopLevelEntry().GetCompleteSeq_entry();
                {
                    const CObject* obj = &entry;
                    int c = reinterpret_cast<const int*>(obj)[1];
                    cnt[obj] = c;
                    NcbiCout << "Entry at " << obj << " have counter " << c << NcbiEndl;
                }
                {
                    const CObject* obj = &entry;
                    int c = reinterpret_cast<const int*>(obj)[1];
                    NcbiCout << "Entry at " << obj << " last counter " << c << NcbiEndl;
                }
            }
            ITERATE ( TCounterMap, it, cnt ) {
                const CObject* obj = it->first;
                int c = reinterpret_cast<const int*>(obj)[1];
                NcbiCout << "Object at " << obj << " last counter " << c << NcbiEndl;
            }
        }
    }
    NON_CONST_ITERATE ( vector< CRef<CSeq_entry> >, i, entries ) {
        CRef<CSeq_entry> entry = *i;
        i->Reset();
        if ( entry->ReferencedOnlyOnce() ) {
            NcbiCout << "Unreferenced: " << &*entry << NcbiEndl;
        }
        else {
            NcbiCout << "Still referenced: " << &*entry << NcbiEndl;
        }
    }
    return 0;
}

int main(int argc, const char* argv[])
{
    return CMemTestApp().AppMain(argc, argv, 0, eDS_Default, 0);
}

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
* Authors:  Eugene Vasilchenko
*
* File Description:
*   Test memory leaks in C++ object manager
*/
