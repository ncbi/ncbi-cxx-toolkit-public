#ifndef OBJTOOLS_FORMAT_ITEMS___CTRL_ITEM__HPP
#define OBJTOOLS_FORMAT_ITEMS___CTRL_ITEM__HPP

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
* Author: Mati Shomrat
*
* File Description:
*   item representing end of section line (slash line)
*   
*/
#include <corelib/ncbistd.hpp>

#include <objtools/format/items/item_base.hpp>
#include <objtools/format/items/comment_item.hpp>
#include <objtools/format/formatter.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CBioseqContext;


class NCBI_FORMAT_EXPORT CCtrlItem : public CFlatItem
{
public:
    CCtrlItem(CBioseqContext* bctx = 0) : CFlatItem(bctx) {}
};


///////////////////////////////////////////////////////////////////////////
//
// START
//
// Signals the start of the data

class NCBI_FORMAT_EXPORT CStartItem : public CCtrlItem
{
public:
    CStartItem(void) {}
    void Format(IFormatter& f, IFlatTextOStream& text_os) const {
        f.Start(text_os);
    }
};


///////////////////////////////////////////////////////////////////////////
//
// START SECTION
// 
// Signals the begining of a new section

class NCBI_FORMAT_EXPORT CStartSectionItem : public CCtrlItem
{
public:
    CStartSectionItem(CBioseqContext& ctx) : CCtrlItem(&ctx) {
        CCommentItem::ResetFirst();
    }
    void Format(IFormatter& f, IFlatTextOStream& text_os) const {
        f.StartSection(*this, text_os);
    }
};


///////////////////////////////////////////////////////////////////////////
//
// END SECTION
//
// Signals the end of a section

class NCBI_FORMAT_EXPORT CEndSectionItem : public CCtrlItem
{
public:
    CEndSectionItem(CBioseqContext& ctx) : CCtrlItem(&ctx) {}
    void Format(IFormatter& f, IFlatTextOStream& text_os) const {
        f.EndSection(*this, text_os);
    }
};


///////////////////////////////////////////////////////////////////////////
//
// END
//
// Signals the termination of data

class NCBI_FORMAT_EXPORT CEndItem : public CCtrlItem
{
public:
    CEndItem(void) {}
    void Format(IFormatter& f, IFlatTextOStream& text_os) const {
        f.End(text_os);
    }
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* OBJTOOLS_FORMAT_ITEMS___CTRL_ITEM__HPP */
