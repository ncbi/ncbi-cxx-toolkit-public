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
*/

#include <ncbi_pch.hpp>

#include <corelib/test_boost.hpp>
#if BOOST_VERSION >= 105900
#  include <boost/test/tools/output_test_stream.hpp>
#else
#  include <boost/test/output_test_stream.hpp>
#endif

#include <serial/iterator.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>

#include <objtools/readers/fasta.hpp>

#include <objtools/seqmasks_io/mask_reader.hpp>
#include <objtools/seqmasks_io/mask_writer.hpp>
#include <objtools/seqmasks_io/mask_writer_blastdb_maskinfo.hpp>
#include <objtools/seqmasks_io/mask_writer_fasta.hpp>
#include <objtools/seqmasks_io/mask_writer_int.hpp>
#include <objtools/seqmasks_io/mask_writer_tab.hpp>
#include <objtools/seqmasks_io/mask_writer_seqloc.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

struct seqmasks_io_fixture {

    seqmasks_io_fixture() {
        x_LoadMaskedBioseqGi555();
        x_LoadMaskedBioseqLocalId();
    }

    CBioseq_Handle m_BioseqHandleWithGi;
    CBioseq_Handle m_BioseqHandleWithLocalId;
    CMaskWriter::TMaskList m_Masks;

    size_t GetExpectedNumberOfMasks() const { return 1U; }
    TSeqPos GetMaskStart() const { return 78U; }
    TSeqPos GetMaskStop() const { return 89U; }

private:
    void x_ConvertMasks(CRef<CSeq_loc> mask) {
        m_Masks.clear();
        for (CTypeConstIterator<CSeq_interval> itr(ConstBegin(*mask)); itr;
             ++itr) {
            CMaskWriter::TMaskedInterval m(itr->GetStart(eExtreme_Positional),
                                           itr->GetStop(eExtreme_Positional));
            m_Masks.push_back(m);
        }
    }

    void x_LoadMaskedBioseqGi555() {
        m_BioseqHandleWithGi = x_LoadMaskedBioseq(true);
    }

    void x_LoadMaskedBioseqLocalId() {
        m_BioseqHandleWithLocalId = x_LoadMaskedBioseq(false);
    }

    CBioseq_Handle x_LoadMaskedBioseq(bool parse_seqids) {
        const char* kDataFile = "data/nt.555.mfsa";
        CNcbiIfstream in(kDataFile);
        _ASSERT(in);

        CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));
        scope->AddDefaults();

        CFastaReader::TFlags flags = CFastaReader::fAssumeNuc;
        if (parse_seqids) {
            flags |= CFastaReader::fAllSeqIds | CFastaReader::fUniqueIDs;
        } else {
            flags |= CFastaReader::fNoParseID;
        }
        CFastaReader fsa_reader(in, flags);
        CRef<CSeq_loc> mask = fsa_reader.SaveMask();
        CRef<CSeq_entry> se = fsa_reader.ReadOneSeq();
        _ASSERT(se->IsSeq());
        scope->AddTopLevelSeqEntry(*se);
        const CSeq_id& best_id = fsa_reader.GetBestID();
        x_ConvertMasks(mask);
        return scope->GetBioseqHandle(best_id);
    }
};

BOOST_FIXTURE_TEST_SUITE(seqmasks_io_tests, seqmasks_io_fixture)

BOOST_AUTO_TEST_CASE(WriteFastaParseSeqids)
{
    boost::test_tools::output_test_stream out("data/sample_parse_seqids_fasta.out");
    const bool kParseSeqids(true);
    unique_ptr<CMaskWriter> writer(new CMaskWriterFasta(out));
    writer->Print(m_BioseqHandleWithGi, m_Masks, kParseSeqids);
    BOOST_CHECK(out.match_pattern());

    //writer.reset(new CMaskWriterFasta(cout));
    //writer->Print(m_BioseqHandleWithGi, m_Masks, kParseSeqids);
}

