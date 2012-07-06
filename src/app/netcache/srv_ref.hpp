#ifndef NETCACHE__SRV_REF__HPP
#define NETCACHE__SRV_REF__HPP
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
 * Authors:  Pavel Ivanov
 *
 * File Description:
 */


BEGIN_NCBI_SCOPE


/// Special variant of CRef that doesn't check for NULL when dereferencing.
/// Segmentation fault in such case is much better than thrown exception.
template <class C, class Locker = typename CLockerTraits<C>::TLockerType>
class CSrvRef : public CRef<C, Locker>
{
    typedef CSrvRef<C, Locker> TThisType;
    typedef CRef<C, Locker> TParent;
    typedef typename TParent::TObjectType TObjectType;
    typedef typename TParent::locker_type locker_type;

public:
    CSrvRef(void)
    {}
    CSrvRef(ENull /*null*/)
    {}
    explicit
    CSrvRef(TObjectType* ptr)
        : TParent(ptr)
    {}
    CSrvRef(TObjectType* ptr, const locker_type& locker_value)
        : TParent(ptr, locker_value)
    {}
    CSrvRef(const TThisType& ref)
        : TParent(ref)
    {}

    TThisType& operator= (const TThisType& ref)
    {
        TParent::operator= (ref);
        return *this;
    }
    TThisType& operator= (TObjectType* ptr)
    {
        TParent::operator= (ptr);
        return *this;
    }
    TThisType& operator= (ENull /*null*/)
    {
        TParent::operator= (null);
        return *this;
    }

    TObjectType& operator* (void)
    {
        return *TParent::GetPointerOrNull();
    }
    TObjectType* operator-> (void)
    {
        return TParent::GetPointerOrNull();
    }
    const TObjectType& operator* (void) const
    {
        return *TParent::GetPointerOrNull();
    }
    const TObjectType* operator-> (void) const
    {
        return TParent::GetPointerOrNull();
    }
};


template<class C>
inline
CSrvRef<C> SrvRef(C* object)
{
    return CSrvRef<C>(object);
}


END_NCBI_SCOPE

#endif /* NETCACHE__SRV_REF__HPP */
