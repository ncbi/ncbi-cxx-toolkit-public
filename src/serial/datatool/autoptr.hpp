#ifndef AUTOPTR_HPP
#define AUTOPTR_HPP

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
*   One more AutoPtr template
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.7  1999/11/15 19:36:13  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/

template<typename T>
class AutoPtr {
public:
    AutoPtr(T* p = 0)
        : ptr(p)
        {
        }
    AutoPtr(const AutoPtr& p)
        : ptr(p.ptr)
        {
            p.ptr = 0;
        }

    ~AutoPtr()
        {
            if ( ptr )
                delete ptr;
        }

    AutoPtr& operator=(const AutoPtr& p)
        {
            if ( ptr != p.ptr ) {
                delete ptr;
                ptr = p.ptr;
                p.ptr = 0;
            }
            return *this;
        }
    AutoPtr& operator=(T* p)
        {
            if ( ptr != p ) {
                delete ptr;
                ptr = p;
            }
            return *this;
        }

    T& operator*() const
        {
            return *ptr;
        }
    T* operator->() const
        {
            return ptr;
        }
    
    T* get() const
        {
            return ptr;
        }
    T* release() const
        {
            T* ret = ptr;
            ptr = 0;
            return ret;
        }

    operator bool() const
        {
            return ptr != 0;
        }

private:
    mutable T* ptr;
};

template<typename T1, typename T2>
inline
void Assign(AutoPtr<T1>& t1, const AutoPtr<T2>& t2)
{
    t1 = t2.release();
}

#define iterate(Type, Var, Cont) \
    for ( Type::const_iterator Var = Cont.begin(); Var != Cont.end(); ++Var )
#define non_const_iterate(Type, Var, Cont) \
    for ( Type::iterator Var = Cont.begin(); Var != Cont.end(); ++Var )

#endif
