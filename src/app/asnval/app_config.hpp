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
 * Author:  Frank Ludwig
 *
 * File Description:
 *   validator
 *
 */

#ifndef ASNVAL_APP_CONFIG_HPP
#define ASNVAL_APP_CONFIG_HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbireg.hpp>

class CValidatorThreadPool;

using namespace ncbi;

class CAppConfig
{
public:
    enum EVerbosity {
        eVerbosity_Normal = 1,
        eVerbosity_Spaced = 2,
        eVerbosity_Tabbed = 3,
        eVerbosity_XML = 4,
        eVerbosity_min = 1, eVerbosity_max = 4
    };

    CAppConfig(const CArgs& args, const CNcbiRegistry& reg);

    bool mQuiet;
    EVerbosity mVerbosity;
    EDiagSev mLowCutoff;
    EDiagSev mHighCutoff;
    EDiagSev mReportLevel;
    bool mBatch = false;
    string mOnlyError;
    bool mContinue;
    bool mOnlyAnnots;
    bool mHugeFile = false;
    unsigned int m_Options = 0;
    unsigned int mNumInstances;
    CValidatorThreadPool* m_thread_pool1 = nullptr;
    CValidatorThreadPool* m_thread_pool2 = nullptr;
};

#endif
