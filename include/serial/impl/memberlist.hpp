#ifndef MEMBERLIST__HPP
#define MEMBERLIST__HPP

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
#include <corelib/ncbiutil.hpp>
#include <corelib/tempstr.hpp>
#include <serial/impl/item.hpp>
#include <vector>
#include <map>


/** @addtogroup FieldsComplex
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class CConstObjectInfo;
class CObjectInfo;

// This class supports sets of members with IDs
class NCBI_XSERIAL_EXPORT CItemsInfo
{
public:
    typedef CMemberId::TTag TTag;
    typedef vector< AutoPtr<CItemInfo> > TItems;
    typedef map<CTempString, TMemberIndex, PQuickStringLess> TItemsByName;
    typedef pair< TTag, CAsnBinaryDefs::ETagClass> TTagAndClass;
    typedef map< TTagAndClass, TMemberIndex> TItemsByTag;
    typedef map<size_t, TMemberIndex> TItemsByOffset;

    CItemsInfo(void);
    virtual ~CItemsInfo(void);

    bool Empty(void) const
        {
            return m_Items.empty();
        }
    size_t Size(void) const
        {
            return m_Items.size();
        }

    static TMemberIndex FirstIndex(void)
        {
            return kFirstMemberIndex;
        }
    TMemberIndex LastIndex(void) const
        {
            return m_Items.size();
        }

    TMemberIndex Find(const CTempString& name) const;
    TMemberIndex FindDeep(const CTempString& name, bool search_attlist = false) const;
    TMemberIndex FindEmpty(void) const;
    TMemberIndex Find(const CTempString& name, TMemberIndex pos) const;
    TMemberIndex Find(TTag tag, CAsnBinaryDefs::ETagClass tagclass) const;
    TMemberIndex Find(TTag tag, CAsnBinaryDefs::ETagClass tagclass, TMemberIndex pos) const;

    static const CTypeInfo* FindRealTypeInfo(const CTypeInfo* info);
    static const CItemInfo* FindNextMandatory(const CItemInfo* info);
    static const CItemInfo* FindNextMandatory(const CTypeInfo* info);

    const CItemInfo* GetItemInfo(TMemberIndex index) const;
    void AddItem(CItemInfo* item);
    void AssignItemsTags(CAsnBinaryDefs::ETagType containerType);

    // helping member iterator class (internal use)
    class CIterator
    {
    public:
        CIterator(const CItemsInfo& items);
        CIterator(const CItemsInfo& items, TMemberIndex index);

        void SetIndex(TMemberIndex index);
        CIterator& operator=(TMemberIndex index);

        bool Valid(void) const;

        void Next(void);
        void operator++(void);

        TMemberIndex GetIndex(void) const;
        TMemberIndex operator*(void) const;

    private:
        TMemberIndex m_CurrentIndex;
        TMemberIndex m_LastIndex;
    };
    const CItemInfo* GetItemInfo(const CIterator& i) const;

protected:
    CItemInfo* x_GetItemInfo(TMemberIndex index) const;

private:
    const TItemsByName& GetItemsByName(void) const;
    const TItemsByOffset& GetItemsByOffset(void) const;
    TTagAndClass GetTagAndClass(const CIterator& i) const;
    pair<TMemberIndex, const TItemsByTag*> GetItemsByTagInfo(void) const;

    // items
    TItems m_Items;

    // items by name
    mutable auto_ptr<TItemsByName> m_ItemsByName;

    // items by tag
    mutable TMemberIndex m_ZeroTagIndex;
    mutable auto_ptr<TItemsByTag> m_ItemsByTag;

    // items by offset
    mutable auto_ptr<TItemsByOffset> m_ItemsByOffset;
};


/* @} */


#include <serial/impl/memberlist.inl>

END_NCBI_SCOPE

#endif  /* MEMBERLIST__HPP */
