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
*/

#include <corelib/ncbistd.hpp>
#include <serial/impl/choice.hpp>
#include <serial/impl/stdtypes.hpp>
#include <serial/impl/stdtypeinfo.hpp>
#include <map>


/** @addtogroup TypeInfoCPP
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class CPointerTypeInfo;

// CTypeInfo for pointers which behave like CHOICE
// (select one of limited choices)
class NCBI_XSERIAL_EXPORT CChoicePointerTypeInfo : public CChoiceTypeInfo
{
    typedef CChoiceTypeInfo CParent;
public:
    typedef map<const type_info*, TMemberIndex, CLessTypeInfo> TVariantsByType;

    CChoicePointerTypeInfo(TTypeInfo pointerType);

    const CPointerTypeInfo* GetPointerTypeInfo(void) const
        {
            return m_PointerTypeInfo;
        }

    static TTypeInfo GetTypeInfo(TTypeInfo base);
    static CTypeInfo* CreateTypeInfo(TTypeInfo base);

protected:
    static TMemberIndex GetPtrIndex(const CChoiceTypeInfo* choiceType,
                                    TConstObjectPtr choicePtr);
    static void SetPtrIndex(const CChoiceTypeInfo* choiceType,
                            TObjectPtr choicePtr,
                            TMemberIndex index,
                            CObjectMemoryPool* memPool);
    static void ResetPtrIndex(const CChoiceTypeInfo* choiceType,
                              TObjectPtr choicePtr);

private:
    void SetPointerType(TTypeInfo pointerType);

    const CPointerTypeInfo* m_PointerTypeInfo;
    TVariantsByType m_VariantsByType;
    TMemberIndex m_NullPointerIndex;
};

END_NCBI_SCOPE

/* @} */

#endif  /* CHOICEPTR__HPP */
