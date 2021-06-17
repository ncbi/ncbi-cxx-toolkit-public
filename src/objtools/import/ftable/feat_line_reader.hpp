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
*   Test application for the CFormatGuess component
*
* ===========================================================================
*/

#ifndef FEAT_LINE_READER__HPP
#define FEAT_LINE_READER__HPP

#include <corelib/ncbifile.hpp>
#include <util/line_reader.hpp>
#include <objtools/import/import_message_handler.hpp>

#include "feat_import_data.hpp"
#include "../annot_import_data.hpp"

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

//  ============================================================================
class CFeatLineReader
//  ============================================================================
{
public:
    CFeatLineReader(
        CImportMessageHandler& );

    virtual ~CFeatLineReader() {};

    virtual bool
    GetNextRecord(
        CStreamLineReader&,
        CFeatImportData&) =0;

    unsigned int LineCount() const;
    unsigned int RecordCount() const { return mRecordCount; };

    virtual void
    SetInputStream(
        CNcbiIstream&,
        bool =false);

    void
    SetProgressReportFrequency(
        unsigned int numLines) { mProgressFreq = numLines; };

    const CAnnotImportData&
    AnnotImportData() const { return mAnnotInfo; };

protected:
    virtual bool
    xIgnoreLine(
        const string&) const;

    virtual void
    xReportProgress();

    virtual void
    xInitializeRecord(
        const std::vector<std::string>&,
        CFeatImportData&) =0;

    //unique_ptr<CStreamLineReader> mpLineReader;
    CImportMessageHandler& mErrorReporter;

    unsigned int mLineCount;
    unsigned int mRecordCount;
    unsigned int mProgressFreq;
    unsigned int mLastProgress;
    CAnnotImportData mAnnotInfo;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif
