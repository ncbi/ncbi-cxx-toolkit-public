#ifndef NST_ERROR_WARNING__HPP
#define NST_ERROR_WARNING__HPP
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
 * File Description: Error and warning codes
 */



BEGIN_NCBI_SCOPE


enum ENSTReplyError {
    eMessageAfterBye        = 1,
    eInvalidMandatoryHeader = 2,
    eShuttingDown           = 3,
    eMandatoryFieldsMissed  = 4,
    eInvalidArguments       = 5,
    eHelloRequired          = 6,
    ePrivileges             = 7,
    eWriteError             = 8,
    eReadError              = 9
};


enum ENSTReplyWarning {
};


END_NCBI_SCOPE

#endif /* NST_ERROR_WARNING__HPP */

