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
* Author:  Sergiy Gotvyanskyy, NCBI
*
* File Description:
*   Reader for feature tables 
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
//#include <objects/general/User_object.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objects/seq/Seq_annot.hpp>

#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/RNA_ref.hpp>

#include <util/line_reader.hpp>
//#include <objtools/readers/source_mod_parser.hpp>

#include "feature_table_reader.hpp"

#include "table2asn_context.hpp"

#include <objtools/readers/readfeat.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

namespace
{

void MoveDescr(CSeq_entry& dest, CBioseq& src)
{
	CSeq_descr::Tdata::iterator it = src.SetDescr().Set().begin();

	while(it != src.SetDescr().Set().end())
	{
		switch ((**it).Which())
		{
		case CSeqdesc_Base::e_Pub:
		case CSeqdesc_Base::e_Source:
			{
			dest.SetDescr().Set().push_back(*it); 
			src.SetDescr().Set().erase(it++);
			}
			break;
		default:
			it++;
		}
	}
	dest.Parentize();
}

CRef<CSerialObject> ConvertSeqIntoSeqSet(const CSeq_entry& obj)
{
    CRef<CSeq_entry> seqset(new CSeq_entry);
	CRef<CSeq_entry> newentry(new CSeq_entry);
	newentry->Assign(obj);
	seqset->SetSet().SetSeq_set().push_back(newentry);

	MoveDescr(*seqset, newentry->SetSeq());
	return CRef<CSerialObject>(seqset);
}

bool CheckIfNeedConversion(const CSeq_annot::C_Data::TFtable& table)
{
	ITERATE(CSeq_annot::C_Data::TFtable, it, table)
	{
		if((**it).CanGetData())
		{
			switch ((**it).GetData().Which())
			{
			case CSeqFeatData::e_Cdregion:
				return true;
                        default:
                          break;
			}
		}
	}
	return false;
}

bool CheckIfNeedConversion(const CSeq_entry& entry)
{
	ITERATE(CSeq_entry::TAnnot, it, entry.GetAnnot())
	{
		if ((**it).IsFtable())
		{
			if (CheckIfNeedConversion((**it).GetData().GetFtable()))
				return true;
		}
	}

	return false;
}

}

void CFeatureTableReader::MergeFeatures(CRef<CSerialObject>& obj)
{
	CRef<CSeq_entry> entry(dynamic_cast<CSeq_entry*>(obj.GetPointerOrNull()));
	if (entry.NotEmpty())
	{
		switch(entry->Which())
		{
		case CSeq_entry::e_Seq:
			if (CheckIfNeedConversion(*entry))
			  obj = ConvertSeqIntoSeqSet(*entry);
			break;
                default:
                        break;
		}
	}
}

void CFeatureTableReader::ReadFeatureTable(CRef<CSerialObject>& obj, ILineReader& line_reader)
{
	CRef<CSeq_entry> entry(dynamic_cast<CSeq_entry*>(obj.GetPointerOrNull()));
	CFeature_table_reader::ReadSequinFeatureTables(line_reader, *entry, CFeature_table_reader::fCreateGenesFromCDSs);
	// this may convert seq into seq-set
	MergeFeatures(obj);
}


END_NCBI_SCOPE

