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

#include <objects/seqloc/PDB_seq_id.hpp>

#include <util/sequtil/sequtil_convert.hpp>

#include <algo/structure/cd_utils/cuSequence.hpp>
#include <objects/seqblock/PDB_block.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/id1/id1_client.hpp>

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

int GetTaxIdInBioseq(const CBioseq& bioseq) {

    bool isTaxIdFound = false;
    int	thisTaxid, taxid =	0;
	list< CRef<	CSeqdesc > >::const_iterator  j, jend;

	if (bioseq.IsSetDescr()) 
	{
		jend = bioseq.GetDescr().Get().end();

		// look	through	the	sequence descriptions
		for	(j=bioseq.GetDescr().Get().begin();	j!=jend; j++) 
		{
			const COrg_ref *org	= NULL;
			if ((*j)->IsOrg())
				org	= &((*j)->GetOrg());
			else if	((*j)->IsSource())
				org	= &((*j)->GetSource().GetOrg());
			if (org) 
			{
				vector < CRef< CDbtag >	>::const_iterator k, kend =	org->GetDb().end();
				for	(k=org->GetDb().begin(); k != kend;	++k) {
					if ((*k)->GetDb() == "taxon") {
						if ((*k)->GetTag().IsId()) {
							thisTaxid = (*k)->GetTag().GetId();

                            //  Mark the first valid tax id found; if there are others, 
                            //  return -(firstTaxid) if they are not all equal.  Allow for
                            //  thisTaxid < 0, which CTaxon1 allows when there are multiple ids.
                            if (isTaxIdFound && taxid != thisTaxid && taxid != -thisTaxid) {
                                taxid *= (taxid > 0) ? -1 : 1;
                            } else if (taxid == 0 && thisTaxid != 0 && !isTaxIdFound) {
                                taxid = (thisTaxid > 0) ? thisTaxid : -thisTaxid;
                                isTaxIdFound = true;
                            } 
//							break;
						}
					}
				}
			} //end	if (org)
		}//end for
	}
	return taxid;
}

