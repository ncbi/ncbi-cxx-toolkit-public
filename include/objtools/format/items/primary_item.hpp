#ifndef OBJTOOLS_FORMAT_ITEMS___PRIMARY_ITEM__HPP
#define OBJTOOLS_FORMAT_ITEMS___PRIMARY_ITEM__HPP

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
*   Primary item for flat-file generator
*
*/
#include <corelib/ncbistd.hpp>
#include <util/range.hpp>

#include <list>
#include <map>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objtools/alnmgr/alnmap.hpp>
#include <objtools/format/items/item_base.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CBioseqContext;
class IFormatter;


///////////////////////////////////////////////////////////////////////////
//
// PRIMARY

class NCBI_FORMAT_EXPORT CPrimaryItem : public CFlatItem
{
public:
    CPrimaryItem(CBioseqContext& ctx);
    void Format(IFormatter& formatter, IFlatTextOStream& text_os) const;

    const string& GetString(void) const { return m_Str; }

private:
    // types
    typedef CConstRef<CSeq_align>            TAln;
    typedef list< CRef< CSeq_align > >       TAlnList;
    typedef list< CConstRef< CSeq_align > >  TAlnConstList;
    typedef multimap<CAlnMap::TRange,  TAln> TAlnMap;

    void x_GatherInfo(CBioseqContext& ctx);
    void x_GetStrForPrimary(CBioseqContext& ctx);
    void x_CollectSegments(TAlnConstList&, const TAlnList& aln_list);
    void x_CollectSegments(TAlnConstList&, const CSeq_align& aln);

    string m_Str;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* OBJTOOLS_FORMAT_ITEMS___PRIMARY_ITEM__HPP */
