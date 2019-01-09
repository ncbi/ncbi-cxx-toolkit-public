#ifndef OBJTOOLS_FORMAT_ITEMS___ITEM_BASE_ITEM__HPP
#define OBJTOOLS_FORMAT_ITEMS___ITEM_BASE_ITEM__HPP

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
* Author:  Aaron Ucko, NCBI
*          Mati Shomrat
*
* File Description:
*   Base class for the various item objects
*
*/
#include <corelib/ncbistd.hpp>
#include <serial/serialbase.hpp>

#include <objtools/format/items/item.hpp>
//#include <objtools/format/context.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CBioseqContext;


class NCBI_FORMAT_EXPORT CFlatItem : public IFlatItem
{
public:
    virtual void Format(IFormatter& formatter,
                        IFlatTextOStream& text_os) const = 0;

    bool IsSetObject(void) const;
    const CSerialObject* GetObject(void) const;

    CBioseqContext* GetContext(void);
    CBioseqContext* GetContext(void) const;

    // should this item be skipped during formatting?
    bool Skip(void) const;
    // is feature on external Seq-annot?
    bool IsExternal(void) const;

    ~CFlatItem(void);

protected:
    CFlatItem(CBioseqContext* ctx = 0);

    virtual void x_GatherInfo(CBioseqContext&) {}

    void x_SetObject(const CSerialObject& obj);
    void x_SetContext(CBioseqContext& ctx);

    void x_SetSkip(void);
    void x_SetExternal(void);

private:

    // The underlying CSerialObject from the information is obtained.
    CConstRef<CSerialObject>    m_Object;
    // a context associated with this item
    CBioseqContext*             m_Context;
    // should this item be skipped?
    bool                        m_Skip;
    // is feature on external Seq-annot?
    bool                        m_External;
};


///////////////////////////////////////////////////////////
///////////////////// inline methods //////////////////////
///////////////////////////////////////////////////////////

// public methods

inline
const CSerialObject* CFlatItem::GetObject(void) const
{
    return m_Object;
}


inline
bool CFlatItem::IsSetObject(void) const
{
    return m_Object.NotEmpty(); 
}


inline
CBioseqContext* CFlatItem::GetContext(void)
{
    return m_Context;
}


inline
CBioseqContext* CFlatItem::GetContext(void) const
{
    return m_Context;
}


inline
bool CFlatItem::Skip(void) const
{
    return m_Skip;
}


inline
bool CFlatItem::IsExternal(void) const
{
    return m_External;
}


inline
CFlatItem::~CFlatItem(void)
{
}

// protected methods:

// constructor
inline
CFlatItem::CFlatItem(CBioseqContext* ctx) :
    m_Object(0),
    m_Context(ctx),
    m_Skip(false),
    m_External(false)
{
}


// Shared utility functions
inline
void CFlatItem::x_SetObject(const CSerialObject& obj) 
{
    m_Object.Reset(&obj);
}


inline
void CFlatItem::x_SetSkip(void)
{
    m_Skip = true;
    m_Object.Reset();
    m_Context = 0;
}


inline
void CFlatItem::x_SetExternal(void)
{
    m_External = true;
}


///////////////////////////////////////////////////////////
////////////////// end of inline methods //////////////////
///////////////////////////////////////////////////////////


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* OBJTOOLS_FORMAT_ITEMS___ITEM_BASE_ITEM__HPP */
