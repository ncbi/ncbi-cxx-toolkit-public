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
*   Unit test module to test CBlastFormatUtil
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbiutil.hpp>
#include <corelib/ncbistre.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <algo/blast/blastinput/blast_scope_src.hpp>
#include <algo/blast/format/blastfmtutil.hpp>
#include <objmgr/util/sequence.hpp>
#include <objtools/blast/seqdb_reader/seqdb.hpp>

#define NCBI_BOOST_NO_AUTO_TEST_MAIN
#include <corelib/test_boost.hpp>
#include <boost/test/auto_unit_test.hpp>
#include <boost/test/floating_point_comparison.hpp>


using namespace ncbi;
using namespace ncbi::blast;
using namespace ncbi::objects;

// Maximum acceptable difference between values of double type
double const kMaxDoubleDiff = 1e-13;

struct DataLoaderRevoker {

    DataLoaderRevoker(CRef<CObjectManager> obj, string name)
        : m_Obj(obj), m_Name(name)
    {}

    DataLoaderRevoker(const string& name)
        : m_Obj(CObjectManager::GetInstance()), m_Name(name)
    {}
    
    ~DataLoaderRevoker()
    {
        // disable the C++ BLAST database data loader
        m_Obj->SetLoaderOptions(m_Name, CObjectManager::eNonDefault);
    }
    

    CRef<CObjectManager> m_Obj;
    string               m_Name;
};

BOOST_AUTO_TEST_SUITE(blastfmtutil)

BOOST_AUTO_TEST_CASE(GetBlastDeflineFromGenbank)
{
    const int gi = 555;
    const bool is_prot = false;
    SDataLoaderConfig config(is_prot, SDataLoaderConfig::eUseGenbankDataLoader);

    CBlastScopeSource ss(config);
    CRef<CScope> scope = ss.NewScope();
    DataLoaderRevoker revoker(CGBDataLoader::GetLoaderNameFromArgs());

    CSeq_id id;
    id.SetGi(gi);

    CBioseq_Handle bh = scope->GetBioseqHandle(id);
    BOOST_REQUIRE(bh);

    CRef<CBlast_def_line_set> defline = CSeqDB::ExtractBlastDefline(bh);
    BOOST_REQUIRE(defline.Empty());
}

BOOST_AUTO_TEST_CASE(GetBlastDeflineFromBlastDb)
{
    CRef<CObjectManager> obj = CObjectManager::GetInstance();  
    CRef<CScope> scope;
    const string kDbName("nr");
    const CBlastDbDataLoader::EDbType 
        kDbType(CBlastDbDataLoader::eNucleotide);
    CBlastDbDataLoader::RegisterInObjectManager(*obj, 
                                                kDbName,
                                                kDbType, true,
                                                CObjectManager::eNonDefault);
    
    scope = new CScope(*obj);
    
    string name = 
        CBlastDbDataLoader::GetLoaderNameFromArgs(kDbName, kDbType);
    
    DataLoaderRevoker revoker(obj, name);
    
    scope->AddDataLoader(name);
    int gi = sequence::GetGiForAccession("NM_001256", *scope);
    CSeq_id id;
    id.SetGi(gi);
    const CBioseq_Handle& handle = scope->GetBioseqHandle(id);
    const CRef<CBlast_def_line_set> bdls = CSeqDB::ExtractBlastDefline(handle);
    const list< CRef< CBlast_def_line > >& bdl = bdls->Get();
    ITERATE(list< CRef< CBlast_def_line > >, iter, bdl){
        const CBioseq::TId& cur_id = (*iter)->GetSeqid();
        int cur_gi =  FindGi(cur_id);
        BOOST_REQUIRE (gi==cur_gi);

        if ((*iter)->IsSetLinks()){
            int linkout = 0;
            ITERATE(list< int >, iter2, (*iter)->GetLinks()){
                if ((*iter2) & eUnigene) {
                    linkout += eUnigene;
                }
                if ((*iter2) & eStructure){
                    linkout += eStructure;
                } 
                if ((*iter2) & eGeo){
                    linkout += eGeo;
                } 
                if ((*iter2) & eGene){
                    linkout += eGene;
                }
            }
            BOOST_REQUIRE (linkout & eGene);
        }
    }
}

BOOST_AUTO_TEST_CASE(GetAlnScoresAndGetScoreString)
{

    CNcbiIfstream is("data/blastfmtutil-cppunit.aln");
    auto_ptr<CObjectIStream> in(CObjectIStream::Open(eSerial_AsnText, is));
    CRef<CSeq_annot> san(new CSeq_annot);
    *in >> *san;
    const CSeq_annot::TData& data = san->GetData();
    const CSeq_annot::TData::TAlign& align= data.GetAlign();
    
    int score, sum_n;
    double bits, evalue;
    list<int> use_this_gi;
    int num_ident;
    
    CBlastFormatUtil::GetAlnScores(*(align.front()), score, bits, evalue,
                                   sum_n, num_ident, use_this_gi);
    BOOST_REQUIRE(score == 1296); 
    BOOST_REQUIRE(sum_n == -1);
    BOOST_REQUIRE(use_this_gi.front() == 18426812);
    BOOST_REQUIRE_CLOSE(evalue, 217774e-146, kMaxDoubleDiff);
    BOOST_REQUIRE_CLOSE(bits, 503.263, 0.0001);
    BOOST_REQUIRE(num_ident == 331);

    string evalue_str, bit_score_str, raw_score_str, total_bit_score_str;
    CBlastFormatUtil::GetScoreString(evalue, bits, 0, 0,
                                     evalue_str, bit_score_str, 
                                     total_bit_score_str,
                                     raw_score_str);
    BOOST_REQUIRE(evalue_str == string("2e-141"));
    BOOST_REQUIRE(bit_score_str == string(" 503"));
}
    
BOOST_AUTO_TEST_SUITE_END()
