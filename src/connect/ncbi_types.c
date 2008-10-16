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
 * Author:  Anton Lavrentiev
 *
 * File Description:
 *   Special types' methods.
 *
 */

#include <connect/ncbi_types.h>


/******************************************************************************
 *  STimeout
 */

extern unsigned long NcbiTimeoutToMs(const STimeout* timeout)
{
    return timeout
        ? timeout->sec * 1000 + (timeout->usec + 500) / 1000
        : (unsigned long)(-1L);
}


extern STimeout* NcbiMsToTimeout(STimeout* timeout, unsigned long ms)
{
    if (ms == (unsigned long)(-1L))
        return 0;
    timeout->sec  =  ms / 1000;
    timeout->usec = (ms % 1000) * 1000;
    return timeout;
}
