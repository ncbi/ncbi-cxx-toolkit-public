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

    static CSeqFeatData::ESubtype GetSubtypeForIndex(size_t index);
    static SAnnotTypeSelector GetTypeSelector(size_t index);

private:
    enum ETypeSizes {
        kAnnotType_size   = CSeq_annot::C_Data::e_MaxChoice,
        kFeatType_size    = CSeqFeatData::e_MaxChoice,
        kFeatSubtype_size = CSeqFeatData::eSubtype_max
    };
    enum EAnnotIndexValues {
        kAnnotIndex_Align,
        kAnnotIndex_Graph,
        kAnnotIndex_Seq_table,
        kAnnotIndex_Ftable,
        kAnnotIndex_size = kAnnotIndex_Ftable + kFeatSubtype_size
    };

    static void x_InitIndexTables(void);

    // Initialization flag
    static bool sm_TablesInitialized;
    // Table: annot type -> index
    // (for Ftable it's just the first feature type index)
    static Uint1 sm_AnnotTypeIndexRange[kAnnotType_size][2];
    // Table: feat type -> index range, [)
    static Uint1 sm_FeatTypeIndexRange[kFeatType_size][2];
    // Table feat subtype -> index
    static Uint1 sm_FeatSubtypeIndex[kFeatSubtype_size];
    // Table index -> subtype
    static Uint1 sm_IndexSubtype[kAnnotIndex_size];
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
    size_t start, end;
    if ( size_t(type) < kAnnotType_size ) {
        start = sm_AnnotTypeIndexRange[type][0];
        end = sm_AnnotTypeIndexRange[type][1];
    }
    else {
        start = end = 0;
    }
    return TIndexRange(start, end);
}


inline
CAnnotType_Index::TIndexRange
CAnnotType_Index::GetFeatTypeRange(CSeqFeatData::E_Choice type)
{
    Initialize();
    size_t start, end;
    if ( size_t(type) < kFeatType_size ) {
        start = sm_FeatTypeIndexRange[type][0];
        end = sm_FeatTypeIndexRange[type][1];
    }
    else {
        start = end = 0;
    }
    return TIndexRange(start, end);
}


inline
size_t CAnnotType_Index::GetSubtypeIndex(CSeqFeatData::ESubtype subtype)
{
    Initialize();
    if ( size_t(subtype) < kFeatSubtype_size ) {
        return sm_FeatSubtypeIndex[subtype];
    }
    else {
        return 0;
    }
}


inline
CSeqFeatData::ESubtype CAnnotType_Index::GetSubtypeForIndex(size_t index)
{
    Initialize();
    if ( index < kAnnotIndex_size ) {
        return CSeqFeatData::ESubtype(sm_IndexSubtype[index]);
    }
    else {
        return CSeqFeatData::eSubtype_bad;
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

#endif// OBJECTS_OBJMGR_IMPL___ANNOT_TYPE_INDEX__HPP
