/* $Id$
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
 * Author:  Chris Lanczycki
 *
 * File Description:
 *
 *       Functions for manipulating Bioseqs and other sequence representations
 *
 * ===========================================================================
 */



#include <ncbi_pch.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seqdesc.hpp>

#include <objects/seq/NCBIeaa.hpp>
#include <objects/seq/IUPACaa.hpp>
#include <objects/seq/NCBIstdaa.hpp>

#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>

#include <objects/seqloc/Seq_id.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqloc/PDB_seq_id.hpp>

#include <util/sequtil/sequtil_convert.hpp>

#include <algo/structure/cd_utils/cuSequence.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils) // namespace ncbi::objects::

//  Mapping of index to amino acid
//const char *cdt_stdaaMap = "-ABCDEFGHIKLMNPQRSTVWXYZU*";

bool SeqIdsMatch(const CRef< CSeq_id >& ID1, const CRef< CSeq_id >& ID2) {
//-------------------------------------------------------------------------
// return true if the 2 sequence-id's match
//-------------------------------------------------------------------------
    if (ID1.Empty() || ID2.Empty()) {
        return false;
    }
    return ID1->Match(*ID2);
}

CRef< CSeq_id > CopySeqId(const CRef< CSeq_id >& seqId)
{
	CRef< CSeq_id > seqIdCopy(new CSeq_id());
	if (seqId->IsPdb())
	{
		seqIdCopy->Reset();
		seqIdCopy->SetPdb().SetMol(seqId->GetPdb().GetMol());
		if (seqId->GetPdb().IsSetChain())
			seqIdCopy->SetPdb().SetChain(seqId->GetPdb().GetChain());
		if (seqId->GetPdb().IsSetRel())
			seqIdCopy->SetPdb().SetRel(const_cast <CDate&> (seqId->GetPdb().GetRel()));
	}
	else
		seqIdCopy->Assign(*seqId);
	return seqIdCopy;
}

//   Return 0 if Seq_id is not of proper type (e_General and database 'CDD')
int  GetCDDPssmIdFromSeqId(const CRef< CSeq_id >& id) {
    int pssmId = 0;
    string database = "CDD";

    if (id.NotEmpty() && id->IsGeneral()) {
        if (id->GetGeneral().GetDb() == database) {
            if (id->GetGeneral().GetTag().IsId()) {
                pssmId = id->GetGeneral().GetTag().GetId();
            }
        }
    }
    return pssmId;
}

//  formerly FindMMDBIdInBioseq(...)
int GetMMDBId(const CBioseq& bioseq) {
  list< CRef< CSeq_annot > >::const_iterator  j;
  list< CRef< CSeq_id > >::const_iterator k;
  int id = -1;

  // look through each seq-annot
  if (bioseq.IsSetAnnot()) {
	  for (j = bioseq.GetAnnot().begin(); j!= bioseq.GetAnnot().end(); j++) {
	     // look through ids for an mmdb id
	      if ((*j)->GetData().IsIds()) {
			  for (k = (*j)->GetData().GetIds().begin(); k != (*j)->GetData().GetIds().end(); k++) {
				  if ((*k)->IsGeneral()) {
					  if ((*k)->GetGeneral().GetDb() == "mmdb") {
						  if ((*k)->GetGeneral().GetTag().IsId()) {
							  id = (*k)->GetGeneral().GetTag().GetId();
							  return(id);
						  }
					  }
				  }
			  }
		  }
	  }
  }

  return(id);
}


//string CCd::GetSpecies(int SeqIndex) {  
string GetSpeciesFromBioseq(const CBioseq& bioseq) {  
//-------------------------------------------------------------------------
// get the species of the SeqIndex sequence; does not use taxonomy server
//-------------------------------------------------------------------------
//  int  SeqCount;
  list< CRef< CSeqdesc > >::const_iterator  j;
  if (bioseq.IsSetDescr()) {
      // look through the sequence descriptions
      for (j=bioseq.GetDescr().Get().begin();
           j!=bioseq.GetDescr().Get().end(); j++) {
          // if there's an organism identifier
          if ((*j)->IsSource()) {
              // retrieve common or formal name
              if ((*j)->GetSource().GetOrg().IsSetTaxname()) {
                  return((*j)->GetSource().GetOrg().GetTaxname());
              }
              if ((*j)->GetSource().GetOrg().IsSetCommon()) {
                  return((*j)->GetSource().GetOrg().GetCommon());
              }
          }
      }
  }
  return(kEmptyStr);
}

//  Length = 0 on failure.
int GetSeqLength(const CBioseq& bioseq) 
{
    int len = 0;

    if (bioseq.GetInst().IsSetLength()) {
		len = bioseq.GetInst().GetLength();
	} else {
        const CSeq_data & pDat = bioseq.GetInst().GetSeq_data();

        if (pDat.IsNcbieaa()) {
			len = pDat.GetNcbieaa().Get().size();
		} else if (pDat.IsIupacaa()) {
			len = pDat.GetIupacaa().Get().size();
		} else if (pDat.IsNcbistdaa()) {
            len = pDat.GetNcbistdaa().Get().size();
        } else {
			len = -1;
		}
	}
	return len;
}



