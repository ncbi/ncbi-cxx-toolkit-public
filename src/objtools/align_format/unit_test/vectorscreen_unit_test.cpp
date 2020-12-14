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
* Author:  Jian Ye
*
* File Description:
*   Unit test module to test CVecscreen
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#define NCBI_BOOST_NO_AUTO_TEST_MAIN
#include <corelib/test_boost.hpp>
#include <boost/test/auto_unit_test.hpp>
#include <boost/test/floating_point_comparison.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbiutil.hpp>
#include <corelib/ncbistre.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/util/sequence.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqloc/Seq_id.hpp>

#include <objtools/align_format/align_format_util.hpp>
#include <objtools/align_format/vectorscreen.hpp>

using namespace std;
using namespace ncbi;
using namespace ncbi::objects;
using namespace ncbi::align_format;

BOOST_AUTO_TEST_SUITE(vectorscreen)

struct CVecscreenTest : public CVecscreen {
    CVecscreenTest(const CSeq_align_set& seqalign, TSeqPos master_length)
        : CVecscreen(seqalign, master_length) 
    {}

    static void x_TestRangeList(CVecscreenTest& vec) {
        //test the range list
        BOOST_REQUIRE(vec.m_AlnInfoList.size() == 5);
        TSeqPos i = 0;
        TSeqPos from[] = {0, 11, 33, 34, 1008};
        TSeqPos to[] = {10, 32, 33, 1007, 1056};
        int type[] = {3, 1, 2, 4, 0};

        ITERATE(list<CVecscreen::AlnInfo*>, iter, vec.m_AlnInfoList){
            BOOST_REQUIRE((*iter)->range.GetFrom() == from[i]);
            BOOST_REQUIRE((*iter)->range.GetTo() == to[i]);
            BOOST_REQUIRE((*iter)->type == type[i]);
            i ++;
        }
    }

    static void x_TestSeqalign(CVecscreenTest& vec) {
        //test the processed seqalign 
        BOOST_REQUIRE(vec.m_FinalSeqalign->Get().size() == 3);
        int i = 0;
        TSeqPos from[] = {1008, 11, 18};
        TSeqPos to[] = {1056, 32, 33};
        ITERATE(CSeq_align_set::Tdata, iter, vec.m_FinalSeqalign->Get()){
            BOOST_REQUIRE((*iter)->GetSeqRange(0).GetFrom() == from[i]);
            BOOST_REQUIRE((*iter)->GetSeqRange(0).GetTo() == to[i]);
            i ++;
        }   
    }      

    static void VecscreenDisplay(void)
    {
        CNcbiIfstream is("data/seqalign.vectorscreen");
        unique_ptr<CObjectIStream> in(CObjectIStream::Open(eSerial_AsnText, is));
        CRef<CSeq_annot> san(new CSeq_annot);
        *in >> *san;
        const CSeq_annot::TData& data = san->GetData();
        const CSeq_annot::TData::TAlign& align= data.GetAlign();
        CRef<CSeq_align_set> seqalign(new CSeq_align_set);
        seqalign->Set() = align;
        CVecscreenTest vec(*seqalign, 1057);
        CSeq_align_set actual_aln_list;
        CAlignFormatUtil::ExtractSeqalignSetFromDiscSegs(actual_aln_list, 
                                                         *(vec.m_SeqalignSetRef));
        vec.x_MergeSeqalign(actual_aln_list); 
        
        //test the range list
        x_TestRangeList(vec);

        //test the processed seqalign
        x_TestSeqalign(vec);
    }
};

BOOST_AUTO_TEST_CASE(VecscreenDisplay)
{
    CVecscreenTest::VecscreenDisplay();
}

BOOST_AUTO_TEST_SUITE_END()

/*
* ===========================================================================

* ===========================================================================
*/

