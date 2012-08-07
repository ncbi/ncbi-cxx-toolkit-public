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

    EKind GetKind(void) const;

    string GetCType(const CNamespace& ns) const;
    string GetPrefixedCType(const CNamespace& ns,
                            const string& methodPrefix) const;
    string GetRef(const CNamespace& ns) const;
    string GetInitializer(void) const;

private:
    string m_CType;
    CNamespace m_Namespace;
};

class CNullTypeStrings : public CTypeStrings
{
public:
    CNullTypeStrings(const CComments& comments);
    EKind GetKind(void) const;

    bool HaveSpecialRef(void) const;

    string GetCType(const CNamespace& ns) const;
    string GetPrefixedCType(const CNamespace& ns,
                            const string& methodPrefix) const;
    string GetRef(const CNamespace& ns) const;
    string GetInitializer(void) const;

};

class CStringTypeStrings : public CStdTypeStrings
{
    typedef CStdTypeStrings CParent;
public:
    CStringTypeStrings(const string& type, const CComments& comments, bool full_ns_name);

    EKind GetKind(void) const;

    string GetInitializer(void) const;
    string GetResetCode(const string& var) const;

    void GenerateTypeCode(CClassContext& ctx) const;

};

class CStringStoreTypeStrings : public CStringTypeStrings
{
    typedef CStringTypeStrings CParent;
public:
    CStringStoreTypeStrings(const string& type, const CComments& comments, bool full_ns_name);

    bool HaveSpecialRef(void) const;

    string GetRef(const CNamespace& ns) const;

};

class CAnyContentTypeStrings : public CStdTypeStrings
{
    typedef CStdTypeStrings CParent;
public:
    CAnyContentTypeStrings(const string& type, const CComments& comments, bool full_ns_name);

    EKind GetKind(void) const;

    string GetInitializer(void) const;
    string GetResetCode(const string& var) const;

    void GenerateTypeCode(CClassContext& ctx) const;

};

class CBitStringTypeStrings : public CStdTypeStrings
{
    typedef CStdTypeStrings CParent;
public:
    CBitStringTypeStrings(const string& type, const CComments& comments);

    EKind GetKind(void) const;

    string GetInitializer(void) const;
    string GetResetCode(const string& var) const;

    void GenerateTypeCode(CClassContext& ctx) const;

};

END_NCBI_SCOPE

#endif
