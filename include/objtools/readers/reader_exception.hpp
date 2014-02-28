#ifndef OBJTOOLS_READERS___READER_EXCEPTION__HPP
#define OBJTOOLS_READERS___READER_EXCEPTION__HPP

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
* Authors:
*	Andrei Gourianov
*
* File Description:
*   Object reader library exceptions
*/

#include <corelib/ncbiexpt.hpp>


BEGIN_NCBI_SCOPE

class CObjReaderException : public CException
{
    NCBI_EXCEPTION_DEFAULT(CObjReaderException,CException);
};

class CObjReaderParseException :
    public CParseTemplException<CObjReaderException>
{
public:
    enum EErrCode { ///< Some of these are pretty specialized
        eFormat, ///< catch-all
        eEOF,
        eNoDefline,
        eNoIDs,
        eAmbiguous,
        eBadSegSet,
        eDuplicateID,
        eUnusedMods,
        eIDTooLong,
        eNoResidues
    };
    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode()) {
        case eFormat:      return "eFormat";
        case eEOF:         return "eEOF";
        case eNoDefline:   return "eNoDefline";
        case eNoIDs:       return "eNoIDs";
        case eAmbiguous:   return "eAmbiguous";
        case eBadSegSet:   return "eBadSegSet";
        case eDuplicateID: return "eDuplicateID";
        case eUnusedMods:  return "eUnusedMods";
        case eIDTooLong:   return "eIDTooLong";
        case eNoResidues:  return "eNoResidues";
        default:           return CException::GetErrCodeString();
        }
    }
    NCBI_EXCEPTION_DEFAULT2(CObjReaderParseException,
        CParseTemplException<CObjReaderException>,std::string::size_type);
};


END_NCBI_SCOPE

#endif // OBJTOOLS_READERS___READER_EXCEPTION__HPP
