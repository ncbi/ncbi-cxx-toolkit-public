#ifndef OBJTOOLS_READERS_OBJHOOK_LAMBDAS_HPP
#define OBJTOOLS_READERS_OBJHOOK_LAMBDAS_HPP
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
 * Author:  Sergiy Gotvyanskyy
 *
 * File Description: ASN.1 read/write/copy/skip hooks via lambda's
 */

#include <serial/objhook.hpp>

BEGIN_NCBI_SCOPE

template<typename _T>
class CLambdaReadHook : public CReadObjectHook
{
public:
    CLambdaReadHook(_T _lambda) : m_lambda{ _lambda } {};
    void ReadObject(CObjectIStream& in, const CObjectInfo& object) override
    {
        m_lambda(in, object);
    }

    _T m_lambda;
};

template<typename _T>
class CLambaReadMemberHook : public CReadClassMemberHook
{
public:
    CLambaReadMemberHook(_T _lambda) : m_lambda{ _lambda } {};
    void ReadClassMember(CObjectIStream& str, const CObjectInfoMI& obj) override
    {
        m_lambda(str, obj);
    }

    _T m_lambda;
};

template<typename _T>
class CLambdaSkipHook : public CSkipObjectHook
{
public:
    CLambdaSkipHook(_T _lambda) : m_lambda{ _lambda } {};
    void SkipObject(CObjectIStream& stream, const CObjectTypeInfo& type) override
    {
        m_lambda(stream, type);
    }

    _T m_lambda;
};

template<typename _T>
class CLambaSkipMemberHook : public CSkipClassMemberHook
{
public:
    CLambaSkipMemberHook(_T _lambda) : m_lambda{ _lambda } {};
    void SkipClassMember(CObjectIStream& stream, const CObjectTypeInfoMI& member) override
    {
        m_lambda(stream, member);
    }

    _T m_lambda;
};

template<typename _T>
class CLambdaCopyHook : public CCopyObjectHook
{
public:
    CLambdaCopyHook(_T _lambda) : m_lambda{ _lambda } {};
    void CopyObject(CObjectStreamCopier& copier, const CObjectTypeInfo& type) override
    {
        m_lambda(copier, type);
    }

    _T m_lambda;
};

template<typename _T>
class CLambaCopyMemberHook : public CCopyClassMemberHook
{
public:
    CLambaCopyMemberHook(_T _lambda) : m_lambda{ _lambda } {};
    void CopyClassMember(CObjectStreamCopier& copier, const CObjectTypeInfoMI& member) override
    {
        m_lambda(copier, member);
    }

    _T m_lambda;
};

template<typename _T>
class CLambdaWriteHook : public CWriteObjectHook
{
public:
    CLambdaWriteHook(_T _lambda) : m_lambda{ _lambda } {};
    void WriteObject(CObjectOStream& out, const CConstObjectInfo& object) override
    {
        m_lambda(out, object);
    }

    _T m_lambda;
};

template<typename _T>
class CLambaWriteMemberHook : public CWriteClassMemberHook
{
public:
    CLambaWriteMemberHook(_T _lambda) : m_lambda{ _lambda } {};
    void WriteClassMember(CObjectOStream& out, const CConstObjectInfoMI& member) override
    {
        m_lambda(out, member);
    }

    _T m_lambda;
};

template<typename _Func>
void SetLocalReadHook(const CObjectTypeInfo& obj_type_info, CObjectIStream& ostr, _Func _func)
{
    auto hook = Ref(new CLambdaReadHook<_Func>(_func));
    obj_type_info.SetLocalReadHook(ostr, hook);
}

template<typename _Func>
void SetLocalReadHook(TTypeInfo type_info, CObjectIStream& ostr, _Func _func)
{
    SetLocalReadHook(CObjectTypeInfo(type_info), ostr, _func);
}

template<typename _Func>
void SetLocalReadHook(const CObjectTypeInfoMI& member_info, CObjectIStream& istr, _Func _func)
{
    auto hook = Ref(new CLambaReadMemberHook<_Func>(_func));
    member_info.SetLocalReadHook(istr, hook);
}

template<typename _Func>
void SetLocalSkipHook(const CObjectTypeInfo& obj_type_info, CObjectIStream& istr, _Func _func)
{
    auto hook = Ref(new CLambdaSkipHook<_Func>(_func));
    obj_type_info.SetLocalSkipHook(istr, hook);
}

template<typename _Func>
void SetLocalSkipHook(TTypeInfo type_info, CObjectIStream& istr, _Func _func)
{
    SetLocalSkipHook(CObjectTypeInfo(type_info), istr, _func);
}

template<typename _Func>
void SetLocalSkipHook(const CObjectTypeInfoMI& member_info, CObjectIStream& istr, _Func _func)
{
    auto hook = Ref(new CLambaSkipMemberHook<_Func>(_func));
    member_info.SetLocalSkipHook(istr, hook);
}

template<typename _Func>
void SetLocalCopyHook(const CObjectTypeInfo& obj_type_info, CObjectStreamCopier& copier, _Func _func)
{
    auto hook = Ref(new CLambdaCopyHook<_Func>(_func));
    obj_type_info.SetLocalCopyHook(copier, hook);
}

template<typename _Func>
void SetLocalCopyHook(TTypeInfo type_info, CObjectStreamCopier& copier, _Func _func)
{
    SetLocalCopyHook(CObjectTypeInfo(type_info), copier, _func);
}

template<typename _Func>
void SetLocalCopyHook(const CObjectTypeInfoMI& member_info, CObjectStreamCopier& copier, _Func _func)
{
    auto hook = Ref(new CLambaCopyMemberHook<_Func>(_func));
    member_info.SetLocalCopyHook(copier, hook);
}

template<typename _Func>
void SetLocalWriteHook(const CObjectTypeInfo& obj_type_info, CObjectOStream& ostr, _Func _func)
{
    auto hook = Ref(new CLambdaWriteHook<_Func>(_func));
    obj_type_info.SetLocalWriteHook(ostr, hook);
}

template<typename _Func>
void SetLocalWriteHook(TTypeInfo type_info, CObjectOStream& ostr, _Func _func)
{
    SetLocalWriteHook(CObjectTypeInfo(type_info), ostr, _func);
}

template<typename _Func>
void SetLocalWriteHook(const CObjectTypeInfoMI& member_info, CObjectOStream& ostr, _Func _func)
{
    auto hook = Ref(new CLambaWriteMemberHook<_Func>(_func));
    member_info.SetLocalWriteHook(ostr, hook);
}

END_NCBI_SCOPE

#endif
