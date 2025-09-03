#ifndef _ALN_SCANNER_CLUSTAL_HPP_
#define _ALN_SCANNER_CLUSTAL_HPP_

/*
 * $Id$
 *
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
 * Authors:  Colleen Bollin
 *
 */
#include <corelib/ncbistd.hpp>
#include "aln_scanner.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects);


//  ============================================================================
class CAlnScannerClustal : public CAlnScanner
//  ============================================================================
{

private:
    void
    xImportAlignmentData(
        CLineInput&) override;

    static bool
    sIsConservationLine(
        const string& line);

    void
    xProcessClustalDataLine(
        const vector<string>& tokens,
        int                   lineNum,
        int                   seqCount,
        int                   numSeqs,
        bool                  firstBlock,
        size_t&               blockLineLength);
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif // _ALN_SCANNER_CLUSTAL_HPP_
