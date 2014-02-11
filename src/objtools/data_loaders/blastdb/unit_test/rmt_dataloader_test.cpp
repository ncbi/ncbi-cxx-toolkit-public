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
* Author:  Tom Madden, NCBI
*
* File Description:
*   Unit tests for remote data loader
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <serial/serial.hpp>
#include <serial/objostr.hpp>
#include <serial/exception.hpp>
#include <util/range.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/blastdb/Blast_def_line_set.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_vector.hpp>

#include <objtools/blast/services/blast_services.hpp>

#include <objtools/data_loaders/blastdb/bdbloader_rmt.hpp>



// This macro should be defined before inclusion of test_boost.hpp in all
// "*.cpp" files inside executable except one. It is like function main() for
// non-Boost.Test executables is defined only in one *.cpp file - other files
// should not include it. If NCBI_BOOST_NO_AUTO_TEST_MAIN will not be defined
// then test_boost.hpp will define such "main()" function for tests.
//
// Usually if your unit tests contain only one *.cpp file you should not
// care about this macro at all.
//
// #define NCBI_BOOST_NO_AUTO_TEST_MAIN


// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>

USING_NCBI_SCOPE;
using namespace ncbi::objects;
BEGIN_SCOPE(blast)

/// Auxiliary class to register the BLAST database data loader of choice
/// (information provided in the constructor) and deactivate it on object
/// destruction (shamelessly copied from bdbloader_unit_test.cpp)
/// @sa bdbloader_unit_test.cpp
class CAutoRegistrar {
public:
    CAutoRegistrar(const string& dbname, bool is_protein,
                   bool use_fixed_slice_size, 
                   bool use_remote_blast_db_loader = false) {
        CRef<CObjectManager> om = CObjectManager::GetInstance();
        if (use_remote_blast_db_loader) {
            loader_name = CRemoteBlastDbDataLoader::RegisterInObjectManager
                        (*om, dbname, is_protein 
                         ? CBlastDbDataLoader::eProtein
                         : CBlastDbDataLoader::eNucleotide,
                         use_fixed_slice_size,
                         CObjectManager::eDefault, 
                         CObjectManager::kPriority_NotSet)
                        .GetLoader()->GetName();
        } else {
            loader_name = CBlastDbDataLoader::RegisterInObjectManager
                        (*om, dbname, is_protein 
                         ? CBlastDbDataLoader::eProtein
                         : CBlastDbDataLoader::eNucleotide,
                         use_fixed_slice_size,
                         CObjectManager::eDefault, 
                         CObjectManager::kPriority_NotSet)
                        .GetLoader()->GetName();
        }
        om->SetLoaderOptions(loader_name, CObjectManager::eDefault);
    }

    //void RegisterGenbankDataLoader() {
    //    CRef<CObjectManager> om = CObjectManager::GetInstance();
    //    gbloader_name = 
    //        CGBDataLoader::RegisterInObjectManager(*om).GetLoader()->GetName();
    //}

    static void RemoveAllDataLoaders() {
        CRef<CObjectManager> om = CObjectManager::GetInstance();
        CObjectManager::TRegisteredNames loader_names;
        om->GetRegisteredNames(loader_names);
        ITERATE(CObjectManager::TRegisteredNames, itr, loader_names) {
            om->RevokeDataLoader(*itr);
        }
    }

    ~CAutoRegistrar() {
        CRef<CObjectManager> om = CObjectManager::GetInstance();
        om->RevokeDataLoader(loader_name);
        //if ( !gbloader_name.empty() ) {
        //    om->RevokeDataLoader(gbloader_name);
        //}
    }

private:
    string loader_name;
    //string gbloader_name;
};


BOOST_AUTO_TEST_CASE(RemoteFetchNucleotideBioseq)
{
    const string db("nt");
    const bool is_protein(false);
    const bool use_fixed_size_slice(true);
    const bool is_remote(true);
    CAutoRegistrar::RemoveAllDataLoaders();
    CAutoRegistrar reg(db, is_protein, use_fixed_size_slice, is_remote);
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CObjectManager::TRegisteredNames loader_names;
    objmgr->GetRegisteredNames(loader_names);
    BOOST_REQUIRE_EQUAL(1U, loader_names.size());
    BOOST_REQUIRE_EQUAL("REMOTE_BLASTDB_ntNucleotide", loader_names.front());

    {{ // limit the lifespan of the CScope object
        CScope scope(*objmgr);

        scope.AddDataLoader(loader_names.front());

        CSeq_id seqid1(CSeq_id::e_Gi, 555); // nucleotide
        CBioseq_Handle handle1 = scope.GetBioseqHandle(seqid1);
        BOOST_REQUIRE_EQUAL((TSeqPos)624, handle1.GetInst().GetLength());
        BOOST_REQUIRE_EQUAL(9913, scope.GetTaxId(seqid1));
        BOOST_REQUIRE_EQUAL(CSeq_inst::eMol_na, scope.GetSequenceType(seqid1));

        CConstRef<CBioseq> bioseq1 = handle1.GetCompleteBioseq();
        BOOST_REQUIRE_EQUAL((TSeqPos)624, bioseq1->GetInst().GetLength());

        CSeq_id seqid2(CSeq_id::e_Gi, 129295); // protein
        CBioseq_Handle handle2 = scope.GetBioseqHandle(seqid2);
        BOOST_REQUIRE(!handle2);
        BOOST_REQUIRE(handle2.State_NoData());
    }}
}

