#ifndef OBJTOOLS_FORMAT_ITEMS___TSA_ITEM__HPP
#define OBJTOOLS_FORMAT_ITEMS___TSA_ITEM__HPP

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
* Author: Michael Kornbluh (heavily based on code by Mati Shomrat)
*
* File Description:
*   item representing TSA
*   
*/
#include <corelib/ncbistd.hpp>

#include <objtools/format/items/item_base.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CUser_object;
class CBioseqContext;
class IFormatter;


///////////////////////////////////////////////////////////////////////////
//
// TSA

class NCBI_FORMAT_EXPORT CTSAItem : public CFlatItem
{
public:
    enum ETSAType {
        eTSA_not_set,
        eTSA_Projects
        /*eTSA_ScaffoldList,
        eTSA_ContigList*/
    };
    typedef ETSAType    TTSAType;

    CTSAItem(TTSAType type, const string& first, const string& last,
        const CUser_object& uo, CBioseqContext& ctx);
    void Format(IFormatter& formatter, IFlatTextOStream& text_os) const;
    
    TTSAType      GetType   (void) const { return m_Type;  }
    const string& GetFirstID(void) const { return m_First; }
    const string& GetLastID (void) const { return m_Last;  }

private:

    void x_GatherInfo(CBioseqContext& ctx);

    // data
    TTSAType    m_Type;
    string      m_First;
    string      m_Last;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* OBJTOOLS_FORMAT_ITEMS___TSA_ITEM__HPP */
