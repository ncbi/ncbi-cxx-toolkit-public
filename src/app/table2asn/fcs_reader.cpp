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
* Author:  Frank Ludwig, Sergiy Gotvyanskyy, NCBI
*
* File Description:
*   Reader for selected data file formats
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <objtools/readers/fasta.hpp>
#include <objtools/readers/idmapper.hpp>

#include <objects/seqset/Seq_entry.hpp>

#if 0
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqfeat/Variation_ref.hpp>

#include <objects/seq/Seq_descr.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objects/submit/Submit_block.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/biblio/Cit_sub.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/general/Object_id.hpp>
#include <corelib/ncbistre.hpp>

#include <serial/iterator.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objostrasn.hpp>
#include <serial/serial.hpp>

#endif

#include <objects/seq/Bioseq.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Delta_ext.hpp>

#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/Feat_id.hpp>

//#include <objtools/readers/error_container.hpp>

//#include "multireader.hpp"
//#include "table2asn_context.hpp"

#include "fcs_reader.hpp"
#include "table2asn_context.hpp"


#include <common/test_assert.h>  /* This header must go last */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

CForeignContaminationScreenReportReader::CForeignContaminationScreenReportReader(const CTable2AsnContext& context):
 m_context(context)
{
}

CForeignContaminationScreenReportReader::~CForeignContaminationScreenReportReader()
{
}

void CForeignContaminationScreenReportReader::LoadFile(ILineReader& reader)
{
    while (!reader.AtEOF())
    {
        reader.ReadLine();
        // First line is a collumn definitions
        CTempString current = reader.GetCurrentLine();
        if (current.empty())
            continue;
        if (*current.begin() == '#')
            continue;

        // Each line except first is a set of values, first collumn is a sequence id
        vector<string> values;
        NStr::Tokenize(current, "\t", values);
        if (values.size() == 5)
        {               
            TColumns new_cols;
            new_cols.name = values[0];
            new_cols.length = NStr::StringToInt(values[1]);
            if (values[2] != "-")
            {
                string s1, s2;
                NStr::SplitInTwo(values[2], "..", s1, s2, NStr::fSplit_ByPattern);
                new_cols.start = NStr::StringToInt(s1);
                new_cols.stop  = NStr::StringToInt(s2);
            }
            new_cols.mode  = values[3][0];
            new_cols.source = values[4];

            m_data.insert(multimap<string, TColumns>::value_type("lcl|" + new_cols.name, new_cols));
        }
    }
}


bool CForeignContaminationScreenReportReader::AnnotateOrRemove(CSeq_entry& entry) const
{
    string label;
    entry.GetLabel(&label, CSeq_entry::eContent);
    if (label.empty())
        return true;

    if (entry.IsSeq())
    {
        if (entry.GetSeq().GetId().empty())
        {
            return true;
        }
        if (entry.SetSeq().IsSetLength())
        {
            if (entry.SetSeq().GetLength() < m_context.m_minimal_sequence_length)
            {
                return true;
            }
        }
    }

    multimap<string, TColumns>::const_iterator it = m_data.find(label);

    if (it == m_data.end())
    {
        return false;
    }

    while (it != m_data.end() && it->first == label)
    {
        const TColumns& data = it->second;
        if (data.mode == 'X')
        {
            // remove it
            return true;
        }
        else
        if (data.mode == 'M')
        {
#if 0
            if (m_context.m_fcs_trim)
            {
            }
            else
#endif
            {
                CRef<CSeq_feat> feat(new CSeq_feat());
                feat->SetLocation().SetInt().SetId().Assign(*entry.SetSeq().SetId().front());
                feat->SetLocation().SetInt().SetFrom(data.start);
                feat->SetLocation().SetInt().SetTo(data.stop);
                feat->SetData().SetImp().SetKey("misc_feature");
                feat->SetComment("possible contamination");

                //if (entry->IsSeq())
                //    m_context.ConvertSeqIntoSeqSet(*entry);

                CRef<CSeq_annot> set_annot(new CSeq_annot);
                set_annot->SetData().SetFtable().push_back(feat);
                entry.SetSeq().SetAnnot().push_back(set_annot);
            }
        }
        ++it;
    }

    return false;
}

void CForeignContaminationScreenReportReader::PostProcess(CSeq_entry& entry)
{
    if (entry.IsSet())
    {
        CSeq_entry::TSet::TSeq_set::iterator it = entry.SetSet().SetSeq_set().begin();
        while (it != entry.SetSet().SetSeq_set().end())
        {
            if ((**it).IsSeq())
            {
                if (AnnotateOrRemove(**it))
                {
                    entry.SetSet().SetSeq_set().erase(it++);
                    continue;
                }

            }
            ++it;
        }
    }
}

END_NCBI_SCOPE