static void RemoteFetchLongNucleotideBioseq(bool fixed_slice_size)
{          
    const string db("nucl_dbs");
    const bool is_protein(false);
    const bool is_remote(true);
    CAutoRegistrar::RemoveAllDataLoaders();
    CAutoRegistrar reg(db, is_protein, fixed_slice_size, is_remote);
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CObjectManager::TRegisteredNames loader_names;
    objmgr->GetRegisteredNames(loader_names);
    BOOST_REQUIRE_EQUAL(1, loader_names.size());
    BOOST_REQUIRE_EQUAL("REMOTE_BLASTDB_nucl_dbsNucleotide",
                        loader_names.front());

    {{ // limit the lifespan of the CScope object
        CScope scope(*objmgr);
        
        scope.AddDataLoader(loader_names.front());
        
        CSeq_id seqid1(CSeq_id::e_Gi, 568801968);  // nucleotide
        
        CBioseq_Handle handle1 = scope.GetBioseqHandle(seqid1);
        BOOST_REQUIRE(handle1);
        const TSeqPos kLength(3084811);
        BOOST_REQUIRE_EQUAL(kLength, handle1.GetInst().GetLength());
        BOOST_REQUIRE_EQUAL(9606, scope.GetTaxId(seqid1));
        BOOST_REQUIRE_EQUAL(CSeq_inst::eMol_na, scope.GetSequenceType(seqid1));

        CSeqVector sv = handle1.GetSeqVector(CBioseq_Handle::eCoding_Iupac);
        const string
            kExpectedSeqData("ATTAACTGCAAATTACACGTATTGAGATGCATAAAAAGCCAAACCCTTGGGATAAAAATCTGAAAAGCTTTAAGAGGAAAAGTCTACCTCCTGAAATGAA");
        string buffer;
        sv.GetSeqData(393200, 393300, buffer);
        BOOST_REQUIRE_EQUAL(kExpectedSeqData, buffer);

        CConstRef<CBioseq> bioseq1 = handle1.GetCompleteBioseq();
        BOOST_REQUIRE_EQUAL(kLength, bioseq1->GetInst().GetLength());
    }}
}    
     
BOOST_AUTO_TEST_CASE(RemoteFetchLongNucleotideBioseq_FixedSliceSize)
{
    RemoteFetchLongNucleotideBioseq(true);
}

BOOST_AUTO_TEST_CASE(RemoteFetchLongNucleotideBioseq_NotFixedSliceSize)
{          
    RemoteFetchLongNucleotideBioseq(false);
}    

BOOST_AUTO_TEST_CASE(RemoteFetchMultipleProteins_FixedSlice)
{
    const string db("nr");
    const bool is_protein(true);
    const bool kFixedSliceSize(true);
    const bool is_remote(true);
    CAutoRegistrar::RemoveAllDataLoaders();
    CAutoRegistrar reg_prot(db, is_protein, kFixedSliceSize, is_remote);
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CObjectManager::TRegisteredNames loader_names;
    objmgr->GetRegisteredNames(loader_names);

    CRef<CScope> scope(new CScope(*objmgr)); 
    
    ITERATE(CObjectManager::TRegisteredNames, name, loader_names) {
        scope->AddDataLoader(*name);
    }

    typedef pair<int, CSeq_inst::TLength> TGiLengthPair;
    typedef vector<TGiLengthPair> TGiLengthVector;
    TGiLengthVector test_data;
    test_data.push_back(TGiLengthPair(129295, 232));
    test_data.push_back(TGiLengthPair(129296, 388));
    test_data.push_back(TGiLengthPair(129295, 232));

    CScope::TIds ids;
    ITERATE(TGiLengthVector, itr, test_data) {
        ids.push_back(CSeq_id_Handle::GetHandle(GI_FROM(int, itr->first)));
    }
    BOOST_REQUIRE_EQUAL(ids.size(), test_data.size());

    CScope::TBioseqHandles bhs = scope->GetBioseqHandles(ids);
    BOOST_REQUIRE_EQUAL(ids.size(), bhs.size());

    TGiLengthVector::size_type i = 0;
    ITERATE(CScope::TBioseqHandles, bh, bhs) {
        BOOST_REQUIRE_EQUAL(bh->GetInst().GetLength(), test_data[i].second);
        i++;
    }
    BOOST_REQUIRE_EQUAL(i, bhs.size());
}

