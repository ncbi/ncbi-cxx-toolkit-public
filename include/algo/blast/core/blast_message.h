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

/** Blast error message severities */
typedef enum {
   BLAST_SEV_INFO = 0,
   BLAST_SEV_WARNING,
   BLAST_SEV_ERROR,
   BLAST_SEV_FATAL
} BlastSeverity;

/** Structure to hold the a message from the BLAST code. */
typedef struct Blast_Message {
	BlastSeverity severity; /**< severity code (0, 1, 2, 3) */
	Int4 code;		/**< major code for error. */
	Int4 subcode;	/**< minor code for this error. */
	char* message;	/**< User message to be saved. */
} Blast_Message;

/** Deallocates message memory.
 * @param blast_msg structure to be deallocated [in]
*/

Blast_Message* Blast_MessageFree(Blast_Message* blast_msg);


/** Writes a message to a structure.  The Blast_Message* is allocated.
 * @param blast_msg structure to be filled in [in] 
 * @param severity severity code (0, 1, 2, 3) [in] 
 * @param code major code for this error [in]
 * @param subcode minor code for this error [in] 
 * @param message User message to be saved [in]
*/

Int2 Blast_MessageWrite(Blast_Message* *blast_msg, BlastSeverity severity, 
                        Int4 code, Int4 subcode, const char *message);


/** Print a message with ErrPostEx
 * @param blast_msg message to be printed [in]
*/

Int2 Blast_MessagePost(Blast_Message* blast_msg);


#ifdef __cplusplus
}
#endif
#endif /* !__BLASTMESSAGES__ */

