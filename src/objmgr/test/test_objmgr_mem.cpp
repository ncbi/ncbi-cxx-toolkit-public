#include <objects/objmgr/object_manager.hpp>
#include <objects/objmgr/scope.hpp>
#include <objects/objmgr/bioseq_handle.hpp>
#include <objects/objmgr/seq_vector.hpp>
#include <objects/objmgr/desc_ci.hpp>
#include <objects/objmgr/feat_ci.hpp>
#include <objects/objmgr/align_ci.hpp>
#include <objects/objmgr/gbloader.hpp>
#include <objects/objmgr/reader_id1.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Bioseq.hpp>

#include <serial/object.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objcopy.hpp>
#include <serial/objectinfo.hpp>
#include <serial/iterator.hpp>
#include <serial/objectiter.hpp>
#include <serial/serial.hpp>

#include <memory>
#include <unistd.h>

using namespace ncbi;
using namespace ncbi::objects;

int main(int argc, char **argv)
{
    try {
        vector< CRef<CSeq_entry> > entries;
        for ( int arg = 1; arg < argc; ++arg ) {
            ifstream ifs(argv[arg]);
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
                    for (CIStreamContainerIterator it(*is, mi.GetTypeInfo());
                         it; ++it) {
                        CRef<CSeq_entry> entry(new CSeq_entry);
                        {
                            cout << "Reading Seq-entry: " << &*entry << endl;
                            it >> *entry;
                            CRef<CObjectManager> objMgr(new CObjectManager);
                            CRef<CScope> scope(new CScope(*objMgr));
#if 1
                            scope->AddTopLevelSeqEntry(*entry);
#endif
                        }
                        if ( entry->ReferencedOnlyOnce() ) {
                            cout << "Unreferenced: " << &*entry << endl;
                        }
                        else {
                            cout << "Still referenced: " << &*entry << endl;
                            entries.push_back(entry);
                        }
                    }
                }
                else {
                    is->SkipObject(mi.GetTypeInfo());
                }
            }
        }
        NON_CONST_ITERATE ( vector< CRef<CSeq_entry> >, i, entries ) {
            CRef<CSeq_entry> entry = *i;
            i->Reset();
            if ( entry->ReferencedOnlyOnce() ) {
                cout << "Unreferenced: " << &*entry << endl;
            }
            else {
                cout << "Still referenced: " << &*entry << endl;
            }
        }
    }
    catch (const exception &e) {
        cerr << e.what() << endl;
        return 1;
    }
    catch (...) {
        cerr << "Unknown exception" << endl;
        return 2;
    }
    return 0;
}
