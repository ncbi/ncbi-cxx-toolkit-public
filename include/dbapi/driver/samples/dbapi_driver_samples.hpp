#ifndef DBAPI_DRIVER___SAMPLES__HPP
#define DBAPI_DRIVER___SAMPLES__HPP

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
 * Author:  Vladimir Soussov
 *
 * Sample procedures to illustrate how to use language and RPC commands
 *
 */


#include <dbapi/driver/interfaces.hpp>


BEGIN_NCBI_SCOPE


void SampleDBAPI_Lang  (I_DriverContext& context, const string& server_name);
void SampleDBAPI_SpWho (I_DriverContext& context, const string& server_name);
void SampleDBAPI_Blob  (I_DriverContext& context, const string& server_name);



END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2002/06/28 14:34:06  sapojnik
 * *** empty log message ***
 *
 * Revision 1.5  2002/01/14 20:26:15  sapojnik
 * new SampleDBAPI_Blob, etc.
 *
 * Revision 1.4  2001/11/06 17:58:06  lavr
 * Formatted uniformly as the rest of the library
 *
 * Revision 1.3  2001/10/25 00:18:00  vakatov
 * SampleDBAPI_XXX() to accept yet another arg -- server name
 *
 * Revision 1.2  2001/10/24 16:36:54  lavr
 * Finish log with horizontal rule
 *
 * Revision 1.1  2001/10/23 20:50:30  lavr
 * Initial revision (derived from former sample programs)
 *
 * ===========================================================================
 */

#endif  /* DBAPI_DRIVER___SAMPLES__HPP */
