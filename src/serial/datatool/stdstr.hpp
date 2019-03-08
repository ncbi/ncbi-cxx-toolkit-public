#ifndef STDSTR_HPP
#define STDSTR_HPP

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
*   C++ class info: includes, used classes, C++ code etc.
*
*/

#include "typestr.hpp"
#include "namespace.hpp"

BEGIN_NCBI_SCOPE

class CStdTypeStrings : public CTypeStrings
{
public:
    CStdTypeStrings(const string& type, const CComments& comments, bool full_ns_name);

    virtual EKind GetKind(void) const override;

    virtual string GetCType(const CNamespace& ns) const override;
    virtual string GetPrefixedCType(const CNamespace& ns,
                            const string& methodPrefix) const override;
    virtual string GetRef(const CNamespace& ns) const override;
    virtual string GetInitializer(void) const override;

    virtual void SetStorageType(const string& storage) override;
    virtual string GetStorageType(const CNamespace& ns) const override;
    void SetBigInt(bool is_big = true);
private:
    string m_CType;
    string m_Storage;
    CNamespace m_Namespace;
    bool m_BigInt;
};

class CNullTypeStrings : public CTypeStrings
{
public:
    CNullTypeStrings(const CComments& comments);
    virtual EKind GetKind(void) const override;

    virtual bool HaveSpecialRef(void) const override;

    virtual string GetCType(const CNamespace& ns) const override;
    virtual string GetPrefixedCType(const CNamespace& ns,
                            const string& methodPrefix) const override;
    virtual string GetRef(const CNamespace& ns) const override;
    virtual string GetInitializer(void) const override;

};

class CStringTypeStrings : public CStdTypeStrings
{
    typedef CStdTypeStrings CParent;
public:
    CStringTypeStrings(const string& type, const CComments& comments, bool full_ns_name);

    virtual EKind GetKind(void) const override;

    virtual string GetInitializer(void) const override;
    virtual string GetResetCode(const string& var) const override;

    virtual void GenerateTypeCode(CClassContext& ctx) const override;

};

class CStringStoreTypeStrings : public CStringTypeStrings
{
    typedef CStringTypeStrings CParent;
public:
    CStringStoreTypeStrings(const string& type, const CComments& comments, bool full_ns_name);

    virtual bool HaveSpecialRef(void) const override;

    virtual string GetRef(const CNamespace& ns) const override;

};

class CAnyContentTypeStrings : public CStdTypeStrings
{
    typedef CStdTypeStrings CParent;
public:
    CAnyContentTypeStrings(const string& type, const CComments& comments, bool full_ns_name);

    virtual EKind GetKind(void) const override;

    virtual string GetInitializer(void) const override;
    virtual string GetResetCode(const string& var) const override;

    virtual void GenerateTypeCode(CClassContext& ctx) const override;

};

class CBitStringTypeStrings : public CStdTypeStrings
{
    typedef CStdTypeStrings CParent;
public:
    CBitStringTypeStrings(const string& type, const CComments& comments, CTypeStrings* bit_names);

    virtual EKind GetKind(void) const override;

    virtual string GetInitializer(void) const override;
    virtual string GetResetCode(const string& var) const override;

    virtual void GenerateTypeCode(CClassContext& ctx) const override;
private:
    AutoPtr<CTypeStrings> m_BitNames;
};

END_NCBI_SCOPE

#endif
