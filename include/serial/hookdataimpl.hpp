#ifndef HOOKDATAIMPL__HPP
#define HOOKDATAIMPL__HPP

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
*   !!! PUT YOUR DESCRIPTION HERE !!!
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2000/10/03 17:22:31  vasilche
* Reduced header dependency.
* Reduced size of debug libraries on WorkShop by 3 times.
* Fixed tag allocation for parent classes.
* Fixed CObject allocation/deallocation in streams.
* Moved instantiation of several templates in separate source file.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/weakmap.hpp>

BEGIN_NCBI_SCOPE

template<class Object>
class CHookDataData
{
public:
    typedef Object mapped_type;
    typedef CWeakMap<mapped_type> TMap;
    
    bool empty(void) const
        {
            return !m_GlobalHook && m_LocalHooks.empty();
        }

    mapped_type m_GlobalHook;
    TMap m_LocalHooks;
};

//#include <serial/hookdataimpl.inl>

END_NCBI_SCOPE

#endif  /* HOOKDATAIMPL__HPP */
