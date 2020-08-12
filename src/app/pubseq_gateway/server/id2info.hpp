#ifndef PUBSEQ_GATEWAY_ID2INFO__HPP
#define PUBSEQ_GATEWAY_ID2INFO__HPP

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
 * Authors:  Sergey Satskiy
 *
 * File Description:
 *   PSG ID2 info wrapper
 *
 */


#include <string>
using namespace std;


class CPSGId2Info
{
    public:
        using TSat = int16_t;
        using TInfo = int32_t;
        using TChunks = int32_t;
        using TSplitVersion = int32_t;

    public:
        CPSGId2Info(const string &  id2_info,
                    bool  count_errors = true);
        CPSGId2Info() :
            m_Sat(-1), m_Info(-1), m_Chunks(-1), m_SplitVersion(-1)
        {}

    public:
        TSat GetSat(void) const
        { return m_Sat; }

        TInfo GetInfo(void) const
        { return m_Info; }

        TChunks GetChunks(void) const
        { return m_Chunks; }

        TSplitVersion GetSplitVersion(void) const
        { return m_SplitVersion; }

    private:
        TSat            m_Sat;
        TInfo           m_Info;
        TChunks         m_Chunks;
        TSplitVersion   m_SplitVersion;
};


#endif /* PUBSEQ_GATEWAY_ID2INFO__HPP */

