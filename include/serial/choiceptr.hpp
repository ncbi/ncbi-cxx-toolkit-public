#ifndef CHOICEPTR__HPP
#define CHOICEPTR__HPP

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
* Revision 1.4  1999/10/25 19:07:12  vasilche
* Fixed coredump on non initialized choices.
* Fixed compilation warning.
*
* Revision 1.3  1999/09/22 20:11:48  vasilche
* Modified for compilation on IRIX native c++ compiler.
*
* Revision 1.2  1999/09/14 18:54:02  vasilche
* Fixed bugs detected by gcc & egcs.
* Removed unneeded includes.
*
* Revision 1.1  1999/09/07 20:57:43  vasilche
* Forgot to add some files.
*
* ===========================================================================
*/

#include <serial/ptrinfo.hpp>
#include <serial/memberlist.hpp>
#include <vector>
#include <map>
#include <memory>

BEGIN_NCBI_SCOPE

// CTypeInfo for pointers which behavelike CHOICE (select one of limited choices)
class CChoicePointerTypeInfo : public CPointerTypeInfo
{
    typedef CPointerTypeInfo CParent;
public:
    typedef CMembers::TIndex TIndex;
    typedef vector<CTypeRef> TVariantTypes;
    typedef map<const type_info*, TIndex, CTypeInfoOrder> TVariantsByType;

    CChoicePointerTypeInfo(TTypeInfo typeInfo);
    CChoicePointerTypeInfo(const string& name, TTypeInfo typeInfo);

    void AddVariant(const CMemberId& id, const CTypeRef& type);

protected:
    virtual TTypeInfo GetTypeInfo(TConstObjectPtr object) const;

    virtual void WriteData(CObjectOStream& out, TConstObjectPtr obejct) const;

    virtual void ReadData(CObjectIStream& in, TObjectPtr object) const;

private:
    void Init(void);
    const TVariantsByType& VariantsByType(void) const;
    TIndex FindVariant(TConstObjectPtr object) const;

    CMembers m_Variants;
    TVariantTypes m_VariantTypes;
    mutable auto_ptr<TVariantsByType> m_VariantsByType;
};

//#include <serial/choiceptr.inl>

END_NCBI_SCOPE

#endif  /* CHOICEPTR__HPP */
