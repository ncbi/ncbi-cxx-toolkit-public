#ifndef DBAPI_DRIVER_UTIL___NUMERIC_CONVERT__HPP
#define DBAPI_DRIVER_UTIL___NUMERIC_CONVERT__HPP

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
 * File Description:  Numeric conversions
 *
 */


#include <corelib/ncbistd.hpp>


BEGIN_NCBI_SCOPE

unsigned char*  longlong_to_numeric (Int8 l_num, unsigned int prec,
				     unsigned char* cs_num);

// long long numeric_to_longlong(CS_NUMERIC* cs_num);
Int8 numeric_to_longlong(unsigned int precision, unsigned char* cs_num);


END_NCBI_SCOPE

#endif  /* DBAPI_DRIVER_UTIL___NUMERIC_CONVERT__HPP */



/*
 * ===========================================================================
 * $Log$
 * Revision 1.4  2001/12/28 21:22:39  sapojnik
 * Made compatible with MS compiler: long long to Int8, static const within class def to enum
 *
 * Revision 1.3  2001/11/06 17:58:07  lavr
 * Formatted uniformly as the rest of the library
 *
 * Revision 1.2  2001/10/11 16:32:26  soussov
 * exclude ctlib dependencies from numeric conversions calls
 *
 * Revision 1.1  2001/09/21 23:39:54  vakatov
 * -----  Initial (draft) revision.  -----
 * This is a major revamp (by Denis Vakatov, with help from Vladimir Soussov)
 * of the DBAPI "driver" libs originally written by Vladimir Soussov.
 * The revamp involved massive code shuffling and grooming, numerous local
 * API redesigns, adding comments and incorporating DBAPI to the C++ Toolkit.
 *
 * ===========================================================================
 */
