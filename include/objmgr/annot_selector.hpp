#ifndef ANNOT_SELECTOR__HPP
#define ANNOT_SELECTOR__HPP

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
* Author: Aleksey Grichenko, Michael Kimelman
*
* File Description:
*   Annotations selector structure.
*
*/


#include <objects/seq/Seq_annot.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

// Structure to select type of Seq-annot
struct SAnnotSelector
{
    typedef CSeq_annot::C_Data::E_Choice TAnnotChoice;
    typedef CSeqFeatData::E_Choice       TFeatChoice;

    SAnnotSelector(TAnnotChoice annot = CSeq_annot::C_Data::e_not_set,
                   TFeatChoice  feat  = CSeqFeatData::e_not_set,
                   int feat_product = false)
        : m_AnnotChoice(annot),
          m_FeatChoice(feat),
          m_FeatProduct(feat_product)
    {
    }

    bool operator==(const SAnnotSelector& sel) const
        {
            return
                m_AnnotChoice == sel.m_AnnotChoice &&
                m_FeatChoice == sel.m_FeatChoice &&
                m_FeatProduct == sel.m_FeatProduct;
        }
    bool operator< (const SAnnotSelector& sel) const
        {
            return
                m_AnnotChoice < sel.m_AnnotChoice ||
                (m_AnnotChoice == sel.m_AnnotChoice &&
                 (m_FeatChoice < sel.m_FeatChoice ||
                  (m_FeatChoice == sel.m_FeatChoice &&
                   m_FeatProduct < sel.m_FeatProduct)));
        }

    TAnnotChoice m_AnnotChoice;  // Annotation type
    TFeatChoice  m_FeatChoice;   // Seq-feat subtype
    int          m_FeatProduct;  // set to "true" for searching products
};

END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2003/02/24 19:36:19  vasilche
* Added missing annot_selector.hpp.
*
*
* ===========================================================================
*/

#endif  // ANNOT_SELECTOR__HPP
