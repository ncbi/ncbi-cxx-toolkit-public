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
* File Description:  Handlers Stack
*
*
*/


#include <corelib/ncbistd.hpp>
#include <ctpublic.h>


BEGIN_NCBI_SCOPE


char* numeric_to_string(CS_NUMERIC* numeric, char *s);

// NOTE: The function string_to_numeric() require a pointer to
//       the  CS_NUMERIC object. If pointer is NULL , CS_NUMERIC object         
//       will be created inside this function. A deletion of the object 
//       in this case is a user's responsibility.

CS_NUMERIC* string_to_numeric(string str, int press, 
                              int scale, CS_NUMERIC* num); 

// NOTE: The function longlong_to_numeric() require a pointer to
//       the  CS_NUMERIC object. If pointer is NULL , CS_NUMERIC object         
//       will be created inside this function. A deletion of the object 
//       in this case is a user's responsibility.

CS_NUMERIC* longlong_to_numeric(long long l_num, int prec, 
                                int scale, CS_NUMERIC* cs_num); 
long long numeric_to_longlong(CS_NUMERIC* cs_num);


END_NCBI_SCOPE

#endif  /* DBAPI_DRIVER_UTIL___NUMERIC_CONVERT__HPP */



/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2001/09/21 23:39:54  vakatov
 * -----  Initial (draft) revision.  -----
 * This is a major revamp (by Denis Vakatov, with help from Vladimir Soussov)
 * of the DBAPI "driver" libs originally written by Vladimir Soussov.
 * The revamp involved massive code shuffling and grooming, numerous local
 * API redesigns, adding comments and incorporating DBAPI to the C++ Toolkit.
 *
 * ===========================================================================
 */