BOOST_AUTO_TEST_CASE(WriteFastaNoParseSeqids)
{
    boost::test_tools::output_test_stream out("data/sample_fasta.out");
    const bool kParseSeqids(false);
    unique_ptr<CMaskWriter> writer(new CMaskWriterFasta(out));
    writer->Print(m_BioseqHandleWithLocalId, m_Masks, kParseSeqids);
    BOOST_CHECK(out.match_pattern());
    //writer.reset(new CMaskWriterFasta(cout));
    //writer->Print(m_BioseqHandleWithLocalId, m_Masks, kParseSeqids);
}

BOOST_AUTO_TEST_CASE(WriteAcclistParseSeqids)
{
    boost::test_tools::output_test_stream out("data/sample_parse_seqids_acclist.out");
    const bool kParseSeqids(true);
    unique_ptr<CMaskWriter> writer(new CMaskWriterTabular(out));
    writer->Print(m_BioseqHandleWithGi, m_Masks, kParseSeqids);
    BOOST_CHECK(out.match_pattern());

    //writer.reset(new CMaskWriterTabular(cout));
    //writer->Print(m_BioseqHandleWithGi, m_Masks, kParseSeqids);
}

BOOST_AUTO_TEST_CASE(WriteAcclistNoParseSeqids)
{
    boost::test_tools::output_test_stream out("data/sample_acclist.out");
    const bool kParseSeqids(false);
    unique_ptr<CMaskWriter> writer(new CMaskWriterTabular(out));
    writer->Print(m_BioseqHandleWithLocalId, m_Masks, kParseSeqids);
    BOOST_CHECK(out.match_pattern());

    //writer.reset(new CMaskWriterTabular(cout));
    //writer->Print(m_BioseqHandleWithLocalId, m_Masks, kParseSeqids);
}

BOOST_AUTO_TEST_CASE(WriteIntervalParseSeqids)
{
    boost::test_tools::output_test_stream out("data/sample_parse_seqids_interval.out");
    const bool kParseSeqids(true);
    unique_ptr<CMaskWriter> writer(new CMaskWriterInt(out));
    writer->Print(m_BioseqHandleWithGi, m_Masks, kParseSeqids);
    BOOST_CHECK(out.match_pattern());

    //writer.reset(new CMaskWriterInt(cout));
    //writer->Print(m_BioseqHandleWithGi, m_Masks, kParseSeqids);
}

BOOST_AUTO_TEST_CASE(WriteIntervalNoParseSeqids)
{
    boost::test_tools::output_test_stream out("data/sample_interval.out");
    const bool kParseSeqids(false);
    unique_ptr<CMaskWriter> writer(new CMaskWriterInt(out));
    writer->Print(m_BioseqHandleWithLocalId, m_Masks, kParseSeqids);
    BOOST_CHECK(out.match_pattern());

    //writer.reset(new CMaskWriterInt(cout));
    //writer->Print(m_BioseqHandleWithGi, m_Masks, kParseSeqids);
}

static const int kAlgoId(2);
static const string kAlgoOptions("window=64; level=20; linker=1");

BOOST_AUTO_TEST_CASE(WriteMaskInfoAsn1TextParseSeqids)
{
    boost::test_tools::output_test_stream out("data/sample_parse_seqids_maskinfo_asn1_text.out");
    const bool kParseSeqids(true);
    unique_ptr<CMaskWriter> writer(new CMaskWriterBlastDbMaskInfo(out,
                                                                "maskinfo_asn1_text",
                                                                kAlgoId,
                                                                eBlast_filter_program_dust,
                                                                kAlgoOptions));
    writer->Print(m_BioseqHandleWithGi, m_Masks, kParseSeqids);
    BOOST_CHECK(out.match_pattern());
}

