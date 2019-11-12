#ifndef ANNOT_TYPE_SELECTOR__HPP
#define ANNOT_TYPE_SELECTOR__HPP

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
* Author: Aleksey Grichenko, Michael Kimelman, Eugene Vasilchenko
*
* File Description:
*   Annotations selector structure.
*
*/


#include <objects/seq/Seq_annot.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


struct SAnnotTypeSelector
{
    typedef CSeq_annot::C_Data::E_Choice TAnnotType;
    typedef Uint1 TAnnotTypeInt;
    typedef CSeqFeatData::E_Choice       TFeatType;
    typedef Uint1 TFeatTypeInt;
    typedef CSeqFeatData::ESubtype       TFeatSubtype;
    typedef Uint2 TFeatSubtypeInt;

    SAnnotTypeSelector(TAnnotType annot = CSeq_annot::C_Data::e_not_set)
        : m_FeatSubtype(CSeqFeatData::eSubtype_any),
          m_FeatType(CSeqFeatData::e_not_set),
          m_AnnotType(TAnnotTypeInt(annot))
    {
    }

    SAnnotTypeSelector(TFeatType  feat)
        : m_FeatSubtype(CSeqFeatData::eSubtype_any),
          m_FeatType(TFeatTypeInt(feat)),
          m_AnnotType(CSeq_annot::C_Data::e_Ftable)
    {
    }

    SAnnotTypeSelector(TFeatSubtype feat_subtype)
        : m_FeatSubtype(TFeatSubtypeInt(feat_subtype)),
          m_FeatType(TFeatTypeInt(CSeqFeatData::GetTypeFromSubtype(feat_subtype))),
          m_AnnotType(CSeq_annot::C_Data::e_Ftable)
    {
    }
   
    TAnnotType GetAnnotType(void) const
        {
            return TAnnotType(m_AnnotType);
        }

    TFeatType GetFeatType(void) const
        {
            return TFeatType(m_FeatType);
        }

    TFeatSubtype GetFeatSubtype(void) const
        {
            return TFeatSubtype(m_FeatSubtype);
        }

    bool operator<(const SAnnotTypeSelector& s) const
        {
            if ( m_AnnotType != s.m_AnnotType )
                return m_AnnotType < s.m_AnnotType;
            if ( m_FeatType != s.m_FeatType )
                return m_FeatType < s.m_FeatType;
            return m_FeatSubtype < s.m_FeatSubtype;
        }

    bool operator==(const SAnnotTypeSelector& s) const
        {
            return m_AnnotType == s.m_AnnotType &&
                m_FeatType == s.m_FeatType &&
                m_FeatSubtype == s.m_FeatSubtype;
        }

    bool operator!=(const SAnnotTypeSelector& s) const
        {
            return m_AnnotType != s.m_AnnotType ||
                m_FeatType != s.m_FeatType ||
                m_FeatSubtype != s.m_FeatSubtype;
        }

    void SetAnnotType(TAnnotType type)
        {
            if ( m_AnnotType != type ) {
                m_AnnotType = TAnnotTypeInt(type);
                // Reset feature type/subtype
                m_FeatType = CSeqFeatData::e_not_set;
                m_FeatSubtype = CSeqFeatData::eSubtype_any;
            }
        }

    void SetFeatType(TFeatType type)
        {
            m_FeatType = TFeatTypeInt(type);
            // Adjust annot type and feature subtype
            m_AnnotType = CSeq_annot::C_Data::e_Ftable;
            m_FeatSubtype = CSeqFeatData::eSubtype_any;
        }

    void SetFeatSubtype(TFeatSubtype subtype)
        {
            m_FeatSubtype = TFeatSubtypeInt(subtype);
            // Adjust annot type and feature type
            m_AnnotType = CSeq_annot::C_Data::e_Ftable;
            if (m_FeatSubtype != CSeqFeatData::eSubtype_any) {
                m_FeatType =
                    TFeatTypeInt(CSeqFeatData::GetTypeFromSubtype(GetFeatSubtype()));
            }
        }

private:
    TFeatSubtypeInt m_FeatSubtype;  // Seq-feat subtype
    TFeatTypeInt    m_FeatType;   // Seq-feat type
    TAnnotTypeInt   m_AnnotType;  // Annotation type
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // ANNOT_TYPE_SELECTOR__HPP
