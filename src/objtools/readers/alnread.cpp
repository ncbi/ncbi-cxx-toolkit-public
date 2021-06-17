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

#include <ncbi_pch.hpp>
#include <corelib/ncbistr.hpp>
#include <objtools/readers/reader_error_codes.hpp>
#include <objtools/readers/message_listener.hpp>
#include <objtools/readers/alnread.hpp>
#include <objtools/readers/reader_error_codes.hpp>
#include "aln_errors.hpp"
#include "aln_formatguess.hpp"
#include "aln_peek_ahead.hpp"
#include "aln_scanner_clustal.hpp"
#include "aln_scanner_nexus.hpp"
#include "aln_scanner_phylip.hpp"
#include "aln_scanner_sequin.hpp"
#include "aln_scanner_fastagap.hpp"
#include "aln_scanner_multalign.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects);

//  ----------------------------------------------------------------------------
CAlnScanner*
GetScannerForFormat(
    EAlignFormat format)
//  ----------------------------------------------------------------------------
{
    switch(format) {
    default:
        return new CAlnScanner();
    case EAlignFormat::PHYLIP:
        return new CAlnScannerPhylip();
    case EAlignFormat::FASTAGAP:
        return new CAlnScannerFastaGap();
    case EAlignFormat::CLUSTAL:
        return new CAlnScannerClustal();
    case EAlignFormat::SEQUIN:
        return new CAlnScannerSequin();
    case EAlignFormat::NEXUS:
        return new CAlnScannerNexus();
    case EAlignFormat::MULTALIN:
        return new CAlnScannerMultAlign();
    }
}


//  ----------------------------------------------------------------------------
bool
ReadAlignmentFile(
    istream& istr,
    bool gen_local_ids,
    bool use_nexus_info,
    CSequenceInfo& sequenceInfo,
    SAlignmentFile& alignmentInfo,
    ILineErrorListener* pErrorListener)
//  ----------------------------------------------------------------------------
{
    theErrorReporter.reset(new CAlnErrorReporter(pErrorListener));

    if (sequenceInfo.Alphabet().empty()) {
        return false;
    }

    CPeekAheadStream iStr(istr);
    EAlignFormat format = CAlnFormatGuesser().GetFormat(iStr);

    unique_ptr<CAlnScanner> pScanner(GetScannerForFormat(format));
    if (!pScanner) {
        return false;
    }

    pScanner->ProcessAlignmentFile(sequenceInfo, iStr, alignmentInfo);
    return true;
};

//  ------------------------------------------------------------------------------
bool ReadAlignmentFile(
    istream& istr,
    CSequenceInfo& sequenceInfo,
    SAlignmentFile& alignmentInfo)
//  ------------------------------------------------------------------------------
{
    EAlignFormat dummy;
    return ReadAlignmentFile(istr, dummy, sequenceInfo, alignmentInfo);
}

//  ------------------------------------------------------------------------------
bool ReadAlignmentFile(
    istream& istr,
    EAlignFormat& alignFormat,
    CSequenceInfo& sequenceInfo,
    SAlignmentFile& alignmentInfo)
//  ------------------------------------------------------------------------------
{
    if (sequenceInfo.Alphabet().empty()) {
        return false;
    }

    CPeekAheadStream iStr(istr);
    alignFormat = CAlnFormatGuesser().GetFormat(iStr);

    unique_ptr<CAlnScanner> pScanner(GetScannerForFormat(alignFormat));
    if (!pScanner) {
        return false;
    }

    pScanner->ProcessAlignmentFile(sequenceInfo, iStr, alignmentInfo);
    return true;
}

END_SCOPE(objects)
END_NCBI_SCOPE