BOOST_AUTO_TEST_CASE(WriteMaskInfoAsn1TextNoParseSeqids)
{
    boost::test_tools::output_test_stream out("data/sample_maskinfo_asn1_text.out");
    const bool kParseSeqids(false);
    unique_ptr<CMaskWriter> writer(new CMaskWriterBlastDbMaskInfo(out,
                                                                "maskinfo_asn1_text",
                                                                kAlgoId,
                                                                eBlast_filter_program_dust,
                                                                kAlgoOptions));
    writer->Print(m_BioseqHandleWithLocalId, m_Masks, kParseSeqids);
    BOOST_CHECK(out.match_pattern());
}

BOOST_AUTO_TEST_CASE(WriteMaskInfoAsn1BinaryParseSeqids)
{
    const bool kMatchOrSave(true);
    const bool kTextOrBin(false);
    boost::test_tools::output_test_stream
        out("data/sample_parse_seqids_maskinfo_asn1_bin.out",
            kMatchOrSave, kTextOrBin);
    const bool kParseSeqids(true);
    unique_ptr<CMaskWriter> writer(new CMaskWriterBlastDbMaskInfo(out,
                                                                "maskinfo_asn1_bin",
                                                                kAlgoId,
                                                                eBlast_filter_program_dust,
                                                                kAlgoOptions));
    writer->Print(m_BioseqHandleWithGi, m_Masks, kParseSeqids);
    BOOST_CHECK(out.match_pattern());
}

BOOST_AUTO_TEST_CASE(WriteMaskInfoAsn1BinaryNoParseSeqids)
{
    const bool kMatchOrSave(true);
    const bool kTextOrBin(false);
    boost::test_tools::output_test_stream out("data/sample_maskinfo_asn1_bin.out", 
            kMatchOrSave, kTextOrBin);
    const bool kParseSeqids(false);
    unique_ptr<CMaskWriter> writer(new CMaskWriterBlastDbMaskInfo(out,
                                                                "maskinfo_asn1_bin",
                                                                kAlgoId,
                                                                eBlast_filter_program_dust,
                                                                kAlgoOptions));
    writer->Print(m_BioseqHandleWithLocalId, m_Masks, kParseSeqids);
    BOOST_CHECK(out.match_pattern());
}

BOOST_AUTO_TEST_CASE(WriteMaskInfoXmlParseSeqids)
{
    boost::test_tools::output_test_stream out("data/sample_parse_seqids_maskinfo_xml.out");
    const bool kParseSeqids(true);
    unique_ptr<CMaskWriter> writer(new CMaskWriterBlastDbMaskInfo(out,
                                                                "maskinfo_xml",
                                                                kAlgoId,
                                                                eBlast_filter_program_dust,
                                                                kAlgoOptions));
    writer->Print(m_BioseqHandleWithGi, m_Masks, kParseSeqids);
    BOOST_CHECK(out.match_pattern());
}

BOOST_AUTO_TEST_CASE(WriteMaskInfoXmlNoParseSeqids)
{
    boost::test_tools::output_test_stream out("data/sample_maskinfo_xml.out");
    const bool kParseSeqids(false);
    unique_ptr<CMaskWriter> writer(new CMaskWriterBlastDbMaskInfo(out,
                                                                "maskinfo_xml",
                                                                kAlgoId,
                                                                eBlast_filter_program_dust,
                                                                kAlgoOptions));
    writer->Print(m_BioseqHandleWithLocalId, m_Masks, kParseSeqids);
    BOOST_CHECK(out.match_pattern());
}

BOOST_AUTO_TEST_CASE(WriteSeqLocAsn1TextParseSeqids)
{
    boost::test_tools::output_test_stream out(
        CSeq_id::PreferAccessionOverGi() ?
        "data/sample_prefacc_parse_seqids_seqloc_asn1_text.out" :
        "data/sample_parse_seqids_seqloc_asn1_text.out");
    const bool kParseSeqids(true);
    unique_ptr<CMaskWriter> writer(new CMaskWriterSeqLoc(out, "seqloc_asn1_text"));
    writer->Print(m_BioseqHandleWithGi, m_Masks, kParseSeqids);
    BOOST_CHECK(out.match_pattern());
}

