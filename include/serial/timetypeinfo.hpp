#ifndef TIMETYPEINFO__HPP
#define TIMETYPEINFO__HPP

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
*   TypeInfo and serialization functions for CTime class
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbitime.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objcopy.hpp>
#include <serial/serial.hpp>
#include <serial/stdtypesimpl.hpp>


/** @addtogroup TypeInfoCPP
 *
 * @{
 */


BEGIN_NCBI_SCOPE

const string kSerialTimeFormat = "M/D/Y h:m:s.S Z";

class CTimeFunctions
{
public:
    static CTime& Get(TObjectPtr object)
        {
            return CTypeConverter<CTime>::Get(object);
        }
    static const CTime& Get(TConstObjectPtr object)
        {
            return CTypeConverter<CTime>::Get(object);
        }

    static void SetIOFunctions(CPrimitiveTypeInfo* info)
        {
            info->SetIOFunctions(&Read, &Write, &Copy, &Skip);
        }

    static void SetMemFunctions(CPrimitiveTypeInfo* info)
        {
            info->SetMemFunctions(&Create,
                                  &IsDefault, &SetDefault,
                                  &Equals, &Assign);
        }

    static TObjectPtr Create(TTypeInfo /*typeInfo*/,
                             CObjectMemoryPool* /*memoryPool*/)
        {
            return new CTime();
        }
    static bool IsDefault(TConstObjectPtr objectPtr)
        {
            return Get(objectPtr).IsEmpty();
        }
    static void SetDefault(TObjectPtr objectPtr)
        {
            Get(objectPtr).Clear();
        }

    static bool Equals(TConstObjectPtr obj1, TConstObjectPtr obj2,
                       ESerialRecursionMode)
        {
            return Get(obj1) == Get(obj2);
        }
    static void Assign(TObjectPtr dst, TConstObjectPtr src,
                       ESerialRecursionMode)
        {
            Get(dst) = Get(src);
        }

    static void Read(CObjectIStream& in,
                     TTypeInfo , TObjectPtr objectPtr)
        {
            string s;
            in.ReadStd(s);
            CTime tmp(s, kSerialTimeFormat);
            Get(objectPtr) = tmp;
        }
    static void Write(CObjectOStream& out,
                      TTypeInfo , TConstObjectPtr objectPtr)
        {
            out.WriteStd(Get(objectPtr).AsString(kSerialTimeFormat));
        }
    static void Skip(CObjectIStream& in, TTypeInfo )
        {
            string s;
            in.ReadStd(s);
            CTime(s, kSerialTimeFormat);
        }
    static void Copy(CObjectStreamCopier& copier, TTypeInfo )
        {
            string s;
            copier.In().ReadStd(s);
            CTime(s, kSerialTimeFormat);
            copier.Out().WriteStd(s);
        }
};


class CTimeTypeInfo : public CPrimitiveTypeInfoString
{
public:
    CTimeTypeInfo(void);
    void GetValueString(TConstObjectPtr objectPtr, string& value) const;
    void SetValueString(TObjectPtr objectPtr, const string& value) const;
private:
};


/* @} */


inline
CTimeTypeInfo::CTimeTypeInfo(void)
{
    typedef CTimeFunctions TFunctions;
    SetMemFunctions(&TFunctions::Create,
                    &TFunctions::IsDefault, &TFunctions::SetDefault,
                    &TFunctions::Equals, &TFunctions::Assign);
    SetIOFunctions(&TFunctions::Read, &TFunctions::Write,
                   &TFunctions::Copy, &TFunctions::Skip);
}


inline
void CTimeTypeInfo::GetValueString(TConstObjectPtr object,
                                   string& value) const
{
    value = CTimeFunctions::Get(object);
}


inline
void CTimeTypeInfo::SetValueString(TObjectPtr object,
                                   const string& value) const
{
    CTimeFunctions::Get(object) = value;
}


/** @addtogroup TypeInfoCPP
 *
 * @{
 */


EMPTY_TEMPLATE
class CStdTypeInfo<CTime>
{
public:
    static TTypeInfo GetTypeInfo(void);
    static CTypeInfo* CreateTypeInfo(void);
};


/* @} */


inline
TTypeInfo CStdTypeInfo<CTime>::GetTypeInfo(void)
{
    static TTypeInfo info = CreateTypeInfo();
    return info;
}


inline
CTypeInfo* CStdTypeInfo<CTime>::CreateTypeInfo(void)
{
    return new CTimeTypeInfo();
}


inline
TTypeInfoGetter GetStdTypeInfoGetter(const CTime* )
{
    return &CStdTypeInfo<CTime>::GetTypeInfo;
}


END_NCBI_SCOPE

#endif  /* TIMETYPEINFO__HPP */



/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.7  2005/04/26 14:18:49  vasilche
* Allow allocation of objects in CObjectMemoryPool.
*
* Revision 1.6  2004/08/17 14:39:40  dicuccio
* Dropped unnecessary export specifiers
*
* Revision 1.5  2004/03/25 15:57:55  gouriano
* Added possibility to copy and compare serial object non-recursively
*
* Revision 1.4  2003/09/29 21:22:43  golikov
* fix GetStdTypeInfoGetter to actually work, remove unused vars warnings
*
* Revision 1.3  2003/04/15 16:19:05  siyan
* Added doxygen support
*
* Revision 1.2  2002/12/23 18:38:51  dicuccio
* Added WIn32 export specifier: NCBI_XSERIAL_EXPORT.
* Moved all CVS logs to the end.
*
* Revision 1.1  2002/11/19 15:12:03  grichenk
* Initial Revision
*
*
* ===========================================================================
*/
