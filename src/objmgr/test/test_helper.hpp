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
* Revision 1.1  2002/03/13 18:06:31  gouriano
* restructured MT test. Put common functions into a separate file
*
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>

#include <serial/object.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objcopy.hpp>
#include <serial/objectinfo.hpp>
#include <serial/iterator.hpp>
#include <serial/objectiter.hpp>
#include <serial/serial.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/IUPACna.hpp>
#include <objects/seq/NCBIeaa.hpp>
#include <objects/seq/NCBI2na.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Seg_ext.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seqalign/Dense_diag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seqalign/Seq_align.hpp>


#include <objects/objmgr1/object_manager.hpp>
#include <objects/objmgr1/bioseq_handle.hpp>
#include <objects/objmgr1/scope.hpp>
#include <objects/objmgr1/seq_vector.hpp>
#include <objects/objmgr1/desc_ci.hpp>
#include <objects/objmgr1/feat_ci.hpp>
#include <objects/objmgr1/align_ci.hpp>

#include <objects/seq/seqport_util.hpp>

#include <objects/general/Date.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CDataGenerator
{
public:
    static CRef<CSeq_entry> CreateTestEntry1(int index);
    static CRef<CSeq_entry> CreateTestEntry2(int index);
    static CRef<CSeq_entry> CreateTestEntry1a(int index);
    static CRef<CSeq_entry> CreateConstructedEntry(int idx, int index);
    static CRef<CSeq_annot> CreateAnnotation1(int index);
};

class CTestHelper
{
public:
    static void ProcessBioseq(CScope& scope, CSeq_id& id,
        int seq_len_unresolved, int seq_len_resolved,
        string seq_str, string seq_str_compl,
        int seq_desc_cnt,
        int seq_feat_cnt, int seq_featrg_cnt,
        int seq_align_cnt, int seq_alignrg_cnt,
        int feat_annots_cnt, int featrg_annots_cnt,
        int align_annots_cnt, int alignrg_annots_cnt,
        bool tse_feat_test = false);

    static void TestDataRetrieval( CScope& scope, int idx,
        int delta, bool check_unresolved);
};

END_SCOPE(objects)
END_NCBI_SCOPE

