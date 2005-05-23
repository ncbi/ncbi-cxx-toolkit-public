#ifndef OBJECTS_OBJMGR_IMPL___ANNOT_TYPE_INDEX__HPP
#define OBJECTS_OBJMGR_IMPL___ANNOT_TYPE_INDEX__HPP

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
* Author: Aleksey Grichenko
*
* File Description:
*   Annotation type indexes
*
*/


#include <corelib/ncbistd.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seq/Seq_annot.hpp>

#include <vector>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


struct SAnnotTypeSelector;
struct SIdAnnotObjs;
class CAnnotObject_Info;
struct SAnnotObject_Key;

////////////////////////////////////////////////////////////////////
//
//  CAnnotType_Index::
//
//    Helper class to convert annotation type/subtype to an index
//


class NCBI_XOBJMGR_EXPORT CAnnotType_Index
{
public:
    typedef pair<size_t, size_t> TIndexRange;

    static void Initialize(void);

    static TIndexRange GetAnnotTypeRange(CSeq_annot::C_Data::E_Choice type);
    static TIndexRange GetFeatTypeRange(CSeqFeatData::E_Choice type);
    static size_t GetSubtypeIndex(CSeqFeatData::ESubtype subtype);

    static TIndexRange GetIndexRange(const SAnnotTypeSelector& sel);
    static TIndexRange GetIndexRange(const SAnnotTypeSelector& sel,
                                     const SIdAnnotObjs& objs);
    static TIndexRange GetTypeIndex(const CAnnotObject_Info& info);
    static TIndexRange GetTypeIndex(const SAnnotObject_Key& key);

private:
    typedef vector<TIndexRange> TIndexRangeTable;
    typedef vector<size_t>      TIndexTable;

    static void x_InitIndexTables(void);

    // Initialization flag
    static bool sm_TablesInitialized;
    // Table: annot type -> index
    // (for Ftable it's just the first feature type index)
    static TIndexRangeTable sm_AnnotTypeIndexRange;
    // Table: feat type -> index range, [)
    static TIndexRangeTable sm_FeatTypeIndexRange;
    // Table feat subtype -> index
    static TIndexTable sm_FeatSubtypeIndex;
};


inline
void CAnnotType_Index::Initialize(void)
{
    if (!sm_TablesInitialized) {
        x_InitIndexTables();
    }
}


inline
CAnnotType_Index::TIndexRange
CAnnotType_Index::GetAnnotTypeRange(CSeq_annot::C_Data::E_Choice type)
{
    Initialize();
    if ( size_t(type) < sm_AnnotTypeIndexRange.size() ) {
        return sm_AnnotTypeIndexRange[type];
    }
    else {
        return TIndexRange(0, 0);
    }
}


inline
CAnnotType_Index::TIndexRange
CAnnotType_Index::GetFeatTypeRange(CSeqFeatData::E_Choice type)
{
    Initialize();
    if ( size_t(type) < sm_FeatTypeIndexRange.size() ) {
        return sm_FeatTypeIndexRange[type];
    }
    else {
        return TIndexRange(0, 0);
    }
}


inline
size_t CAnnotType_Index::GetSubtypeIndex(CSeqFeatData::ESubtype subtype)
{
    Initialize();
    if ( size_t(subtype) < sm_FeatSubtypeIndex.size() ) {
        return sm_FeatSubtypeIndex[subtype];
    }
    else {
        return 0;
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  2005/05/23 14:09:55  grichenk
* Fixed indexing of feature types
*
* Revision 1.1  2004/02/04 18:03:21  grichenk
* Initial revision
*
*
* ===========================================================================
*/

#endif// OBJECTS_OBJMGR_IMPL___ANNOT_TYPE_INDEX__HPP
