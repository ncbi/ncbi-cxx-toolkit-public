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
* Author:  Frank Ludwig, NCBI
*
* File Description:
*   Scanner for BED input data.
*
* ===========================================================================
*/

#ifndef BED_LINE_READER__HPP
#define BED_LINE_READER__HPP

#include <corelib/ncbifile.hpp>
#include <util/line_reader.hpp>

#include "feat_line_reader.hpp"
#include "bed_import_data.hpp"

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

//  ============================================================================
class CBedLineReader: 
    public CFeatLineReader
//  ============================================================================
{
public:
    CBedLineReader(
        CImportMessageHandler&);

    virtual ~CBedLineReader() {};

    bool
    GetNextRecord(
        CStreamLineReader&,
        CFeatImportData&) override;

    void
    SetInputStream(
        CNcbiIstream&,
        bool = false) override;

private:
    bool
    xIgnoreLine(
        const string&) const override;

    bool
    xProcessTrackLine(
        const string&);

    void
    xSplitLine(
        const std::string&,
        std::vector<std::string>&);

    void
    xInitializeRecord(
        const std::vector<std::string>&,
        CFeatImportData&) override;

    void
    xInitializeChromInterval(
        const std::vector<std::string>&,
        std::string&,
        TSeqPos&,
        TSeqPos&,
        ENa_strand&);

    void
    xInitializeChromName(
        const std::vector<std::string>&,
        std::string&);

    void
    xInitializeScore(
        const std::vector<string>&,
        double&);

    void
    xInitializeThickInterval(
        const std::vector<std::string>&,
        TSeqPos&,
        TSeqPos&);

    void
    xInitializeRgb(
        const std::vector<string>&,
        CBedImportData::RgbValue&);
    void
    xInitializeRgbFromScoreColumn(
        const std::vector<string>&,
        CBedImportData::RgbValue&);
    void
    xInitializeRgbFromRgbColumn(
        const std::vector<string>&,
        CBedImportData::RgbValue&);
    void
    xInitializeRgbFromStrandColumn(
        const std::vector<string>&,
        CBedImportData::RgbValue&);

    void
    xInitializeBlocks(
        const std::vector<string>&,
        unsigned int& blockCount,
        std::vector<int>& blockStarts,
        std::vector<int>& blockSizes);

	size_t mColumnCount;
    std::string mColumnDelimiter;
    int mSplitFlags;
    bool mUseScore;
    bool mItemRgb;
    bool mColorByStrand;
    CBedImportData::RgbValue mRgbStrandPlus;
    CBedImportData::RgbValue mRgbStrandMinus;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif
