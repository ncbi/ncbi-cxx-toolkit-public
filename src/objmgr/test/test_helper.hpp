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
                              size_t seq_desc_cnt,
                              size_t seq_feat_ra_cnt,
                              size_t seq_feat_cnt, size_t seq_featrg_cnt,
                              size_t seq_align_cnt, size_t seq_alignrg_cnt,
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
