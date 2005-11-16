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
 */

/** @file blast_message.c
 * These functions provide access to Blast_Message objects, used by
 * the BLAST code as a wrapper for error and warning messages.
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <algo/blast/core/blast_message.h>

Blast_Message* 
Blast_MessageFree(Blast_Message* blast_msg)
{
	if (blast_msg == NULL)
		return NULL;

	sfree(blast_msg->message);

	sfree(blast_msg);
	return NULL;
}

Int2 
Blast_MessageWrite(Blast_Message* *blast_msg, EBlastSeverity severity, 
                   Int4 code,	Int4 subcode, const char *message)
{
	if (blast_msg == NULL)
		return 1;

	*blast_msg = (Blast_Message*) malloc(sizeof(Blast_Message));

	(*blast_msg)->severity = severity;
	(*blast_msg)->code = code;
	(*blast_msg)->subcode = subcode;
	(*blast_msg)->message = strdup(message);

	return 0;
}

Int2 
Blast_MessagePost(Blast_Message* blast_msg)
{
	if (blast_msg == NULL)
		return 1;

	fprintf(stderr, "%s", blast_msg->message);	/* FIXME! */

	return 0;
}

Blast_Message*
Blast_Perror(Int2 error_code)
{
    Blast_Message* retval = (Blast_Message*) calloc(1, sizeof(Blast_Message));

    switch (error_code) {
    case BLASTERR_IDEALSTATPARAMCALC:
        retval->message = strdup("Failed to calculate ideal Karlin-Altschul "
                                 "parameters");
        retval->severity = eBlastSevError;
        break;
    case BLASTERR_REDOALIGNMENTCORE_NOTSUPPORTED:
        retval->message = strdup("Composition based statistics or "
                                 "Smith-Waterman not supported for your "
                                 "program type");
        retval->severity = eBlastSevError;
        break;
    case 0:
        retval = Blast_MessageFree(retval);
        break;
    default:
        {
            char buf[512];
            snprintf(buf, sizeof(buf) - 1, "Unknown error code %d", error_code);
            retval->message = strdup(buf);
            retval->severity = eBlastSevError;
        }
        break;
    }

    return retval;
}

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.19  2005/11/16 14:27:03  madden
 * Fix spelling in CRN
 *
 * Revision 1.18  2005/06/20 13:09:36  madden
 * Rename BlastSeverity enums in line with C++ tookit convention
 *
 * Revision 1.17  2005/02/07 15:18:39  bealer
 * - Fix doxygen file-level comments.
 *
 * Revision 1.16  2004/11/26 20:28:38  camacho
 * + BLASTERR_REDOALIGNMENTCORE_NOTSUPPORTED
 *
 * Revision 1.15  2004/11/23 21:48:10  camacho
 * Added default handler for undefined error codes in Blast_Perror.
 *
 * Revision 1.14  2004/11/19 00:07:47  camacho
 * + Blast_Perror
 *
 * Revision 1.13  2004/11/02 17:56:48  camacho
 * Add DOXYGEN_SKIP_PROCESSING to guard rcsid string
 *
 * Revision 1.12  2004/05/19 14:52:02  camacho
 * 1. Added doxygen tags to enable doxygen processing of algo/blast/core
 * 2. Standardized copyright, CVS $Id string, $Log and rcsid formatting and i
 *    location
 * 3. Added use of @todo doxygen keyword
 *
 * Revision 1.11  2004/02/19 21:16:00  dondosha
 * Use enum type for severity argument in Blast_MessageWrite
 *
 * Revision 1.10  2003/08/11 15:01:59  dondosha
 * Added algo/blast/core to all #included headers
 *
 * Revision 1.9  2003/07/31 14:31:41  camacho
 * Replaced Char for char
 *
 * Revision 1.8  2003/07/31 00:32:37  camacho
 * Eliminated Ptr notation
 *
 * Revision 1.7  2003/07/30 16:32:02  madden
 * Use ansi functions when possible
 *
 * Revision 1.6  2003/07/29 14:42:31  coulouri
 * use strdup() instead of StringSave()
 *
 * Revision 1.5  2003/07/25 21:12:28  coulouri
 * remove constructions of the form "return sfree();" and "a=sfree(a);"
 *
 * Revision 1.4  2003/07/25 19:11:16  camacho
 * Change VoidPtr to const void* in compare functions
 *
 * Revision 1.3  2003/07/25 17:25:43  coulouri
 * in progres:
 *  * use malloc/calloc/realloc instead of Malloc/Calloc/Realloc
 *  * add sfree() macro and __sfree() helper function to util.[ch]
 *  * use sfree() instead of MemFree()
 *
 * Revision 1.2  2003/05/15 22:01:22  coulouri
 * add rcsid string to sources
 *
 * Revision 1.1  2003/03/31 18:22:30  camacho
 * Moved from parent directory
 *
 * Revision 1.2  2003/03/04 14:09:14  madden
 * Fix prototype problem
 *
 * Revision 1.1  2003/02/13 21:38:54  madden
 * Files for messaging warnings etc.
 *
 * ===========================================================================
 */
