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
 * Author: Mike DiCuccio
 *
 * File Description:
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/test_boost.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/feat_ci.hpp>

#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seqset/Seq_entry.hpp>

#include <algo/sequence/polya.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

NCBITEST_INIT_CMDLINE(arg_desc)
{
    // Here we make descriptions of command line parameters that we are
    // going to use.

    arg_desc->AddKey("data-in", "InputData",
                     "Concatenated Seq-aligns used to generate gene models",
                     CArgDescriptions::eInputFile);
}

BOOST_AUTO_TEST_CASE(TestUsingArg)
{
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    CScope scope(*om);
    scope.AddDefaults();

    const CArgs& args = CNcbiApplication::Instance()->GetArgs();
    CNcbiIstream& istr = args["data-in"].AsInputFile();

    auto_ptr<CObjectIStream> is(CObjectIStream::Open(eSerial_AsnText, istr));

    for (int count = 1; istr; ++count) {

        CRef<CSeq_entry> entry(new CSeq_entry);

        try {
            *is >> *entry;
        }
        catch (CEofException&) {
            break;
        }

        cerr << "Sequence "<< count <<  endl;

        TSignedSeqPos site = -1;
        CSeqVector vec(entry->GetSeq(), NULL, CBioseq_Handle::eCoding_Iupac);
        EPolyTail tail = FindPolyTail(vec.begin(), vec.end(), site, 10);
        TSeqRange actual_polya(tail == ePolyTail_A3 ? site : 0,
                               tail == ePolyTail_A3 ? vec.size() - 1 : site);

        TSeqRange expected_polya;
        CSeq_entry_Handle handle = scope.AddTopLevelSeqEntry(*entry);
        for(CFeat_CI feat_it(handle, SAnnotSelector(CSeqFeatData::e_Region));
                    feat_it; ++feat_it)
            if(feat_it->GetData().GetRegion() == "polya")
                expected_polya = feat_it->GetRange();

        BOOST_CHECK_EQUAL( expected_polya, actual_polya );
    }
}


