#ifndef _ALN_SCANNER_MULTALIGN_HPP_
#define _ALN_SCANNER_MULTALIGN_HPP_

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
 * Authors: Frank Ludwig
 *
 */
#include <corelib/ncbistd.hpp>
#include "aln_scanner.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects);

struct SAlignFileRaw;

//  ============================================================================
class CAlnScannerMultAlign:
    public CAlnScanner
    //  ============================================================================
{
public:
    CAlnScannerMultAlign() {};
    ~CAlnScannerMultAlign() {};

protected:
    void
    xImportAlignmentData(
        CSequenceInfo&,
        CLineInput&) override;

    virtual void
    xAdjustSequenceInfo(
        CSequenceInfo&) override;

    virtual void
    xVerifySingleSequenceData(
        const CSequenceInfo&,
        const TLineInfo& seqId,
        const vector<TLineInfo> seqData) override;

    void
    xGetExpectedDataSize(
        const string& line,
        int lineNumber,
        size_t& dataSize);
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif // _ALN_SCANNER_MULTALIGN_HPP_
