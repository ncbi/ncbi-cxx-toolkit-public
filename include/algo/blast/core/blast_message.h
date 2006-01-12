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
 * Author:  Tom Madden
 *
 */

/** @file blast_message.h
 * Structures for BLAST messages
 */

#ifndef __BLASTMESSAGES__
#define __BLASTMESSAGES__

#include <algo/blast/core/blast_def.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Structure to enclose the origin of an error message or warning
 */
typedef struct SMessageOrigin {
    char* filename;     /**< Name of the file */
    int lineno;         /**< Line number in the file above */
} SMessageOrigin;

/** Blast error message severities .
 * These start with 1 to be consistent
 * with the C toolkit severity numbers.
 */
typedef enum {
   eBlastSevInfo = 1,
   eBlastSevWarning,
   eBlastSevError,
   eBlastSevFatal
} EBlastSeverity;

/** Structure to hold the a message from the BLAST code. */
typedef struct Blast_Message {
	EBlastSeverity severity; /**< severity code */
	Int4 code;		/**< major code for error (currently unused) */
	Int4 subcode;	/**< minor code for this error (currently unused). */
	char* message;	/**< User message to be saved. */
    SMessageOrigin* origin; /**< Optional: origin of the message */
} Blast_Message;

/** Deallocates message memory.
 * @param blast_msg structure to be deallocated [in]
*/

Blast_Message* Blast_MessageFree(Blast_Message* blast_msg);


/** Writes a message to a structure.  The Blast_Message* is allocated.
 * @param blast_msg structure to be filled in [in] 
 * @param severity severity code [in] 
 * @param code major code for this error [in]
 * @param subcode minor code for this error [in] 
 * @param message User message to be saved [in]
*/

Int2 Blast_MessageWrite(Blast_Message* *blast_msg, EBlastSeverity severity, 
                        Int4 code, Int4 subcode, const char *message);


/** Print a message with ErrPostEx
 * @param blast_msg message to be printed [in]
*/

Int2 Blast_MessagePost(Blast_Message* blast_msg);

/* FIXME: should the code below and its implementation be moved to another
 * file, say blast_error.[hc]? */

/** Analogous to perror
 * @param error_code error code returned from BLAST function [in]
 * @return Blast_Message structure containing error description
 */
Blast_Message* Blast_Perror(Int2 error_code);

/** Convenient define to call the function Blast_PerrorEx. */
#define Blast_PerrorWithLocation(error_code) \
Blast_PerrorEx(error_code, __FILE__, __LINE__)

/** Extended version of Blast_Perror which includes parameters for the file
 * name and line number where the error/warning occurred. This function should
 * be invoked via the Blast_PerrorWithLocation macro.
 * @param error_code one of the error codes defined below [in]
 * @param file_name name of the file where the error ocurred [in]
 * @param lineno line number where the error ocurred in the file above [in]
 */
Blast_Message* Blast_PerrorEx(Int2 error_code, 
                              const char* file_name, 
                              int lineno);

/* BLAST error codes: these are meant to describe errors that can occur in the
 * core of BLAST only 
 */

/** System error: out of memory condition */
#define BLASTERR_MEMORY                             50

/** Invalid parameter: possible programmer error or pre-condition not met */
#define BLASTERR_INVALIDPARAM                       75

/** Could not compute the ideal Karlin-Altschul parameters */
#define BLASTERR_IDEALSTATPARAMCALC                 100

/** Composition based statistics/Smith-Waterman not supported for a program 
 * type */
#define BLASTERR_REDOALIGNMENTCORE_NOTSUPPORTED     101

/** All queries/contexts are determined invalid in the setup code */
#define BLASTERR_INVALIDQUERIES                     102

#ifdef __cplusplus
}
#endif
#endif /* !__BLASTMESSAGES__ */

