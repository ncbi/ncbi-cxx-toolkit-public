#ifndef STL__HPP
#define STL__HPP

/*  $RCSfile$  $Revision$  $Date$
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
* Author:  Lewis Geer
*
* File Description:
*   convenience header
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  1998/10/06 20:34:32  lewisg
* html library includes
*
* ===========================================================================
*/

#include <ncbistd.hpp>


// include the standard library and sets some compiler specific macros

#ifdef __SUNPRO_CC
#include <iostream.h>
#define SIZE_TYPE size_t
#else
#define SIZE_TYPE string::size_type
#define NPOS string::npos
#include <iostream>
using namespace std;
#endif

#ifdef  _MSC_VER
#pragma warning(disable: 4786)  // gets rid of the long identifier warnings for Visual C
#endif



// don't know if some of these are nested inside of each other
// also don't know if I missed some.  please feel free to add or delete.

#include <algorithm>
#include <deque>
#include <functional>
#include <memory>
#include <numeric>
#include <iterator>
#include <list>
#include <map>
#include <queue>
#include <set>
#include <stack>
#include <string>
#include <utility>
#include <vector>

#endif