BOOST_AUTO_TEST_CASE(RemoteFetchProteinsAndNucleotides_FixedSlice)
{
    const string db("nr");
    const bool is_protein(true);
    const bool kFixedSliceSize(true);
    const bool is_remote(true);
    CAutoRegistrar::RemoveAllDataLoaders();
    CAutoRegistrar reg_prot(db, is_protein, kFixedSliceSize, is_remote);
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CObjectManager::TRegisteredNames loader_names;
    objmgr->GetRegisteredNames(loader_names);
    BOOST_REQUIRE_EQUAL(1, loader_names.size());
    BOOST_REQUIRE_EQUAL("REMOTE_BLASTDB_nrProtein", loader_names.front());

    CAutoRegistrar reg_nucl(db, !is_protein, kFixedSliceSize, is_remote);
    loader_names.clear();
    objmgr->GetRegisteredNames(loader_names);
    BOOST_REQUIRE_EQUAL(2, loader_names.size());
    bool found_prot(false), found_nucl(false);
    ITERATE(CObjectManager::TRegisteredNames, name, loader_names) {
        if (*name == string("REMOTE_BLASTDB_nrNucleotide")) {
            found_nucl = true;
        }
        if (*name == string("REMOTE_BLASTDB_nrProtein")) {
            found_prot = true;
        }
    }
    BOOST_REQUIRE(found_nucl);
    BOOST_REQUIRE(found_prot);

    //{{ // limit the lifespan of the CScope object
        CRef<CScope> scope(new CScope(*objmgr)); 
        
        ITERATE(CObjectManager::TRegisteredNames, name, loader_names) {
            scope->AddDataLoader(*name);
        }

        CSeq_id seqid1(CSeq_id::e_Gi, 129295);  // protein
        CBioseq_Handle handle1 = scope->GetBioseqHandle(seqid1);
        BOOST_REQUIRE(handle1);
        BOOST_REQUIRE_EQUAL((TSeqPos)232, handle1.GetInst().GetLength());
        BOOST_REQUIRE_EQUAL(9031, scope->GetTaxId(seqid1));
        BOOST_REQUIRE_EQUAL(CSeq_inst::eMol_aa, scope->GetSequenceType(seqid1));
        
        CConstRef<CBioseq> bioseq1 = handle1.GetCompleteBioseq();
        BOOST_REQUIRE_EQUAL((TSeqPos)232, bioseq1->GetInst().GetLength());

        CSeq_id seqid2(CSeq_id::e_Gi, 555); // nucleotide
        CBioseq_Handle handle2 = scope->GetBioseqHandle(seqid2);
        BOOST_REQUIRE(handle2);
        BOOST_REQUIRE_EQUAL((TSeqPos)624, handle2.GetInst().GetLength());
        BOOST_REQUIRE_EQUAL(9913, scope->GetTaxId(seqid2));
        BOOST_REQUIRE_EQUAL(CSeq_inst::eMol_na, scope->GetSequenceType(seqid2));

        CConstRef<CBioseq> bioseq2 = handle2.GetCompleteBioseq();
        BOOST_REQUIRE_EQUAL((TSeqPos)624, bioseq2->GetInst().GetLength());
    //}}

}
     
BOOST_AUTO_TEST_CASE(RemoteFetchProteinBioseq)
{          
    const string dbname("nr");
    const bool is_protein(true);
    const bool use_fixed_size_slice(true);
    const bool is_remote(true);
    CAutoRegistrar::RemoveAllDataLoaders();
    CAutoRegistrar reg(dbname, is_protein, use_fixed_size_slice, is_remote);
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CObjectManager::TRegisteredNames loader_names;
    objmgr->GetRegisteredNames(loader_names);
    BOOST_REQUIRE_EQUAL(1, loader_names.size());
    BOOST_REQUIRE_EQUAL("REMOTE_BLASTDB_nrProtein", loader_names.front());
    
    {{ // limit the lifespan of the CScope object
        CScope scope(*objmgr); 
        
        scope.AddDataLoader(loader_names.front());

        CSeq_id seqid1(CSeq_id::e_Gi, 129295);  // protein
        CBioseq_Handle handle1 = scope.GetBioseqHandle(seqid1);
        BOOST_REQUIRE(handle1);
        BOOST_REQUIRE_EQUAL((TSeqPos)232, handle1.GetInst().GetLength());
        
        CConstRef<CBioseq> bioseq1 = handle1.GetCompleteBioseq();
        BOOST_REQUIRE_EQUAL((TSeqPos)232, bioseq1->GetInst().GetLength());

        CSeq_id seqid2(CSeq_id::e_Gi, 555); // nucleotide
        CBioseq_Handle handle2 = scope.GetBioseqHandle(seqid2);
        BOOST_REQUIRE(!handle2);
        BOOST_REQUIRE(handle2.State_NoData());
    }}
}    

END_SCOPE(blast)
