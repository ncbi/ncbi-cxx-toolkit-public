#if defined(PTRINFO__HPP)  &&  !defined(PTRINFO__INL)
#define PTRINFO__INL

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
*/

inline
TTypeInfo CPointerTypeInfo::GetPointedType(void) const
{
    return m_DataTypeRef.Get();
}

inline
TConstObjectPtr CPointerTypeInfo::GetObjectPointer(TConstObjectPtr object) const
{
    return m_GetData(this, const_cast<TObjectPtr>(object));
}

inline
TObjectPtr CPointerTypeInfo::GetObjectPointer(TObjectPtr object) const
{
    return m_GetData(this, object);
}

inline
void CPointerTypeInfo::SetObjectPointer(TObjectPtr object,
                                        TObjectPtr data) const
{
    m_SetData(this, object, data);
}

#endif /* def PTRINFO__HPP  &&  ndef PTRINFO__INL */
