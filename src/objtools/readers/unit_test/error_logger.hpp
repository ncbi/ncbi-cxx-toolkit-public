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
*   Reader for selected data file formats
*
* ===========================================================================
*/

#include <objtools/readers/message_listener.hpp>
#include <objtools/readers/line_error.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

//  ============================================================================
class CErrorLogger:
//  ============================================================================
    public CMessageListenerBase
{
public:
    CErrorLogger(
            const string& filename) {
        mStream.open(filename.c_str());
    };

    ~CErrorLogger() {
        mStream.close();
    };

    bool
    PutError(
        const ILineError& err )
    {
        StoreError(err);
        if (err.Line() == 0) {
            mStream << "Line No    : "
                    << "None (Encountered during pre or post processing)" << endl;
        }
        else {
            mStream << "Line No    : " << err.Line() << endl;
        }
        mStream << "Severity   : " << err.SeverityStr() << endl;

        if (err.GetCode()) {
            mStream << "Code       : " << err.GetCode();
            if (err.GetSubCode()) {
               mStream << "." << err.GetSubCode();
            }
            mStream << endl;
        }

        vector<string> messageLines;
        NStr::Split(err.Message(), "\n", messageLines);
        if (!messageLines.empty()) {
            mStream << "Message    : " << messageLines.front() << endl;
            for (auto i=1; i < messageLines.size(); ++i) {
                mStream << "             " << messageLines[i] << endl;
            }
        }
        mStream << endl;

        if (Count() < 500000  &&  err.Severity() != eDiag_Fatal) {
            return true;
        }
        NCBI_THROW2(CObjReaderParseException, eFormat, err.Message(), 0);
        return false;
    };

protected:
    ofstream mStream;
};