bool IsEnvironmentalSeq(const CBioseq& bioseq) {
	//256318 is the taxid for Venter's infamous environmental sequences
	return GetTaxIdInBioseq(bioseq) == ENVIRONMENTAL_SEQUENCE_TAX_ID;
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
	} else if (bioseq.GetInst().IsSetSeq_data()) {
        const CSeq_data & pDat = bioseq.GetInst().GetSeq_data();

        if (pDat.IsNcbieaa()) {
			len = pDat.GetNcbieaa().Get().size();
		} else if (pDat.IsIupacaa()) {
			len = pDat.GetIupacaa().Get().size();
		} else if (pDat.IsNcbistdaa()) {
            len = pDat.GetNcbistdaa().Get().size();
        } else {
			len = 0;
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
        str->erase();
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

bool GetNcbistdSeq(const CBioseq& bioseq, vector<char>& seqData) 
{
    if (bioseq.GetInst().IsSetSeq_data()) 
	{
        const CSeq_data & pDat= bioseq.GetInst().GetSeq_data();
		if (pDat.IsNcbieaa())
		{
			string Str = pDat.GetNcbieaa().Get();
			try {
				CSeqConvert::Convert(Str, CSeqUtil::e_Ncbieaa, 0, Str.size(), seqData, CSeqUtil::e_Ncbistdaa);
			} catch (...) {
				return false;
			}
		}
		else if (pDat.IsIupacaa()) 
		{
			string Str = pDat.GetIupacaa().Get();
			try {
				CSeqConvert::Convert(Str, CSeqUtil::e_Iupacaa, 0, Str.size(), seqData, CSeqUtil::e_Ncbistdaa);
			} catch (...) {
				return false;
			}
		}
        else if (pDat.IsNcbistdaa()) {
                const std::vector < char >& vec = pDat.GetNcbistdaa().Get();
				seqData.assign(vec.begin(), vec.end());
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
        if (zeroBased && pos < (int) str.size()) {
            residue = str[pos];
        } else if (!zeroBased && pos <= (int) str.size() && pos != 0) {
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

bool GetAccAndVersion(const CRef< CBioseq > bioseq, string& acc, int& version, CRef< CSeq_id>& seqId)
{
	acc.erase();
	const list< CRef< CSeq_id > >& seqIds = bioseq->GetId();
	for (list< CRef< CSeq_id > >::const_iterator cit = seqIds.begin();
		cit != seqIds.end(); cit++)
	{
		const CTextseq_id* textId = (*cit)->GetTextseq_Id();
		if (textId)
		{
			if (textId->CanGetAccession())
				acc = textId->GetAccession();
			if (acc.size() >= 0)
			{
				if (textId->CanGetVersion())
					version = textId->GetVersion();	
				seqId = new CSeq_id;
				seqId->Assign(**cit);
				break;
			}
		}
	}
	return acc.size() != 0;
}


bool GetPDBBlockFromSeqEntry(CRef< CSeq_entry > seqEntry, CRef< CPDB_block >& pdbBlock)
{
	if (seqEntry->IsSeq())
	{
		const list< CRef< CSeqdesc > >& descrList = seqEntry->GetSeq().GetDescr().Get();
		list< CRef< CSeqdesc > >::const_iterator cit = descrList.begin();
		for (; cit != descrList.end(); cit++)
		{
			if ((*cit)->IsPdb())
			{
				CRef< CSeqdesc > desc= *cit;
				pdbBlock.Reset( &(desc->SetPdb()) );
				return true;
			}
		}
	}
	else
	{
		const list< CRef< CSeqdesc > >& descrList = seqEntry->GetSet().GetDescr().Get();
		list< CRef< CSeqdesc > >::const_iterator cit = descrList.begin();
		for (; cit != descrList.end(); cit++)
		{
			if ((*cit)->IsPdb())
			{
				CRef< CSeqdesc > desc= *cit;
				pdbBlock.Reset( &(desc->SetPdb()) );
				return true;
			}
		}
		list< CRef< CSeq_entry > >::const_iterator lsei;
		const list< CRef< CSeq_entry > >& seqEntryList = seqEntry->GetSet().GetSeq_set();  
		for (lsei = seqEntryList.begin(); lsei != seqEntryList.end(); ++lsei) 
		{
			if(GetPDBBlockFromSeqEntry(*lsei, pdbBlock))
				return true;
		} 
	}
	return false;
}

bool checkAndFixPdbBioseq(CRef< CBioseq > bioseq)
{
	const list< CRef< CSeq_id > >& idList = bioseq->GetId();
	list< CRef< CSeq_id > >::const_iterator cit = idList.begin();
	CRef< CSeq_id > pdbId;
	for(; cit != idList.end(); cit++)
		if( (*cit)->IsPdb() )
			pdbId = *cit;
	if (pdbId.Empty())
		return false;
	list< CRef< CSeqdesc > >& descrList = bioseq->SetDescr().Set();
	list< CRef< CSeqdesc > >::const_iterator cdit = descrList.begin();
	for (; cdit != descrList.end(); cdit++)
	{
		if ( (*cdit)->IsTitle() )
			return false;
	}
	CID1Client id1Client;
	id1Client.SetAllowDeadEntries(false);
	CRef<CSeq_entry> seqEntry;
	try {
		seqEntry = id1Client.FetchEntry(*pdbId, 1);
	} catch (...)
	{
		return false;
	}
	CRef< CPDB_block > pdbBlock;
	if (GetPDBBlockFromSeqEntry(seqEntry, pdbBlock))
	{
		CRef< CSeqdesc > seqDesc(new CSeqdesc);
		if (pdbBlock->CanGetCompound())
		{
			const list< string >& compounds = pdbBlock->GetCompound();
			if (compounds.size() != 0)
			{
				seqDesc->SetTitle(*(compounds.begin()));
				descrList.push_back(seqDesc);
				return true;
			}
		}
	}
	return false;
}

bool CopyGiSeqId(const CRef<CBioseq>& bioseq, CRef<CSeq_id>& giSeqId, unsigned int nth)
{
    bool result = false;
    unsigned int ctr = 0;
    CBioseq::TId::const_iterator idCit, idEnd;

    idEnd = bioseq->GetId().end();
    for (idCit = bioseq->GetId().begin(); idCit != idEnd && ctr < nth; ++idCit) {
        if ((*idCit).NotEmpty() && (*idCit)->IsGi()) {

            //  Skip until hit the specified entry in the bioseq.
            ++ctr;
            if (ctr != nth) continue;
            
            giSeqId->Assign(**idCit);
            result = true;
        }
    }
    return result;
}

bool ExtractGi(const CRef<CBioseq>& bioseq, unsigned int& gi, unsigned int nth)
{
    bool result = false;
    CRef< CSeq_id > giSeqId(new CSeq_id());

    gi = 0;
    if (CopyGiSeqId(bioseq, giSeqId, nth)) {
        gi = (unsigned int) giSeqId->GetGi();
        result = true;
    }
    return result;
}

//  Last arg tells which id to use if there are multiple pdbs - which there shouldn't be.
bool CopyPdbSeqId(const CRef<CBioseq>& bioseq, CRef<CSeq_id>& pdbSeqId, unsigned int nth)
{
    bool result = false;
    unsigned int ctr = 0;
    CBioseq::TId::const_iterator idCit, idEnd;

    idEnd = bioseq->GetId().end();
    for (idCit = bioseq->GetId().begin(); idCit != idEnd && ctr < nth; ++idCit) {
        if ((*idCit).NotEmpty() && (*idCit)->IsPdb()) {

            //  Skip until hit the specified entry in the bioseq.
            ++ctr;
            if (ctr != nth) continue;
            
            pdbSeqId->Assign(**idCit);
            result = true;
        }
    }
    return result;
}

//  Last arg tells which id to use if there are multiple pdbs - which there shouldn't be.
bool ExtractPdbMolChain(const CRef<CBioseq>& bioseq, string& pdbMol, string& pdbChain, unsigned int nth)
{
    bool result = false;
    CRef< CSeq_id > pdbSeqId(new CSeq_id());

    pdbMol = "";
    pdbChain = "";
    if (CopyPdbSeqId(bioseq, pdbSeqId, nth)) {
        pdbMol = pdbSeqId->GetPdb().GetMol().Get();
        if (pdbSeqId->GetPdb().IsSetChain()) {
            pdbChain = string(1, pdbSeqId->GetPdb().GetChain());
        }
        result = true;
    }
    return result;
}


unsigned int CopySeqIdsOfType(const CBioseq& bioseq, CSeq_id::E_Choice choice, list< CRef< CSeq_id > >& idsOfType)
{
    CBioseq::TId::const_iterator idCit = bioseq.GetId().begin(), idEnd = bioseq.GetId().end();

    idsOfType.clear();                    
    for (; idCit != idEnd; ++idCit) {
        if ((*idCit)->Which() == choice) {
            CRef< CSeq_id > id(new CSeq_id);
            id->Assign(**idCit);
            idsOfType.push_back(id);
        }
    }
    return idsOfType.size();
}

unsigned int CopySeqIdsOfType(const CRef< CSeq_entry >& seqEntry, CSeq_id::E_Choice choice, list< CRef< CSeq_id > >& idsOfType)
{
    list< CRef< CSeq_id > > tmpList;
    CBioseq_set::TSeq_set::const_iterator bssCit, bssEnd;

    idsOfType.clear();
    if (seqEntry.NotEmpty()) {
        if (seqEntry->IsSet()) {
            bssCit = seqEntry->GetSet().GetSeq_set().begin();
            bssEnd = seqEntry->GetSet().GetSeq_set().end();
            for (; bssCit != bssEnd; ++bssCit) {
                if ((*bssCit)->IsSeq()) {
                    tmpList.clear();
                    if (CopySeqIdsOfType((*bssCit)->GetSeq(), choice, tmpList) > 0) {
                        idsOfType.insert(idsOfType.end(), tmpList.begin(), tmpList.end());
                    }
                }
            }
        } else if (seqEntry->IsSeq()) {
            CopySeqIdsOfType(seqEntry->GetSeq(), choice, idsOfType);
        }
    }
    return idsOfType.size();
}


bool CopyBioseqWithType(const CRef< CSeq_entry >& seqEntry, CSeq_id::E_Choice choice, CRef< CBioseq >& seqEntryBioseq) 
{
    bool result = false;
    list< CRef< CSeq_id > > tmpList;
    CBioseq_set::TSeq_set::const_iterator bssCit, bssEnd;

    if (seqEntry.NotEmpty()) {
        if (seqEntry->IsSet()) {

            bssCit = seqEntry->GetSet().GetSeq_set().begin();
            bssEnd = seqEntry->GetSet().GetSeq_set().end();
            for (; bssCit != bssEnd && !result; ++bssCit) {
                if ((*bssCit)->IsSeq()) {
                    tmpList.clear();
                    if (CopySeqIdsOfType((*bssCit)->GetSeq(), choice, tmpList) > 0) {
                        seqEntryBioseq->Assign((*bssCit)->GetSeq());
                        result = true;
                    }
                }
            }

        } else if (seqEntry->IsSeq()) {
            if (CopySeqIdsOfType(seqEntry->GetSeq(), choice, tmpList) > 0) {
                seqEntryBioseq->Assign(seqEntry->GetSeq());
                result = true;
            }
        }
    }

    return result;
}

bool GetBioseqWithType(CRef< CSeq_entry >& seqEntry, CSeq_id::E_Choice choice, CRef< CBioseq >& seqEntryBioseq) 
{
    bool result = false;
    list< CRef< CSeq_id > > tmpList;
    CBioseq_set::TSeq_set::iterator bssIt, bssEnd;

    if (seqEntry.NotEmpty()) {
        if (seqEntry->IsSet()) {

            bssIt = seqEntry->SetSet().SetSeq_set().begin();
            bssEnd = seqEntry->SetSet().SetSeq_set().end();
            for (; bssIt != bssEnd && !result; ++bssIt) {
                if ((*bssIt)->IsSeq()) {
                    tmpList.clear();
                    if (CopySeqIdsOfType((*bssIt)->GetSeq(), choice, tmpList) > 0) {
                        seqEntryBioseq = &(*bssIt)->SetSeq();
                        result = true;
                    }
                }
            }

        } else if (seqEntry->IsSeq()) {
            if (CopySeqIdsOfType(seqEntry->GetSeq(), choice, tmpList) > 0) {
                seqEntryBioseq = &(seqEntry->SetSeq());
                result = true;
            }
        }
    }

    return result;
}

END_SCOPE(cd_utils) // namespace ncbi::objects::
END_NCBI_SCOPE
