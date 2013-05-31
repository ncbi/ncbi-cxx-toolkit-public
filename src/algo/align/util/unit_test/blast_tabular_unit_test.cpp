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
* Author:  Vyacheslav Chetvernin
*
* File Description:
*   Unit tests for CBlastTabular
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>
#include <corelib/ncbifile.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <algo/align/util/blast_tabular.hpp>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;
USING_SCOPE(objects);


NCBITEST_INIT_CMDLINE(arg_desc)
{
    // Here we make descriptions of command line parameters that we are
    // going to use.

    arg_desc->AddKey("expected-results", "ResultsFile",
                     "File containing expected results",
                     CArgDescriptions::eInputFile);
    arg_desc->AddKey("input-dir", "InputDirectory",
                     "Directory containint input alignment sets",
                     CArgDescriptions::eString);
}


BOOST_AUTO_TEST_CASE(Test_CBlastTabular_constructor)
{
    const CArgs& args = CNcbiApplication::Instance()->GetArgs();

    CNcbiIstream& istr = args["expected-results"].AsInputFile();
    string line;
    while (NcbiGetlineEOL(istr, line)) {
        string input_name = line;

        cerr << "input=" << input_name
            << endl;

        list< CRef<CSeq_align> > alignments;
        CNcbiIfstream align_istr(
            CDirEntry::MakePath(args["input-dir"].AsString(),
                                input_name, "asn").c_str());
        for (;;) {
            try {
                CRef<CSeq_align> alignment(new CSeq_align);
                align_istr >> MSerial_AsnText >> *alignment;
                alignments.push_back(alignment);
            }
            catch (CEofException&) {
                break;
            }
         }

        size_t actual_hit_length = 0;

        ITERATE(list< CRef<CSeq_align> >, align, alignments) {
            try {
                CRef<CBlastTabular> hitref
                    (new CBlastTabular(**align));
                actual_hit_length += hitref->GetLength();
            } catch (CException e) {
                ERR_POST("error converting alignment: " << e);
                CNcbiOstrstream o;
                o << MSerial_AsnText << **align;
                ERR_POST("source alignment: "
                         << (string)(CNcbiOstrstreamToString(o)));
                BOOST_CHECK_NO_THROW(throw);
            }
        }
        NcbiGetlineEOL(istr, line);
        size_t blank = line.find(' ');
        size_t expected_hit_length = NStr::StringToUInt(line.substr(blank+1));
        BOOST_CHECK_EQUAL(expected_hit_length, actual_hit_length);
    }
}
