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
* Author: Aleksey Grichenko
*
* File Description:
*   CMutexPool_Base template -- to distribute mutex pool between
*   several objects.
*
*/


#include <corelib/ncbithr.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


////////////////////////////////////////////////////////////////////
//
//  CMutexPool_Base<>::
//
//    Distribute a mutex pool between multiple objects
//


const size_t kMutexPoolSize = 16;


template<class TProtected>
class CMutexPool_Base {
public:
    CMutexPool_Base(void) {}

    static CMutex& GetMutex(const TProtected* obj);
private:
    CMutexPool_Base(const CMutexPool_Base&);
    CMutexPool_Base& operator=(const CMutexPool_Base&);

    static CMutex sm_Pool[kMutexPoolSize];
};


template<class TProtected>
CMutex CMutexPool_Base<TProtected>::sm_Pool[kMutexPoolSize];

template<class TProtected>
CMutex& CMutexPool_Base<TProtected>::GetMutex(const TProtected* obj)
{
    return sm_Pool[((size_t)(obj) >> 4) % kMutexPoolSize];
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
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
