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
* Author:  Ilya Dondoshansky
*
* File Description:
*   Function to throw a CBlastException given a code and message
*
*/

#include <algo/blast/api/blast_exception.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)
void ThrowBlastException(CBlastException::EErrCode code, const char* message)
{
    switch ( code ) {
    case CBlastException::eInternal:
        NCBI_THROW(CBlastException, eInternal, message);
    case CBlastException::eBadParameter:
        NCBI_THROW(CBlastException, eBadParameter, message);
    case CBlastException::eOutOfMemory:
        NCBI_THROW(CBlastException, eOutOfMemory, message);
    case CBlastException::eMemoryLimit:
        NCBI_THROW(CBlastException, eMemoryLimit, message);
    case CBlastException::eInvalidCharacter:
        NCBI_THROW(CBlastException, eInvalidCharacter, message);
    case CBlastException::eNotSupported:
        NCBI_THROW(CBlastException, eNotSupported, message);
    default:
        NCBI_THROW(CBlastException, eInternal, message);
    }
}

END_SCOPE(blast)
END_NCBI_SCOPE
