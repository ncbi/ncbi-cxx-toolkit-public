#ifndef OBJTOOLS_FORMAT_ITEMS___CONTIG_ITEM__HPP
#define OBJTOOLS_FORMAT_ITEMS___CONTIG_ITEM__HPP

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
*   Contig item for flat-file generator
*
*/
#include <corelib/ncbistd.hpp>

#include <objects/seqloc/Seq_loc.hpp>

#include <objtools/format/items/item_base.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CBioseqContext;
class IFormatter;


///////////////////////////////////////////////////////////////////////////
//
// CONTIG

class CContigItem : public CFlatItem
{
public:
    CContigItem(CBioseqContext& ctx);
    void Format(IFormatter& formatter, IFlatTextOStream& text_os) const;

    const CSeq_loc& GetLoc(void) const { return *m_Loc; }

private:
    void x_GatherInfo(CBioseqContext& ctx);
    // data
    CRef<CSeq_loc>     m_Loc;
};


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.3  2004/04/22 15:34:41  shomrat
* Changes in context
*
* Revision 1.2  2004/02/19 17:49:35  shomrat
* use non-const reference
*
* Revision 1.1  2003/12/17 19:45:27  shomrat
* Initial revision (adapted from flat lib)
*
*
* ===========================================================================
*/


#endif  /* OBJTOOLS_FORMAT_ITEMS___CONTIG_ITEM__HPP */