//  Returns false (length = 0) on failure, empty seq_entry, 
//  or if the seq_entry represents a set of seq_entry objects.
bool GetSeqLength(const CRef< CSeq_entry >& Seq, int& len) 
{
	len = 0;
    if (Seq.Empty() || Seq->IsSet()) return false;
    if (Seq->GetSeq().GetInst().IsSetLength()) {
		len = Seq->GetSeq().GetInst().GetLength();
	} else {
        len = GetSeqLength(Seq->GetSeq());
	}
	return (len != 0);
}




// for converting ncbistdaa sequences to ncbieaa sequences
void  NcbistdaaToNcbieaaString(const std::vector < char >& vec, std::string *str) 
{
    if (str) {
        str->clear();
        str->resize(vec.size());
        try {
            CSeqConvert::Convert(vec, CSeqUtil::e_Ncbistdaa, 0, vec.size(), *str, CSeqUtil::e_Ncbieaa);
        } catch (exception& e) {
            *str = e.what();
        }
    }
}

//  some stuff from cdt_manipcd
//  False if the seq_entry is not a single bioseq.
bool GetNcbieaaString(const CRef< CSeq_entry >& Seq, string & Str) 
{
    if (Seq->IsSeq() && Seq->GetSeq().GetInst().IsSetSeq_data()) {
        const CBioseq& bioseq = Seq->GetSeq();
        return GetNcbieaaString(bioseq, Str);
    }
    return false;
}

bool GetNcbieaaString(const CBioseq& bioseq, string & Str) 
{
    if (bioseq.GetInst().IsSetSeq_data()) {

        const CSeq_data & pDat= bioseq.GetInst().GetSeq_data();

        if (pDat.IsNcbieaa()) Str = pDat.GetNcbieaa().Get();
        else if (pDat.IsIupacaa()) Str = pDat.GetIupacaa().Get();
        else if (pDat.IsNcbistdaa()) {
                const std::vector < char >& vec = pDat.GetNcbistdaa().Get();
                NcbistdaaToNcbieaaString(vec, &Str);
        }
        return true;
    }
    return false;
}


//  Return as a raw string whatever found in the bioseq.  Ignore types ncbi8aa
//  and ncbipaa, and all nucleic acid encodings.
string GetRawSequenceString(const CBioseq& bioseq) 
{
    string s = kEmptyStr;
    if (bioseq.GetInst().IsSetSeq_data()) {
        const CSeq_data & pDat= bioseq.GetInst().GetSeq_data();
        if (pDat.IsNcbieaa()) {
			s = pDat.GetNcbieaa().Get();
		} else if (pDat.IsIupacaa()) {
			s = pDat.GetIupacaa().Get();
		} else if (pDat.IsNcbistdaa()) {
            const std::vector < char >& vec = pDat.GetNcbistdaa().Get();
            s.resize(vec.size());
            for (unsigned int i=0; i<vec.size(); i++) {
                s.at(i) = vec[i];
            }
        }
    }
    return s;
}


//   If zeroBased == true, first letter is 0, otherwise number residues from 1.
char GetResidueAtPosition(const CRef< CSeq_entry >& seqEntry, int pos, bool zeroBased) 
{
    if (pos > 0 && seqEntry->IsSeq() && seqEntry->GetSeq().GetInst().IsSetSeq_data()) {
        return GetResidueAtPosition(seqEntry->GetSeq(), pos, zeroBased);
    }
    return 0;
}

//   If zeroBased == true, first letter is 0, otherwise number residues from 1.
char GetResidueAtPosition(const CBioseq& bioseq, int pos, bool zeroBased) 
{

    char residue = 0;
    string str = "";
    if (pos >= 0 && GetNcbieaaString(bioseq, str)) {
        if (zeroBased && pos < str.size()) {
            residue = str[pos];
        } else if (!zeroBased && pos <= str.size() && pos != 0) {
            residue = str[pos-1];
        }
    }
    return residue;
}


bool IsConsensus(const CRef< CSeq_id >& seqId) {
    bool result = false;

    if (seqId.NotEmpty()) {
        if (seqId->IsLocal()) {
            if (seqId->GetLocal().IsStr()) {
                if(seqId->GetLocal().GetStr() == "consensus") {
                    result = true;
                }
            }
        }
    }
    return result;
}

END_SCOPE(cd_utils) // namespace ncbi::objects::

END_NCBI_SCOPE


/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.1  2005/04/19 14:27:18  lanczyck
 * initial version under algo/structure
 *
 *
 * ===========================================================================
 */
