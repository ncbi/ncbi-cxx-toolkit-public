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
* Author:  Justin Foley
*
* File Description:
*   Unit tests for code processing secondary accessions
*
*/

#include <ncbi_pch.hpp>

// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>
#include <corelib/ncbifile.hpp>
#include <objtools/flatfile/flatfile_parser.hpp>
#include <objtools/flatfile/fta_parse_buf.h>
#include <common/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;
USING_SCOPE(objects);


BOOST_AUTO_TEST_CASE(TestIstreamInterface)
{
    string test_data_dir {"test_data"};

    unique_ptr<Parser> pConfig(new Parser()); 
    pConfig->mode = Parser::EMode::Relaxed;
    pConfig->output_format = Parser::EOutput::Seqsubmit;
    string format {"genbank"};
    string source {"ncbi"};
    fta_set_format_source(*pConfig, format, source);

    string filestub = CDir::ConcatPath(test_data_dir, "TP53_MH011443");
    string inputFile = filestub + ".gb";
    BOOST_REQUIRE(CDirEntry(inputFile).Exists());

    auto pIstr = make_unique<CNcbiIfstream>(inputFile);
    CFlatFileParser ffparser(nullptr);
    auto pResult = ffparser.Parse(*pConfig, *pIstr);
    BOOST_REQUIRE(pResult.NotNull());

    const string& outputName = CDirEntry::GetTmpName();
    CNcbiOfstream ofstr(outputName);
    ofstr << MSerial_AsnText << *pResult;
    ofstr.close();

    CFile goldenFile(filestub + ".asn");
    BOOST_REQUIRE(goldenFile.Exists());

    bool success = goldenFile.CompareTextContents(outputName, CFile::eIgnoreWs);

    if (!success) {
        const auto& args = CNcbiApplication::Instance()->GetArgs();
        if (args["keep-diffs"]) {
            CDirEntry outputFile(outputName);
            outputFile.Copy(filestub + ".new");
        }
    }

    BOOST_REQUIRE(success);
}


NCBITEST_INIT_CMDLINE(argDescrs) 
{
    argDescrs->AddFlag("keep-diffs",
            "Keep output files that are different from expected.",
            true);
}

