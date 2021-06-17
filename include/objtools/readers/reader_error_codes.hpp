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
 * Author: Justin Foley
 *
 * File Description:
 *  Codes and subcodes used for error reporting
 *
 */

#include <serial/serialimpl.hpp>
#include <serial/enumvalues.hpp>

#ifndef _READER_ERROR_CODES_HPP_
#define _READER_ERROR_CODES_HPP_

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

enum EReaderCode
{
    eReader_Undefined,
    eReader_Mods,
    eReader_Alignment,
};

NCBI_XOBJREAD_EXPORT const NCBI_NS_NCBI::CEnumeratedTypeValues* ENUM_METHOD_NAME(EReaderCode)(void);

enum EModSubcode
{
    eModSubcode_Undefined,
    eModSubcode_Unrecognized,
    eModSubcode_InvalidValue,
    eModSubcode_Duplicate,
    eModSubcode_ConflictingValues,
    eModSubcode_Deprecated,
    eModSubcode_ProteinModOnNucseq,
    eModSubcode_Excluded,
    eModSubcode_Applied
};

NCBI_XOBJREAD_EXPORT const NCBI_NS_NCBI::CEnumeratedTypeValues* ENUM_METHOD_NAME(EModSubcode)(void);


enum EAlnSubcode
{
    eAlnSubcode_Undefined,
    eAlnSubcode_BadDataChars,
    eAlnSubcode_UnterminatedCommand,
    eAlnSubcode_UnterminatedBlock,
    eAlnSubcode_UnexpectedSeqId,
    eAlnSubcode_BadDataCount,
    eAlnSubcode_BadSequenceCount,
    eAlnSubcode_IllegalDataLine,
    eAlnSubcode_MissingDataLine,
    eAlnSubcode_IllegalSequenceId,
    eAlnSubcode_IllegalDefinitionLine,
    eAlnSubcode_InsufficientDeflineInfo,
    eAlnSubcode_UnsupportedFileFormat,
    eAlnSubcode_UnterminatedComment,
    eAlnSubcode_UnusedLine,
    eAlnSubcode_InconsistentMolType,
    eAlnSubcode_IllegalDataDescription,
    eAlnSubcode_FileDoesNotExist,
    eAlnSubcode_FileTooShort,
    eAlnSubcode_UnexpectedCommand,
    eAlnSubcode_UnexpectedCommandArgs
};

NCBI_XOBJREAD_EXPORT const NCBI_NS_NCBI::CEnumeratedTypeValues* ENUM_METHOD_NAME(EAlnSubcode)(void);

END_SCOPE(objects)
END_NCBI_SCOPE


#endif // _READER_ERROR_CODES_HPP_
