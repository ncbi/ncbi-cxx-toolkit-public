#ifndef OBJECTS_OBJMGR_IMPL___ANNOT_FINDER__HPP
#define OBJECTS_OBJMGR_IMPL___ANNOT_FINDER__HPP

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
* Author: Maxim Didenko
*
* File Description:
*
*/

#include <corelib/ncbiobj.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CTSE_Info;
class CAnnotName;

class CSeq_feat;
class CSeq_align;
class CSeq_graph;
class CSeq_entry_Info;
class CAnnotObject_Info;
class CSeq_annot_Info;
class CAnnot_descr;


class IFindContext;

class NCBI_XOBJMGR_EXPORT CSeq_annot_Finder
{
public:
    CSeq_annot_Finder(CTSE_Info& tse);

    const CAnnotObject_Info* Find(const CSeq_entry_Info& entry,
                                  const CAnnotName& name, 
                                  const CSeq_feat& feat);
    const CAnnotObject_Info* Find(const CSeq_entry_Info& entry,
                                  const CAnnotName& name, 
                                  const CSeq_align& align);
    const CAnnotObject_Info* Find(const CSeq_entry_Info& entry,
                                  const CAnnotName& name, 
                                  const CSeq_graph& graph);

    const CSeq_annot_Info* Find(const CSeq_entry_Info& entry,
                                const CAnnotName& name,
                                const CAnnot_descr& descr); 
    const CSeq_annot_Info* Find(const CSeq_entry_Info& entry,
                                const CAnnotName& name);

private:

    void x_Find(const CSeq_entry_Info& entry,
                const CAnnotName& name, 
                IFindContext& context);
   
    CTSE_Info& m_TSE;
};


inline 
CSeq_annot_Finder::CSeq_annot_Finder(CTSE_Info& tse)
    : m_TSE(tse)
{
}

END_SCOPE(objects)
END_NCBI_SCOPE

#endif //OBJECTS_OBJMGR_IMPL___ANNOT_FINDER__HPP
