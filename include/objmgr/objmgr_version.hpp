#ifndef OBJMGR___OBJMGR_VERSION__HPP
#define OBJMGR___OBJMGR_VERSION__HPP

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
 * Author:   Denis Vakatov
 *
 * File Description:
 *   ObjMgr API version.
 *
 */


/* Now, just to give an extra hint to the developers, we are encoding the date
 * of the API change in the "development" NCBI C++ Toolkit CVS tree in the
 * macro value, in format YYYYMMDD.
 *
 * NOTE:
 *   1) This can be a non-exact date.
 *   2) Moreover, one day in the future, it can become not a date at all.
 *      The only thing guaranteed is that this integer number can only get
 *      bigger with time.
 */

#define NCBI_OBJMGR_VERSION 20040607



/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2004/06/07 20:53:14  vakatov
 * Initial revision
 *
 * ===========================================================================
 */

#endif  /* OBJMGR___OBJMGR_VERSION__HPP */
