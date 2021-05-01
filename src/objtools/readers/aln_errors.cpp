/*
 * $Id$
 *
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
 * Authors:  Colleen Bollin
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistr.hpp>
#include <objtools/readers/reader_error_codes.hpp>
#include <objtools/readers/message_listener.hpp>
#include <objtools/readers/reader_error_codes.hpp>
#include "aln_errors.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects);

thread_local unique_ptr<CAlnErrorReporter> theErrorReporter;

//  ----------------------------------------------------------------------------
BEGIN_NAMED_ENUM_INFO("", EReaderCode, false)
//  ----------------------------------------------------------------------------
{
     ADD_ENUM_VALUE("Undefined", eReader_Undefined);
     ADD_ENUM_VALUE("Mods", eReader_Mods);
     ADD_ENUM_VALUE("Alignment", eReader_Alignment);
}
END_ENUM_INFO


//  ----------------------------------------------------------------------------
BEGIN_NAMED_ENUM_INFO("", EAlnSubcode, false)
//  ----------------------------------------------------------------------------
{
     ADD_ENUM_VALUE("Undefined", eAlnSubcode_Undefined);
     ADD_ENUM_VALUE("BadDataChars", eAlnSubcode_BadDataChars);
     ADD_ENUM_VALUE("UnterminatedCommand", eAlnSubcode_UnterminatedCommand);
     ADD_ENUM_VALUE("UnterminatedBlock", eAlnSubcode_UnterminatedBlock);
     ADD_ENUM_VALUE("UnexpectedSeqId", eAlnSubcode_UnexpectedSeqId);
     ADD_ENUM_VALUE("BadDataCount", eAlnSubcode_BadDataCount);
     ADD_ENUM_VALUE("BadSequenceCount", eAlnSubcode_BadSequenceCount);
     ADD_ENUM_VALUE("IllegalDataLine", eAlnSubcode_IllegalDataLine);
     ADD_ENUM_VALUE("MissingDataLine", eAlnSubcode_MissingDataLine);
     ADD_ENUM_VALUE("IllegalSequenceId", eAlnSubcode_IllegalSequenceId);
     ADD_ENUM_VALUE("IllegalDefinitionLine", eAlnSubcode_IllegalDefinitionLine);
     ADD_ENUM_VALUE("InsufficientDeflineInfo", eAlnSubcode_InsufficientDeflineInfo);
     ADD_ENUM_VALUE("UnsupportedFileFormat", eAlnSubcode_UnsupportedFileFormat);
     ADD_ENUM_VALUE("UnterminatedComment", eAlnSubcode_UnterminatedComment);
     ADD_ENUM_VALUE("UnusedLine", eAlnSubcode_UnusedLine);
     ADD_ENUM_VALUE("InconsistentMolType", eAlnSubcode_InconsistentMolType);
     ADD_ENUM_VALUE("IllegalDataDescription", eAlnSubcode_IllegalDataDescription);
     ADD_ENUM_VALUE("FileDoesNotExist", eAlnSubcode_FileDoesNotExist);
     ADD_ENUM_VALUE("FileTooShort", eAlnSubcode_FileTooShort);
     ADD_ENUM_VALUE("UnexpectedCommand", eAlnSubcode_UnexpectedCommand);
     ADD_ENUM_VALUE("UnexpectedCommandArgs", eAlnSubcode_UnexpectedCommandArgs);
}
END_ENUM_INFO


//  ----------------------------------------------------------------------------
BEGIN_NAMED_ENUM_INFO("", EModSubcode, false)
//  ----------------------------------------------------------------------------
{
     ADD_ENUM_VALUE("Undefined", eModSubcode_Undefined);
     ADD_ENUM_VALUE("Unrecognized", eModSubcode_Unrecognized);
     ADD_ENUM_VALUE("InvalidValue", eModSubcode_InvalidValue);
     ADD_ENUM_VALUE("Duplicate", eModSubcode_Duplicate);
     ADD_ENUM_VALUE("ConflictingValues", eModSubcode_ConflictingValues);
     ADD_ENUM_VALUE("Deprecated", eModSubcode_Deprecated);
     ADD_ENUM_VALUE("ProteinModOnNucseq", eModSubcode_ProteinModOnNucseq);
}
END_ENUM_INFO

//  ----------------------------------------------------------------------------
string ErrorPrintf(const char *format, ...)
//  ----------------------------------------------------------------------------
{
    va_list args;
    va_start(args, format);
    return NStr::FormatVarargs(format, args);
}

//  ----------------------------------------------------------------------------
string BadCharCountPrintf(int expectedCount, int actualCount)
//  ----------------------------------------------------------------------------
{
    return
        ("Number of characters on sequence line is different from expected. " +
        ErrorPrintf("Expected number of characters is %d. Actual number of characters is %d.",
                expectedCount, actualCount));
}

END_SCOPE(objects)
END_NCBI_SCOPE
