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
* Author:  Aleksey Grichenko
*
* File Description:
*   Examples of split data loader implementation
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <connect/ncbi_core_cxx.hpp>

// Object manager includes
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/graph_ci.hpp>
#include <objmgr/align_ci.hpp>

#include <objmgr/object_manager.hpp>
#include "split_loader.hpp"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


/////////////////////////////////////////////////////////////////////////////
//
//  Demo application
//


class CDemoApp : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run (void);
};


void CDemoApp::Init(void)
{
    CONNECT_Init(&GetConfig());

    // Prepare command line descriptions
    //

    // Create
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // GI to fetch
    arg_desc->AddKey("id", "SeqEntryID",
                     "Seq-id of the Seq-Entry to fetch",
                     CArgDescriptions::eString);
    arg_desc->AddKey("file", "SeqEntryFile",
                     "file with Seq-entry to load (text ASN.1)",
                     CArgDescriptions::eString);

    // Program description
    string prog_description = "Example of split data loader implementation\n";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              prog_description, false);

    // Pass argument descriptions to the application
    //

    SetupArgDescriptions(arg_desc.release());
}


int CDemoApp::Run(void)
{
    // Process command line args: get GI to load
    const CArgs& args = GetArgs();

    // Create seq-id, set it to GI specified on the command line
    CRef<CSeq_id> id(new CSeq_id(args["id"].AsString()));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*id);

    string data_file = args["file"].AsString();

    NcbiCout << "Loading " << idh.AsString()
        << " from " << data_file << NcbiEndl;

    // Create object manager. Use CRef<> to delete the OM on exit.
    CRef<CObjectManager> pOm = CObjectManager::GetInstance();
    // Create demo data loader and register it with the OM.
    CSplitDataLoader::RegisterInObjectManager(*pOm, data_file);
    // Create a new scope.
    CScope scope(*pOm);
    // Add default loaders (GB loader in this demo) to the scope.
    scope.AddDefaults();

    CBioseq_Handle handle = scope.GetBioseqHandle(idh);
    // Check if the handle is valid
    if ( !handle ) {
        ERR_POST(Fatal << "Bioseq not found");
    }

    size_t count = 0;
    for (CFeat_CI fit(handle); fit; ++fit) {
        ++count;
    }
    NcbiCout << "Found " << count << " features" << NcbiEndl;

    count = 0;
    for (CSeqdesc_CI dit(handle); dit; ++dit) {
        ++count;
    }
    NcbiCout << "Found " << count << " descriptors" << NcbiEndl;

    CSeqVector vect(handle);
    string data;
    vect.GetSeqData(vect.begin(), vect.end(), data);
    NcbiCout << "Seq-data: " << NStr::PrintableString(data.substr(0, 10))
             << NcbiEndl;

    NcbiCout << "Done" << NcbiEndl;
    return 0;
}


END_NCBI_SCOPE



/////////////////////////////////////////////////////////////////////////////
//  MAIN


USING_NCBI_SCOPE;

