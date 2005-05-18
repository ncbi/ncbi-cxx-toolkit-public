#ifndef CORELIB___NCBICFG__H
#define CORELIB___NCBICFG__H

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
 * Author:  Aaron Ucko
 *
 *
 */

/**
 * @file ncbicfg.h
 *
 * Defines access to miscellaneous global configuration settings.
 *
 */

#include <ncbiconf.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Get default run path. */
const char* NCBI_GetDefaultRunpath(void);

/** Get run path. */
const char* NCBI_GetRunpath(void);

/** Set run path. */
void        NCBI_SetRunpath(const char* runpath);

#ifdef __cplusplus
}
#endif


#endif  /* CORELIB___NCBICFG__HPP */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.4  2005/05/18 14:00:44  ivanov
 * Do not use C++ comments in the .h files
 *
 * Revision 1.3  2003/07/11 12:47:09  siyan
 * Documentation changes.
 *
 * Revision 1.2  2002/04/11 20:39:16  ivanov
 * CVS log moved to end of the file
 *
 * Revision 1.1  2002/03/11 15:30:10  ucko
 * Dropped NCBI_RUNPATH from ncbiconf.h in favor of new API in corelib/ncbicfg.h
 *
 * ===========================================================================
 */
