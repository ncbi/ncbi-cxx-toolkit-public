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
 * Author:  Christiam Camacho
 *
 * File Description:
 *   Unit tests for the sequence writting API for BLAST database apps
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbifile.hpp>


#include <corelib/ncbistl.hpp>
#include <serial/serial.hpp>    
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>

#include <objects/seqalign/Seq_align_set.hpp>
#include <corelib/ncbifile.hpp> // for CTmpFile
#include "../blast_hitmatrix.hpp"

#include <corelib/test_boost.hpp>

#ifndef SKIP_DOXYGEN_PROCESSING

USING_NCBI_SCOPE;

// font files must be present in kFontPath + "/fonts"
const string kFontPath = "data";

BOOST_AUTO_TEST_SUITE(hitmatrix)
BOOST_AUTO_TEST_CASE(TestHitMatrixWriteToFileAcc)
{
    string seqAlignFileName_in = "data/in_HitMatrixSeqalign.txt";
        
    CRef<CSeq_align_set> fileSeqAlignSet(new CSeq_align_set);        
    {
        ifstream in(seqAlignFileName_in.c_str());
        in >> MSerial_AsnText >> *fileSeqAlignSet;
    }
    

    unique_ptr<CBlastHitMatrix> blastHitMatrix
        (new CBlastHitMatrix(fileSeqAlignSet->Get(),
                             600, 800, CImageIO::ePng, kFontPath));
    CTmpFile tmpfile;
    blastHitMatrix->SetFileName(tmpfile.GetFileName());
    blastHitMatrix->WriteToFile();

    CFile outFile(tmpfile.GetFileName());
    BOOST_REQUIRE_EQUAL(true, outFile.Exists() && outFile.GetLength() > 8000);    
}

BOOST_AUTO_TEST_CASE(TestHitMatrixWriteToFileFasta)
{
    string seqAlignFileName_in = "data/in_HitMatrixSeqalignFSTA.txt";
        
    CRef<CSeq_align_set> fileSeqAlignSet(new CSeq_align_set);        
    {
        ifstream in(seqAlignFileName_in.c_str());
        in >> MSerial_AsnText >> *fileSeqAlignSet;
    }
    

    unique_ptr<CBlastHitMatrix> blastHitMatrix
        (new CBlastHitMatrix(fileSeqAlignSet->Get(), 600, 800, CImageIO::ePng,
                             kFontPath));
    CTmpFile tmpfile;
    blastHitMatrix->SetFileName(tmpfile.GetFileName());
    blastHitMatrix->WriteToFile();

    CFile outFile(tmpfile.GetFileName());
    BOOST_REQUIRE_EQUAL(true, outFile.Exists() && outFile.GetLength() > 8000);    
}
BOOST_AUTO_TEST_SUITE_END()

#endif /* SKIP_DOXYGEN_PROCESSING */