int main(int argc, const char* argv[])
{
    int ret = CDemoApp().AppMain(argc, argv);
    NcbiCout << NcbiEndl;
    return ret;
}

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  2005/05/23 15:06:48  grichenk
* Removed commented includes
*
* Revision 1.1  2005/05/23 15:04:46  grichenk
* Initial revision
*
* Revision 1.101  2005/03/14 18:12:44  vasilche
* Allow loading Seq-annot from file.
*
* Revision 1.100  2005/03/10 20:57:28  vasilche
* New way of CGBLoader instantiation.
*
* Revision 1.99  2005/02/24 17:13:20  vasilche
* Added flag -print_mapper to check CSeq_loc_Mapper.
* Added scan of all features in TSE when -whole_tse is specified.
* Avoid failure when cdregions don't have product.
*
* Revision 1.98  2005/02/01 21:55:11  grichenk
* Added direction flag for mapping between top level sequence
* and segments.
*
* Revision 1.97  2005/01/27 20:06:17  grichenk
* Fixed unused variable warning
*
* Revision 1.96  2005/01/05 18:58:41  vasilche
* Added -noexternal option.
*
* Revision 1.95  2004/12/23 17:01:45  grichenk
* Use BDB_CACHE_LIB and HAVE_BDB_CACHE
*
* Revision 1.94  2004/12/22 15:56:45  vasilche
* Added test for auto-release of TSEs by scope
*
* Revision 1.93  2004/12/21 18:57:35  vasilche
* Display second location on cdregion features.
*
* Revision 1.92  2004/12/06 17:54:09  grichenk
* Replaced calls to deprecated methods
*
* Revision 1.91  2004/11/16 21:41:11  grichenk
* Removed default value for CScope* argument in CSeqMap methods
*
* Revision 1.90  2004/11/01 19:33:08  grichenk
* Removed deprecated methods
*
* Revision 1.89  2004/10/26 20:03:33  vasilche
* Added output of CSeq_loc_Mapper result.
*
* Revision 1.88  2004/10/21 15:48:40  vasilche
* Added options -range_loc and -overlap.
*
* Revision 1.87  2004/10/14 17:44:52  vasilche
* Added bidirectional tests for CSeqMap_CI.
*
* Revision 1.86  2004/10/07 14:09:48  vasilche
* More options to control demo application.
*
* Revision 1.85  2004/10/05 21:05:35  vasilche
* Added option -by_product.
*
* Revision 1.84  2004/09/27 14:37:24  grichenk
* More args (missing and external)
*
* Revision 1.83  2004/09/08 16:43:31  vasilche
* More tuning of BDB cache.
*
* Revision 1.82  2004/08/31 14:15:46  vasilche
* Added options -count_types and -reset_scope
*
* Revision 1.81  2004/08/24 16:43:53  vasilche
* Removed TAB symbols from sources.
* Seq-id cache is put in the same directory as blob cache.
* Removed dead code.
*
* Revision 1.80  2004/08/19 17:02:35  vasilche
* Use CBlob_id instead of obsolete CSeqref.
* Use requested feature subtype for all feature iterations.
* Added -limit_tse test option.
*
* Revision 1.79  2004/08/17 15:41:20  vasilche
* Added -used_memory_check option.
*
* Revision 1.78  2004/08/11 17:58:17  vasilche
* Added -print_cds option.
*
* Revision 1.77  2004/08/09 15:55:57  vasilche
* Fine tuning of blob cache.
*
* Revision 1.76  2004/07/26 14:13:31  grichenk
* RegisterInObjectManager() return structure instead of pointer.
* Added CObjectManager methods to manipuilate loaders.
*
* Revision 1.75  2004/07/21 15:51:24  grichenk
* CObjectManager made singleton, GetInstance() added.
* CXXXXDataLoader constructors made private, added
* static RegisterInObjectManager() and GetLoaderNameFromArgs()
* methods.
*
* Revision 1.74  2004/07/15 16:50:51  vasilche
* All annotation iterators use -range_from and -range_to options.
*
* Revision 1.73  2004/07/13 21:09:09  vasilche
* Added "range_from" and "range_to" options.
*
* Revision 1.72  2004/07/13 14:04:28  vasilche
* Optional compilation with Berkley DB.
*
* Revision 1.71  2004/07/12 19:21:14  ucko
* Don't assume that size_t == unsigned.
*
* Revision 1.70  2004/07/12 17:08:01  vasilche
* Allow limited processing to have benefits from split blobs.
*
* Revision 1.69  2004/06/30 22:15:20  vasilche
* Removed obsolete code for old blob cache interface.
*
* Revision 1.68  2004/05/21 21:41:40  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.67  2004/05/13 19:50:48  vasilche
* Added options to skip and get mapped alignments.
*
* Revision 1.66  2004/05/13 19:33:15  vasilche
* Added option for printing descriptors.
*
* Revision 1.65  2004/04/27 19:00:08  kuznets
* Commented out old cache modes
*
* Revision 1.64  2004/04/09 20:37:48  vasilche
* Added optional test to get mapped location and object for graphs.
*
* Revision 1.63  2004/04/08 14:11:57  vasilche
* Added option to dump loaded Seq-entry.
*
* Revision 1.62  2004/04/05 15:56:13  grichenk
* Redesigned CAnnotTypes_CI: moved all data and data collecting
* functions to CAnnotDataCollector. CAnnotTypes_CI is no more
* inherited from SAnnotSelector.
*
* Revision 1.61  2004/03/18 16:30:24  grichenk
* Changed type of seq-align containers from list to vector.
*
* Revision 1.60  2004/02/09 19:18:50  grichenk
* Renamed CDesc_CI to CSeq_descr_CI. Redesigned CSeq_descr_CI
* and CSeqdesc_CI to avoid using data directly.
*
* Revision 1.59  2004/02/09 14:54:22  vasilche
* Dump synonyms of the sequence.
*
* Revision 1.58  2004/02/05 14:53:53  vasilche
* Used "new" cache interface by default.
*
* Revision 1.57  2004/02/04 18:05:34  grichenk
* Added annotation filtering by set of types/subtypes.
* Renamed *Choice to *Type in SAnnotSelector.
*
* Revision 1.56  2004/01/30 19:18:49  vasilche
* Added -seq_map flag.
*
* Revision 1.55  2004/01/29 20:38:47  vasilche
* Removed loading of whole sequence by CSeqMap_CI.
*
* Revision 1.54  2004/01/26 18:06:35  vasilche
* Add option for printing Seq-align.
* Use MSerial_Asn* manipulators.
* Removed unused includes.
*
* Revision 1.53  2004/01/07 17:38:02  vasilche
* Fixed include path to genbank loader.
*
* Revision 1.52  2004/01/05 18:14:03  vasilche
* Fixed name of project and path to header.
*
* Revision 1.51  2003/12/30 20:00:41  vasilche
* Added test for CGBDataLoader::GetSatSatKey().
*
* Revision 1.50  2003/12/30 19:51:54  vasilche
* Test CGBDataLoader::GetSatSatkey() method.
*
* Revision 1.49  2003/12/30 16:01:41  vasilche
* Added possibility to specify type of cache to use: -cache_mode (old|newid|new).
*
* Revision 1.48  2003/12/19 19:50:22  vasilche
* Removed obsolete split code.
* Do not use intemediate gi.
*
* Revision 1.47  2003/12/02 23:20:22  vasilche
* Allow to specify any Seq-id via ASN.1 text.
*
* Revision 1.46  2003/11/26 17:56:01  vasilche
* Implemented ID2 split in ID1 cache.
* Fixed loading of splitted annotations.
*
* Revision 1.45  2003/10/27 15:06:31  vasilche
* Added option to set ID1 cache keep time.
*
* Revision 1.44  2003/10/21 14:27:35  vasilche
* Added caching of gi -> sat,satkey,version resolution.
* SNP blobs are stored in cache in preprocessed format (platform dependent).
* Limit number of connections to GenBank servers.
* Added collection of ID1 loader statistics.
*
* Revision 1.43  2003/10/14 18:29:05  vasilche
* Added -exclude_named option.
*
* Revision 1.42  2003/10/09 20:20:58  vasilche
* Added possibility to include and exclude Seq-annot names to annot iterator.
* Fixed adaptive search. It looked only on selected set of annot names before.
*
* Revision 1.41  2003/10/08 17:55:32  vasilche
* Print sequence gi when -id option is used.
*
* Revision 1.40  2003/10/07 13:43:23  vasilche
* Added proper handling of named Seq-annots.
* Added feature search from named Seq-annots.
* Added configurable adaptive annotation search (default: gene, cds, mrna).
* Fixed selection of blobs for loading from GenBank.
* Added debug checks to CSeq_id_Mapper for easier finding lost CSeq_id_Handles.
* Fixed leaked split chunks annotation stubs.
* Moved some classes definitions in separate *.cpp files.
*
* Revision 1.39  2003/09/30 20:13:38  vasilche
* Print original feature location if requested.
*
* Revision 1.38  2003/09/30 16:22:05  vasilche
* Updated internal object manager classes to be able to load ID2 data.
* SNP blobs are loaded as ID2 split blobs - readers convert them automatically.
* Scope caches results of requests for data to data loaders.
* Optimized CSeq_id_Handle for gis.
* Optimized bioseq lookup in scope.
* Reduced object allocations in annotation iterators.
* CScope is allowed to be destroyed before other objects using this scope are
* deleted (feature iterators, bioseq handles etc).
* Optimized lookup for matching Seq-ids in CSeq_id_Mapper.
* Added 'adaptive' option to objmgr_demo application.
*
* Revision 1.37  2003/08/27 14:22:01  vasilche
* Added options get_mapped_location, get_mapped_feature and get_original_feature
* to test feature iterator speed.
*
* Revision 1.36  2003/08/15 19:19:16  vasilche
* Fixed memory leak in string packing hooks.
* Fixed processing of 'partial' flag of features.
* Allow table packing of non-point SNP.
* Allow table packing of SNP with long alleles.
*
* Revision 1.35  2003/08/14 20:05:20  vasilche
* Simple SNP features are stored as table internally.
* They are recreated when needed using CFeat_CI.
*
* Revision 1.34  2003/07/25 21:41:32  grichenk
* Implemented non-recursive mode for CSeq_annot_CI,
* fixed friend declaration in CSeq_entry_Info.
*
* Revision 1.33  2003/07/25 15:25:27  grichenk
* Added CSeq_annot_CI class
*
* Revision 1.32  2003/07/24 20:36:17  vasilche
* Added arguemnt to choose ID1<->PUBSEQOS on Windows easier.
*
* Revision 1.31  2003/07/17 20:06:18  vasilche
* Added OBJMGR_LIBS definition.
*
* Revision 1.30  2003/07/17 19:10:30  grichenk
* Added methods for seq-map and seq-vector validation,
* updated demo.
*
* Revision 1.29  2003/07/08 15:09:44  vasilche
* Added argument to test depth of segment resolution.
*
* Revision 1.28  2003/06/25 20:56:32  grichenk
* Added max number of annotations to annot-selector, updated demo.
*
* Revision 1.27  2003/06/02 16:06:38  dicuccio
* Rearranged src/objects/ subtree.  This includes the following shifts:
*     - src/objects/asn2asn --> arc/app/asn2asn
*     - src/objects/testmedline --> src/objects/ncbimime/test
*     - src/objects/objmgr --> src/objmgr
*     - src/objects/util --> src/objmgr/util
*     - src/objects/alnmgr --> src/objtools/alnmgr
*     - src/objects/flat --> src/objtools/flat
*     - src/objects/validator --> src/objtools/validator
*     - src/objects/cddalignview --> src/objtools/cddalignview
* In addition, libseq now includes six of the objects/seq... libs, and libmmdb
* replaces the three libmmdb? libs.
*
* Revision 1.26  2003/05/27 20:54:52  grichenk
* Fixed CRef<> to bool convertion
*
* Revision 1.25  2003/05/06 16:52:54  vasilche
* Added 'pause' argument.
*
* Revision 1.24  2003/04/29 19:51:14  vasilche
* Fixed interaction of Data Loader garbage collector and TSE locking mechanism.
* Made some typedefs more consistent.
*
* Revision 1.23  2003/04/24 16:12:38  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.22  2003/04/15 14:24:08  vasilche
* Changed CReader interface to not to use fake streams.
*
* Revision 1.21  2003/04/03 14:17:43  vasilche
* Allow using data loaded from file.
*
* Revision 1.20  2003/03/27 19:40:11  vasilche
* Implemented sorting in CGraph_CI.
* Added Rewind() method to feature/graph/align iterators.
*
* Revision 1.19  2003/03/26 17:27:04  vasilche
* Added optinal reverse feature traversal.
*
* Revision 1.18  2003/03/21 14:51:41  vasilche
* Added debug printing of features collected.
*
* Revision 1.17  2003/03/18 21:48:31  grichenk
* Removed obsolete class CAnnot_CI
*
* Revision 1.16  2003/03/10 16:33:44  vasilche
* Added dump of features if errors were detected.
*
* Revision 1.15  2003/02/27 20:57:36  vasilche
* Addef some options for better performance testing.
*
* Revision 1.14  2003/02/24 19:02:01  vasilche
* Reverted testing shortcut.
*
* Revision 1.13  2003/02/24 18:57:22  vasilche
* Make feature gathering in one linear pass using CSeqMap iterator.
* Do not use feture index by sub locations.
* Sort features at the end of gathering in one vector.
* Extracted some internal structures and classes in separate header.
* Delay creation of mapped features.
*
* Revision 1.12  2002/12/06 15:36:01  grichenk
* Added overlap type for annot-iterators
*
* Revision 1.11  2002/12/05 19:28:33  grichenk
* Prohibited postfix operator ++()
*
* Revision 1.10  2002/11/08 19:43:36  grichenk
* CConstRef<> constructor made explicit
*
* Revision 1.9  2002/11/04 21:29:13  grichenk
* Fixed usage of const CRef<> and CRef<> constructor
*
* Revision 1.8  2002/10/02 17:58:41  grichenk
* Added CBioseq_CI sample code
*
* Revision 1.7  2002/09/03 21:27:03  grichenk
* Replaced bool arguments in CSeqVector constructor and getters
* with enums.
*
* Revision 1.6  2002/06/12 14:39:03  grichenk
* Renamed enumerators
*
* Revision 1.5  2002/05/06 03:28:49  vakatov
* OM/OM1 renaming
*
* Revision 1.4  2002/05/03 21:28:11  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.3  2002/05/03 18:37:34  grichenk
* Added more examples of using CFeat_CI and GetSequenceView()
*
* Revision 1.2  2002/03/28 14:32:58  grichenk
* Minor fixes
*
* Revision 1.1  2002/03/28 14:07:25  grichenk
* Initial revision
*
*
* ===========================================================================
*/
