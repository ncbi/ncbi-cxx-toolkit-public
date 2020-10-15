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
 * Author:  Aaron Ucko, NCBI
 *
 * File Description:
 *   Unit test to confirm C-Toolkit-compatible formatting and parsing of
 *   Seq-locs and the like.
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>

#include <connect/ncbi_core_cxx.hpp>
#include <serial/objectinfo.hpp>
#include <serial/objistrasn.hpp>
#include <objects/seqloc/Seq_id.hpp>

#include <sequtil.h>

#include <corelib/test_boost.hpp>
#include <boost/test/parameterized_test.hpp>
#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;
USING_SCOPE(objects);

#if defined(NCBI_COMPILER_MSVC)  &&  defined(NCBI_DLL_BUILD)
NCBITEST_AUTO_INIT()
{
    NcbiTestSetGlobalDisabled();
}

BOOST_AUTO_TEST_CASE(s_TestIdFormatting)
{
}
#else
static AsnTypePtr s_SeqIdATP;

NCBITEST_AUTO_INIT()
{
    CONNECT_Init();
    SeqLocAsnLoad();
    s_SeqIdATP = AsnFind(const_cast<char*>("Seq-id")); // avoid warnings
}

static const char* const kRepresentativeIDs[] = {
    "local id 123",
    "local str \"foo|\"\"bar\"\"\"",
    "gibbsq 123",
    "gibbmt 123",
    "giim { id 123, db \"foo\", release \"bar\" }",
    // NB: release and version not used together below due to a minor
    // discrepancy whose resolution remains to be determined.
    "genbank { name \"AMU12345\", accession \"U12345\", release \"foo\" }",
    "embl { name \"MTBH37RV\", accession \"AL123456\", version 2 }",
    "pir { name \"S16356\" }",
    "swissprot { name \"RS22_SALTY\", accession \"Q7CQJ0\","
    " release \"reviewed\" }",
    "swissprot { name \"Q9ORT2_9HIV1\", accession \"Q90RT2\","
    " release \"unreviewed\" }",
    "swissprot { accession \"Q7CQJ0\", release \"reviewed\", version 1 }",
    "patent { seqid 1, cit { country \"US\", id number \"RE33188\" } }",
    // "patent { seqid 7, cit { country \"EP\", id app-number \"0238993\" } }",
    "other { accession \"NM_000170\", version 1 }",
    "general { db \"EcoSeq\", tag str \"EcoAce\" }",
    "general { db \"taxon\", tag id 9606 }",
    "general { db \"dbSNP\", tag str \"rs31251_allelePos=201totallen=401"
    "|taxid=9606|snpClass=1|alleles=?|mol=?|build=?\" }",
    "gi 1234",
    "ddbj { accession \"N00068\" }",
    "prf { accession \"0806162C\" }",
    "pdb { mol \"1GAV\" }",
    "pdb { mol \"1GAV\", chain 0 }",
    "pdb { mol \"1GAV\", chain 33 }",  // !
    "pdb { mol \"1GAV\", chain 88 }",  // X
    // "pdb { mol \"1GAV\", chain 120 }", // x
    "pdb { mol \"1GAV\", chain 124 }", // |
    "tpg { accession \"BK003456\" }",
    "tpe { accession \"BN000123\" }",
    "tpd { accession \"FAA00017\" }",
    "gpipe { accession \"GPC_123456789\", version 1 }",
    "named-annot-track { accession \"AT_123456789\", version 1 }"
};
static const size_t kNumRepresentativeIDs
= sizeof(kRepresentativeIDs)/sizeof(*kRepresentativeIDs);

struct SSeqIdDeleter
{
    static void Delete(SeqIdPtr sip) { SeqIdFree(sip); }
};
typedef AutoPtr<SeqId, SSeqIdDeleter> TAutoSeqId;

enum EIDLabelType {
    eFastaShort  = PRINTID_FASTA_SHORT,
    eTextAccVer  = PRINTID_TEXTID_ACC_VER,
    eTextAccOnly = PRINTID_TEXTID_ACC_ONLY,
    eReport      = PRINTID_REPORT
};

static string s_IdLabel(const TAutoSeqId& c_id, EIDLabelType type)
{
    CharPtr c_label = SeqIdWholeLabel(c_id.get(), type);
    string result = NStr::TruncateSpaces_Unsafe(c_label, NStr::eTrunc_End);
    Nlm_MemFree(c_label);
    return result;
}

static string s_IdLabel(const CSeq_id& cxx_id, EIDLabelType type)
{
    string               label;
    CSeq_id::ELabelType  gl_type = CSeq_id::eContent;
    CSeq_id::TLabelFlags flags = (CSeq_id::fLabel_Version |
                                  CSeq_id::fLabel_GeneralDbIsContent);

    switch (type) {
    case eFastaShort:   gl_type  =  CSeq_id::eFasta;                     break;
    case eTextAccOnly:  flags   &= ~CSeq_id::fLabel_Version;             break;
    case eReport:       flags   &= ~CSeq_id::fLabel_GeneralDbIsContent;  break;
    default:            break;
    }
    cxx_id.GetLabel(&label, gl_type, flags);
    NStr::TruncateSpacesInPlace(label, NStr::eTrunc_End);
    return label;
}

static void s_TestIdFormatting(const char* s)
{
    size_t            len = strlen(s);
    CObjectIStreamAsn ois(s, len);
    CSeq_id           cxx_id;
    AsnIoMemPtr       aimp = AsnIoMemOpen
        (const_cast<char*>("r"),
         reinterpret_cast<BytePtr>(const_cast<char*>(s)), len);
    AsnIoPtr          aip = AsnIoNew((ASNIO_IN | ASNIO_TEXT), NULL, aimp,
                                     AsnIoMemRead, AsnIoMemWrite);
    TAutoSeqId        c_id;

    ois.Read(&cxx_id, CSeq_id::GetTypeInfo(), CObjectIStream::eNoFileHeader);
    aip->read_id = TRUE;
    c_id = SeqIdAsnRead(aip, s_SeqIdATP);

    BOOST_CHECK_EQUAL(s_IdLabel(c_id,   eFastaShort),
                      s_IdLabel(cxx_id, eFastaShort));
    BOOST_CHECK_EQUAL(s_IdLabel(c_id,   eTextAccVer),
                      s_IdLabel(cxx_id, eTextAccVer));
    BOOST_CHECK_EQUAL(s_IdLabel(c_id,   eTextAccOnly),
                      s_IdLabel(cxx_id, eTextAccOnly));
    BOOST_CHECK_EQUAL(s_IdLabel(c_id, eReport), s_IdLabel(cxx_id, eReport));

    AsnIoClose(aip);
    AsnIoMemClose(aimp);
}

BOOST_AUTO_PARAM_TEST_CASE(s_TestIdFormatting, kRepresentativeIDs + 0,
                           kRepresentativeIDs + kNumRepresentativeIDs);
#endif
