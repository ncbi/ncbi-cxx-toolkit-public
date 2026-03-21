/* $Id$
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
 * File Name: flatparse_report.hpp
 *
 * Author: Frank Ludwig
 *
 * File Description:
 *
 */

#ifndef FLATFILE__FLATPARSE_REPORT__HPP
#define FLATFILE__FLATPARSE_REPORT__HPP

#include "ftaerr.hpp"
#include <objtools/flatfile/flat2err.h>

BEGIN_NCBI_SCOPE

//  ----------------------------------------------------------------------------
class CFlatParseReport
//  ----------------------------------------------------------------------------
{
    //  Signature convention:
    //  Return value indicates whether the reported problem leaves the qualifier
    //   usable or not.
    //   ==true: problem not serious, use qualifier
    //   ==false: problem serious, don't use qualifier
    //  Args are generally in order featKey, featLocation, qualKey, qualVal but the
    //   ones that don't apply may be missing.
    //  Any specific arguments come after that.
public:
    static void ContainsEmbeddedQualifier(
        const string& featKey,
        const string& featLocation,
        const string& qualKey,
        const string& firstEmbedded,
        bool          inNote = false);

    static void NoTextAfterEqualSign(
        const string& featKey,
        const string& featLocation,
        const string& qualKey);

    static void QualShouldHaveValue(
        const string& featKey,
        const string& featLocation,
        const string& qualKey);

    static void QualShouldNotHaveValue(
        const string& featKey,
        const string& featLocation,
        const string& qualKey);

    static void UnbalancedQuotes(
        const string& qualKey);

    static void UnexpectedData(
        const string& featKey,
        const string& featLocation);

    static void UnknownQualifierKey(
        const string& featKey,
        const string& featLocation,
        const string& qualKey);

};


END_NCBI_SCOPE

#endif // FLATFILE__FLATPARSE_REPORT__HPP