BOOST_AUTO_TEST_CASE(WriteSeqLocAsn1TextNoParseSeqids)
{
    boost::test_tools::output_test_stream out("data/sample_seqloc_asn1_text.out");
    const bool kParseSeqids(false);
    unique_ptr<CMaskWriter> writer(new CMaskWriterSeqLoc(out, "seqloc_asn1_text"));
    writer->Print(m_BioseqHandleWithLocalId, m_Masks, kParseSeqids);
    BOOST_CHECK(out.match_pattern());
    BOOST_REQUIRE_EQUAL(GetExpectedNumberOfMasks(), m_Masks.size());
    BOOST_CHECK_EQUAL(GetMaskStart(), m_Masks[0].first);
    BOOST_CHECK_EQUAL(GetMaskStop(), m_Masks[0].second);

    //writer.reset(new CMaskWriterSeqLoc(cout, "seqloc_asn1_text"));
    //writer->Print(m_BioseqHandleWithLocalId, m_Masks, kParseSeqids);
}

BOOST_AUTO_TEST_CASE(WriteSeqLocAsn1BinaryParseSeqids)
{
    const bool kMatchOrSave(true);
    const bool kTextOrBin(false);
    boost::test_tools::output_test_stream
        out(
        CSeq_id::PreferAccessionOverGi() ?
        "data/sample_prefacc_parse_seqids_seqloc_asn1_bin.out" :
        "data/sample_parse_seqids_seqloc_asn1_bin.out",
        kMatchOrSave,
        kTextOrBin);
    const bool kParseSeqids(true);
    unique_ptr<CMaskWriter> writer(new CMaskWriterSeqLoc(out, "seqloc_asn1_bin"));
    writer->Print(m_BioseqHandleWithGi, m_Masks, kParseSeqids);
    BOOST_CHECK(out.match_pattern());
}

BOOST_AUTO_TEST_CASE(WriteSeqLocAsn1BinaryNoParseSeqids)
{   
    const bool kMatchOrSave(true);
    const bool kTextOrBin(false);
    boost::test_tools::output_test_stream
        out("data/sample_seqloc_asn1_bin.out", kMatchOrSave, kTextOrBin);
    const bool kParseSeqids(false);
    unique_ptr<CMaskWriter> writer(new CMaskWriterSeqLoc(out, "seqloc_asn1_bin"));
    writer->Print(m_BioseqHandleWithLocalId, m_Masks, kParseSeqids);
    BOOST_CHECK(out.match_pattern());
}

BOOST_AUTO_TEST_CASE(WriteSeqLocXmlParseSeqids)
{
    boost::test_tools::output_test_stream out(
        CSeq_id::PreferAccessionOverGi() ?
        "data/sample_prefacc_parse_seqids_seqloc_xml.out" :
        "data/sample_parse_seqids_seqloc_xml.out");
    const bool kParseSeqids(true);
    unique_ptr<CMaskWriter> writer(new CMaskWriterSeqLoc(out, "seqloc_xml"));
    writer->Print(m_BioseqHandleWithGi, m_Masks, kParseSeqids);
    BOOST_CHECK(out.match_pattern());
}

BOOST_AUTO_TEST_CASE(WriteSeqLocXmlNoParseSeqids)
{
    boost::test_tools::output_test_stream out("data/sample_seqloc_xml.out");
    const bool kParseSeqids(false);
    unique_ptr<CMaskWriter> writer(new CMaskWriterSeqLoc(out, "seqloc_xml"));
    writer->Print(m_BioseqHandleWithLocalId, m_Masks, kParseSeqids);
    BOOST_CHECK(out.match_pattern());

    //writer.reset(new CMaskWriterSeqLoc(cout, "seqloc_xml"));
    //writer->Print(m_BioseqHandleWithLocalId, m_Masks, kParseSeqids);
}


BOOST_AUTO_TEST_SUITE_END()
