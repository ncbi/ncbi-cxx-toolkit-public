 #ifndef OBJTOOLS_FORMAT_ITEMS___GENOME_PROJECT_ITEM__HPP
#define OBJTOOLS_FORMAT_ITEMS___GENOME_PROJECT_ITEM__HPP

/*
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
*          Frank Ludwig, NCBI
*
* File Description:
*   Genome Project item for flat-file generator
*
*/
#include <corelib/ncbistd.hpp>

#include <list>

#include <objtools/format/items/item_base.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CBioseqContext;
class IFormatter;


///////////////////////////////////////////////////////////////////////////
//
// Genome Project

class NCBI_FORMAT_EXPORT CGenomeProjectItem : public CFlatItem
{
public:
    typedef string TDBLinkLine;
    typedef vector< TDBLinkLine > TDBLinkLineVec;

    CGenomeProjectItem(CBioseqContext& ctx);
    void Format(IFormatter& formatter, IFlatTextOStream& text_os) const;
    
    const vector<int> & GetProjectNumbers(void) const;
    const TDBLinkLineVec & GetDBLinkLines(void) const;

private:
    void x_GatherInfo(CBioseqContext& ctx);

    // data
    vector<int> m_ProjectNumbers;
    TDBLinkLineVec m_DBLinkLines;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* OBJTOOLS_FORMAT_ITEMS___GENOME_PROJECT_ITEM__HPP */
