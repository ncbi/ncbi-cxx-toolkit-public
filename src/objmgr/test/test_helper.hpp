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
* Authors:  Eugene Vasilchenko, Aleksey Grichenko, Denis Vakatov
*
* File Description:
*   Bio sequence data generator to test Object Manager
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.11  2004/02/03 17:58:50  vasilche
* Always test CScope::RemoveEntry() in single thread.
*
* Revision 1.10  2003/06/02 16:06:39  dicuccio
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
* Revision 1.9  2003/03/04 16:43:53  grichenk
* +Test CFeat_CI with eResolve_All flag
*
* Revision 1.8  2003/02/28 16:37:47  vasilche
* Fixed expected feature count.
* Added optional flags to testobjmgr to dump generated data and found features.
*
* Revision 1.7  2002/12/26 16:39:24  vasilche
* Object manager class CSeqMap rewritten.
*
* Revision 1.6  2002/05/09 14:21:50  grichenk
* Turned GetTitle() test on, removed unresolved seq-map test
*
* Revision 1.5  2002/05/06 03:28:53  vakatov
* OM/OM1 renaming
*
* Revision 1.4  2002/05/03 21:28:12  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.3  2002/04/22 20:07:45  grichenk
* Commented calls to CBioseq::ConstructExcludedSequence()
*
* Revision 1.2  2002/03/18 21:47:15  grichenk
* Moved most includes to test_helper.cpp
* Added test for CBioseq::ConstructExcludedSequence()
*
* Revision 1.1  2002/03/13 18:06:31  gouriano
* restructured MT test. Put common functions into a separate file
*
*
* ===========================================================================
*/

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_entry;
class CSeq_annot;
class CScope;
class CSeq_id;

class CDataGenerator
{
public:
    static CSeq_entry& CreateTestEntry1(int index);
    static CSeq_entry& CreateTestEntry2(int index);
    static CSeq_entry& CreateTestEntry1a(int index);
    static CSeq_entry& CreateConstructedEntry(int idx, int index);
    // static CSeq_entry& CreateConstructedExclusionEntry(int idx, int index);
    static CSeq_annot& CreateAnnotation1(int index);

    static bool sm_DumpEntries;
};

class CTestHelper
{
public:
    static void ProcessBioseq(CScope& scope, CSeq_id& id,
                              TSeqPos seq_len,
                              string seq_str, string seq_str_compl,
                              int seq_desc_cnt,
                              int seq_feat_ra_cnt,
                              int seq_feat_cnt, int seq_featrg_cnt,
                              int seq_align_cnt, int seq_alignrg_cnt,
                              size_t feat_annots_cnt, size_t featrg_annots_cnt,
                              size_t align_annots_cnt, size_t alignrg_annots_cnt,
                              bool tse_feat_test = false,
                              bool have_errors = false);

    static void TestDataRetrieval( CScope& scope, int idx, int delta);

    static bool sm_DumpFeatures;
    static bool sm_TestRemoveEntry;
};

END_SCOPE(objects)
END_NCBI_SCOPE

