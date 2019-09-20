#ifndef _ALN_FORMATS_HPP_
#define _ALN_FORMATS_HPP_

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

#include <objtools/readers/aln_reader.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects);

class CPeekAheadStream;

//  ============================================================================
class CAlnFormatGuesser
//  ============================================================================
{
    using TSample = vector<string>;

public:
    CAlnFormatGuesser() {};
    ~CAlnFormatGuesser() {};

    EAlignFormat
    GetFormat(
        CPeekAheadStream& iStr);

protected:
    void
    xInitSample(
        CPeekAheadStream& iStr,
        TSample& sample);

    bool
    xSampleIsNexus(
        const TSample&);

    bool
    xSampleIsClustal(
        TSample&,
        CPeekAheadStream& iStr);

    bool
    xSampleIsFastaGap(
        const TSample&);

    bool
    xSampleIsPhylip(
        const TSample&);

    bool
    xSampleIsSequin(
        const TSample&);

    bool
    xSampleIsMultAlign(
        const TSample&);

    static const int SAMPLE_SIZE = 10;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif // _ALN_FORMATS_HPP_
