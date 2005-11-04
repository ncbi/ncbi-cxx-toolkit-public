#error don't include me
#ifndef OBJECTS_FLAT___FLAT_ITEM__HPP
#define OBJECTS_FLAT___FLAT_ITEM__HPP

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
*
* File Description:
*   new (early 2003) flat-file generator -- base class for items
*   (which roughly correspond to blocks/paragraphs in the C version)
*
*/

#include <corelib/ncbiobj.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class IFlatFormatter;


class IFlatItem : public CObject
{
public:
    virtual void Format(IFlatFormatter& f) const = 0;
};


class IFlatItemOStream
{
public:
    // NB: i must be allocated on the heap!
    virtual void AddItem(CConstRef<IFlatItem> i) = 0;
    virtual ~IFlatItemOStream() { }
};

inline
IFlatItemOStream& operator <<(IFlatItemOStream& out, const IFlatItem* i)
{
    out.AddItem(CConstRef<IFlatItem>(i));
    return out;
}


// saves the items for later use (e.g., on-demand graphical display)
class CFlatItemCollector : public IFlatItemOStream
{
public:
    typedef list<CConstRef<IFlatItem> > TItems;

    const TItems& GetItems(void) const { return m_Items; }

    void AddItem(CConstRef<IFlatItem> i)
        { m_Items.push_back(i); }

private:
    TItems m_Items;
};


// derived classes predeclared here, for (minor) convenience
class CFlatForehead;
class CFlatHead;
class CFlatKeywords;
class CFlatSegment;
class CFlatSource;
class CFlatReference;
class CFlatComment;
class CFlatPrimary;
class CFlatFeatHeader;
class IFlattishFeature;
class CFlatDataHeader;
class CFlatData;
class CFlatContig;
class CFlatWGSRange;
class CFlatGenomeInfo;
class CFlatTail;

// another useful predeclaration...
class CFlatContext;

END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.5  2005/11/04 15:02:22  dicuccio
* Add explicit #error to catch compilation use
*
* Revision 1.4  2003/04/10 20:08:22  ucko
* Arrange to pass the item as an argument to IFlatTextOStream::AddParagraph
*
* Revision 1.3  2003/03/21 18:47:47  ucko
* Turn most structs into (accessor-requiring) classes; replace some
* formerly copied fields with pointers to the original data.
*
* Revision 1.2  2003/03/10 21:59:41  ucko
* Fix keywords (struct/class) in forward declarations to match reality.
*
* Revision 1.1  2003/03/10 16:39:08  ucko
* Initial check-in of new flat-file generator
*
*
* ===========================================================================
*/

#endif  /* OBJECTS_FLAT___FLAT_ITEM__HPP */
