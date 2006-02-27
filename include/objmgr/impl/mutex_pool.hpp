#ifndef OBJECTS_OBJMGR___MUTEX_POOL__HPP
#define OBJECTS_OBJMGR___MUTEX_POOL__HPP

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
* Author: Eugene Vasilchenko
*
* File Description:
*   CMutexPool -- to distribute mutex pool among several objects.
*
*/

// CMutexPool class is moved to util library

#include <util/mutex_pool.hpp>


/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.10  2006/02/27 15:42:13  vasilche
* CMutexPool moved from xobjmgr to xutil.
*
* Revision 1.9  2006/01/25 14:17:09  vasilche
* Do not export completely inlined classes.
*
* Revision 1.8  2005/04/20 15:42:40  vasilche
* Use DECLARE_OPERATOR_BOOL* macro.
*
* Revision 1.7  2005/04/20 15:07:14  ucko
* Reimplement operator bool in CInitMutex<>, as some versions of
* WorkShop ignore the version defined for CInitMutex_Base.
*
* Revision 1.6  2005/01/24 17:09:36  vasilche
* Safe boolean operators.
*
* Revision 1.5  2005/01/12 17:16:14  vasilche
* Avoid performance warning on MSVC.
*
* Revision 1.4  2004/09/13 18:31:20  vasilche
* Fixed ASSERT condition.
*
* Revision 1.3  2003/07/01 18:02:37  vasilche
* Removed invalid assert.
* Moved asserts from .hpp to .cpp file.
*
* Revision 1.2  2003/06/25 17:09:27  vasilche
* Fixed locking in CInitMutexPool.
*
* Revision 1.1  2003/06/19 18:23:45  vasilche
* Added several CXxx_ScopeInfo classes for CScope related information.
* CBioseq_Handle now uses reference to CBioseq_ScopeInfo.
* Some fine tuning of locking in CScope.
*
* Revision 1.4  2003/04/24 16:12:37  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.3  2003/03/03 18:46:45  dicuccio
* Removed unnecessary Win32 export specifier
*
* Revision 1.2  2002/12/26 20:51:35  dicuccio
* Added Win32 export specifier
*
* Revision 1.1  2002/07/08 20:35:50  grichenk
* Initial revision
*
* Revision 1.4  2002/06/04 17:18:32  kimelman
* memory cleanup :  new/delete/Cref rearrangements
*
* Revision 1.3  2002/05/06 03:30:36  vakatov
* OM/OM1 renaming
*
* Revision 1.2  2002/02/25 21:05:27  grichenk
* Removed seq-data references caching. Increased MT-safety. Fixed typos.
*
* Revision 1.1  2002/02/21 19:21:02  grichenk
* Initial revision
*
*
* ===========================================================================
*/

#endif  /* OBJECTS_OBJMGR___MUTEX_POOL__HPP */
