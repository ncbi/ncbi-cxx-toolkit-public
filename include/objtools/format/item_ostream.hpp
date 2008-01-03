#ifndef OBJTOOLS_FORMAT___ITEM_OSTREAM_HPP
#define OBJTOOLS_FORMAT___ITEM_OSTREAM_HPP

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
*         
*
*/
#include <corelib/ncbistd.hpp>

#include <objtools/format/items/item.hpp>
#include <objtools/format/formatter.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class IFlatTextOStreamFactory;


class NCBI_FORMAT_EXPORT CFlatItemOStream : public CObject
{
public:
    // NB: formatter must be allocated on the heap!
    CFlatItemOStream(IFormatter* formatter = 0);

    void SetFormatter(IFormatter* formatter);

    // NB: item must be allocated on the heap!
    virtual void AddItem(CConstRef<IFlatItem> item) = 0;

    virtual ~CFlatItemOStream();

protected:

    // data
    CRef<IFormatter>    m_Formatter;
};


inline
CFlatItemOStream& operator<<(CFlatItemOStream& os, CConstRef<IFlatItem>& item)
{
//    if ( item != 0  &&  !item->Skip() ) {
//        os.AddItem(item);
//    }
    return os;
}


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* OBJTOOLS_FORMAT___ITEM_OSTREAM_HPP */
