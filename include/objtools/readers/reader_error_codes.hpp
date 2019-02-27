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


#ifndef _READER_ERROR_CODES_HPP_
#define _READER_ERROR_CODES_HPP_

enum EReaderCode
{
    eReader_Undefined,
    eReader_Mods,
    eReader_Alignment,
};


enum EModSubcode 
{
    eModSubcode_Undefined,
    eModSubcode_Unrecognized,
    eModSubcode_InvalidValue,
    eModSubcode_Duplicate,
    eModSubcode_ConflictingValues,
    eModSubcode_Deprecated,
    eModSubcode_ProteinModOnNucseq
};


enum EAlnSubcode
{
    eAlnSubcode_Undefined,
    eAlnSubcode_InvalidInput,
    eAlnSubcode_SingleSeq,
    eAlnSubcode_MissingSeqData,
    eAlnSubcode_BadCharacters,
    eAlnSubcode_UnexpectedSeqLength,
    eAlnSubcode_UnexpectedNumSeqs,
    eAlnSubcode_UnexpectedLineLength,
    eAlnSubcode_InconsistentBlockLines,
    eAlnSubcode_UnexpectedBlockLength,
    eAlnSubcode_UnexpectedID,
    eAlnSubcode_DuplicateID,
    eAlnSubcode_Asn1File,
    eAlnSubcode_ReplicatedSeq,
    eAlnSubcode_DifferingSeqLengths,
    eAlnSubcode_UnmatchedDeflines
};


#endif // _READER_ERROR_CODES_HPP_
