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
* Author:  ludwigf@ncbi.nlm.nih.gov
*
* File Description:
*
* ===========================================================================
*/

#include <objtools/readers/reader_listener.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects);

//  ============================================================================
class CTeamCityMessageListener:
    public CReaderListener
//  ============================================================================
{
public:
    CTeamCityMessageListener(
        const string& fileName) { mOstr = ofstream(fileName); };

    ~CTeamCityMessageListener() { mOstr.close(); };

    bool PutMessage(
        const IObjtoolsMessage& message) override
    {
        const CReaderMessage* pReaderMessage =
            dynamic_cast<const CReaderMessage*>(&message);
        mLevelCounts[pReaderMessage->Severity()]++;
        if (!pReaderMessage) {
            throw;
        }
        pReaderMessage->Write(mOstr);
        if (pReaderMessage->Severity() == eDiag_Fatal) {
            throw;
        }
        return true;
    };

    size_t LevelCount(
        EDiagSev severity) const override { return mLevelCounts.at(severity); };

protected:
    ofstream mOstr;
    using LEVELCOUNT = map<EDiagSev, size_t>;
    LEVELCOUNT mLevelCounts = {
        {eDiag_Info, 0}, {eDiag_Warning, 0}, {eDiag_Error, 0}, {eDiag_Critical, 0},
        {eDiag_Fatal, 0}, {eDiag_Trace, 0}};
};

END_SCOPE(objects);
END_NCBI_SCOPE;

