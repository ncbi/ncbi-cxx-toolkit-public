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
 * Authors:  Frank Ludwig
 *
 * File Description:  Write bed file
 *
 */

#include <ncbi_pch.hpp>

#include <objects/seq/Seq_annot.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/mapped_feat.hpp>

#include <objtools/writers/write_util.hpp>
#include <objtools/writers/ucscregion_writer.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//  ----------------------------------------------------------------------------
CUCSCRegionWriter::CUCSCRegionWriter(
    CScope& scope,
    CNcbiOstream& ostr,
    unsigned int uFlags ) :
//  ----------------------------------------------------------------------------
    CWriterBase(ostr, uFlags),
    m_Scope(scope)
{
};

//  ----------------------------------------------------------------------------
CUCSCRegionWriter::~CUCSCRegionWriter()
//  ----------------------------------------------------------------------------
{
};

//  ----------------------------------------------------------------------------
bool CUCSCRegionWriter::WriteAnnot( 
    const CSeq_annot& annot,
    const CTempString& separator)
//  ----------------------------------------------------------------------------
{
    if( annot.CanGetDesc() ) {
        ITERATE(CAnnot_descr::Tdata, DescIter, annot.GetDesc().Get()) {
            const CAnnotdesc& desc = **DescIter;
            if(desc.IsUser()) {
            }
        }
    }
    
    if (annot.IsSetData() && annot.GetData().IsFtable())
    {
        size_t line_number=0;
        ITERATE(CSeq_annot::C_Data::TFtable,it, annot.GetData().GetFtable())
        {
            const CSeq_feat& feat = **it;
            if (!feat.IsSetData()) continue;
            if (!feat.IsSetLocation()) continue;
            if (!feat.IsSetTitle()) continue;
            if (!feat.GetData().IsRegion()) continue;

            int from_loc = -1;
            int to_loc = -1;
            if (feat.GetLocation().IsInt())
            {
                if (feat.GetLocation().GetInt().IsSetFrom())
                    from_loc = feat.GetLocation().GetInt().GetFrom();
                if (feat.GetLocation().GetInt().IsSetTo())
                    to_loc = feat.GetLocation().GetInt().GetTo();
            }
            else
                continue;

            const string& region = feat.GetData().GetRegion();
            string strand;
            if (feat.GetLocation().IsSetStrand())
            {
                if (feat.GetLocation().GetStrand() == eNa_strand_plus)
                    strand = "+";
                if (feat.GetLocation().GetStrand() == eNa_strand_minus)
                    strand = "-";
            }
            string label;
            feat.GetLocation().GetId()->GetLabel(&label, CSeq_id::eContent);

            m_Os << label << separator 
                 << from_loc + 1 << separator
                 << to_loc + 1;
            if (!strand.empty())
                m_Os << separator << strand;
            m_Os << endl;
        }

    return true;
    }
    else
        return false;
}


END_NCBI_SCOPE
