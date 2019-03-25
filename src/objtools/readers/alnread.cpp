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
#include "aln_formats.hpp"
#include "aln_peek_ahead.hpp"
#include "aln_scanner_clustal.hpp"
#include "aln_scanner_nexus.hpp"
#include "aln_scanner_phylip.hpp"
#include "aln_scanner_sequin.hpp"
#include "aln_scanner_fastagap.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects);

static const size_t kMaxPrintedIntLen = 10;

//  ----------------------------------------------------------------------------
static EAlignFormat
sGetFileFormat(
    CPeekAheadStream& iStr)
//  ----------------------------------------------------------------------------
{
    string line;
    int lineCount = 0;
    int leadingBlankCount = 0;
    bool inLeadingFastaComment = true;
    while (iStr.PeekLine(line)) {
        lineCount++;
        NStr::TruncateSpacesInPlace(line);
        NStr::ToLower(line); 

        if (NStr::StartsWith(line, ";")) {
            continue;
        }
        if (NStr::StartsWith(line, ">")) {
            return EAlignFormat::FASTAGAP;
        }
        inLeadingFastaComment = false;

        if (line.empty()) {
            leadingBlankCount++;
            continue;
        }

        if (lineCount == 1) {
            if (NStr::StartsWith(line, "#nexus")) {
                return EAlignFormat::NEXUS;
            }
            if (NStr::StartsWith(line, "clustalw")) {
                return EAlignFormat::CLUSTAL;
            }
            if (NStr::StartsWith(line, "clustal w")) {
                return EAlignFormat::CLUSTAL;
            }
            vector<string> tokens;
            NStr::Split(line, " \t", tokens, NStr::fSplit_MergeDelimiters);
            if (tokens.size() != 2) {
                return EAlignFormat::UNKNOWN;
            }
            if (tokens.front().find_first_not_of("0123456789") != string::npos) {
                return EAlignFormat::UNKNOWN;
            }
            if (tokens.back().find_first_not_of("0123456789") != string::npos) {
                return EAlignFormat::UNKNOWN;
            }
            return EAlignFormat::PHYLIP;
        }
        if (lineCount == 2) {
            if (leadingBlankCount == 1) {
                vector<string> tokens;
                NStr::Split(line, " \t", tokens, NStr::fSplit_MergeDelimiters);
                for (int index=0; index < tokens.size(); ++index) {
                    auto offset = NStr::StringToInt(tokens[index], NStr::fConvErr_NoThrow);
                    if (offset != 10 + 10*index) {
                        return EAlignFormat::UNKNOWN;
                    }
                }
                return EAlignFormat::SEQUIN;
            }
        }
        return EAlignFormat::UNKNOWN;
    }
    return EAlignFormat::UNKNOWN;
}


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
    if (pErrorListener) {
        theErrorReporter.reset(new CAlnErrorReporter(pErrorListener));
    }
    if (sequenceInfo.Alphabet().empty()) {
        return false;
    }

    CPeekAheadStream iStr(istr);
    EAlignFormat format = sGetFileFormat(iStr);
    unique_ptr<CAlnScanner> pScanner(GetScannerForFormat(format));
    if (!pScanner) {
        return false;
    }
    pScanner->ProcessAlignmentFile(sequenceInfo, iStr, alignmentInfo);
    return true;
};


END_SCOPE(objects)
END_NCBI_SCOPE
