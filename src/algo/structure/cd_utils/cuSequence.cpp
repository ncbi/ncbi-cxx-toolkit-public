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
#include <objects/seqblock/PDB_block.hpp>
#include <objects/seqloc/Giimport_id.hpp>
#include <objects/seqloc/Patent_seq_id.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/id1/id1_client.hpp>

#include <util/sequtil/sequtil_convert.hpp>

#include <algo/structure/cd_utils/cuSequence.hpp>
#include <algo/structure/cd_utils/cuDbPriority.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)


bool SeqIdsMatch(const CRef< CSeq_id >& ID1, const CRef< CSeq_id >& ID2) {
//-------------------------------------------------------------------------
// return true if the 2 sequence-id's match
//-------------------------------------------------------------------------
    if (ID1.Empty() || ID2.Empty()) {
        return false;
    }
    return ID1->Match(*ID2);
}

bool SeqIdHasMatchInBioseq(const CRef< CSeq_id>& id, const CBioseq& bioseq)
{
//-------------------------------------------------------------------------
// return true if 'id' matches at least one element in bioseq's id list
//-------------------------------------------------------------------------
    if (id.Empty()) return false;

    bool result = false;
    const CBioseq::TId& bioseqIds = bioseq.GetId();
    CBioseq::TId::const_iterator cit = bioseqIds.begin(), cend = bioseqIds.end();
    for (; cit != cend && !result; ++cit) {
        result = SeqIdsMatch(id, *cit);
    }
    return result;
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

TTaxId GetTaxIdInBioseq(const CBioseq& bioseq) {

    bool isTaxIdFound = false;
    TTaxId	thisTaxid, taxid =	ZERO_TAX_ID;
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
							thisTaxid = TAX_ID_FROM(CObject_id::TId, (*k)->GetTag().GetId());

                            //  Mark the first valid tax id found; if there are others, 
                            //  return -(firstTaxid) if they are not all equal.  Allow for
                            //  thisTaxid < 0, which CTaxon1 allows when there are multiple ids.
                            if (isTaxIdFound && taxid != thisTaxid && taxid != -thisTaxid) {
                                if (taxid > ZERO_TAX_ID) taxid = -taxid;
                            } else if (taxid == ZERO_TAX_ID && thisTaxid != ZERO_TAX_ID && !isTaxIdFound) {
                                taxid = (thisTaxid > ZERO_TAX_ID) ? thisTaxid : -thisTaxid;
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

// for converting ncbieaa sequences to ncbistdaa sequences
bool NcbieaaToNcbistdaaString(const std::string& str, vector < char >& vec)
{
    bool result = true;
    vec.clear();
    if (str.size() > 0) {
        vec.reserve(str.size());
        try {
            CSeqConvert::Convert(str, CSeqUtil::e_Ncbieaa, 0, str.size(), vec, CSeqUtil::e_Ncbistdaa);
        } catch (...) {
            result = false;
        }
    }
    return result;
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
    //  copied from algo/blast/core/blast_encoding.h
    //  usage copied as per line 2190 in objtools/blast_format/blastfmtutil.cpp
    static const char MY_NCBISTDAA_TO_AMINOACID[28] = {
    '-','A','B','C','D','E','F','G','H','I','K','L','M',
    'N','P','Q','R','S','T','V','W','X','Y','Z','U','*',
    'O', 'J'};


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
                //  simply doing s.at(i) = vec[i] didn't work
                s.at(i) = MY_NCBISTDAA_TO_AMINOACID[(int)vec[i]];
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
			if (acc.size() > 0)
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

bool ExtractGi(const CRef<CBioseq>& bioseq, TGi& gi, unsigned int nth)
{
    bool result = false;
    CRef< CSeq_id > giSeqId(new CSeq_id());

    gi = ZERO_GI;
    if (CopyGiSeqId(bioseq, giSeqId, nth)) {
        gi = giSeqId->GetGi();
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
#ifdef _STRUCTURE_USE_LONG_PDB_CHAINS_
        pdbChain = pdbSeqId->GetPdb().GetEffectiveChain_id();
#else            
        if (pdbSeqId->GetPdb().IsSetChain()) {
            pdbChain = string(1, pdbSeqId->GetPdb().GetChain());
        }
#endif            
        result = true;
    }
    return result;
}

bool HasSeqIdOfType(const CBioseq& bioseq, CSeq_id::E_Choice choice)
{
    bool result = false;
    CBioseq::TId::const_iterator idCit = bioseq.GetId().begin(), idEnd = bioseq.GetId().end();

    for (; idCit != idEnd && !result; ++idCit) {
        if ((*idCit)->Which() == choice) {
            result = true;
        }
    }
    return result;
}

bool HasSeqIdOfType(const CRef< CSeq_entry >& seqEntry, CSeq_id::E_Choice choice)
{
    bool result = false;
    CBioseq_set::TSeq_set::const_iterator bssCit, bssEnd;

    if (seqEntry.NotEmpty()) {
        if (seqEntry->IsSet()) {
            bssCit = seqEntry->GetSet().GetSeq_set().begin();
            bssEnd = seqEntry->GetSet().GetSeq_set().end();
            for (; bssCit != bssEnd && !result; ++bssCit) {
                if ((*bssCit)->IsSeq()) {
                    result = HasSeqIdOfType((*bssCit)->GetSeq(), choice);
                } else if ((*bssCit)->IsSet()) {
                    result = HasSeqIdOfType(*bssCit, choice);  // recursive
                }
            }
        } else if (seqEntry->IsSeq()) {
            result = HasSeqIdOfType(seqEntry->GetSeq(), choice);
        }
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
                tmpList.clear();
                if ((*bssCit)->IsSeq()) {
                    if (CopySeqIdsOfType((*bssCit)->GetSeq(), choice, tmpList) > 0) {
                        idsOfType.insert(idsOfType.end(), tmpList.begin(), tmpList.end());
                    }
                } else if ((*bssCit)->IsSet()) {  //recursive
                    if (CopySeqIdsOfType(*bssCit, choice, tmpList) > 0) {
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

bool AddCommentToBioseq(CBioseq& bioseq, const string& comment)
{
    bool result = (bioseq.IsSetDescr() && comment.length() > 0);

    if (result) {
        CSeq_descr& seqDescr = bioseq.SetDescr();
        CRef< CSeqdesc> seqdescComment(new CSeqdesc);
        seqdescComment->SetComment(comment);
        seqDescr.Set().push_back(seqdescComment);
    }

    return result;
}

void SimplifyBioseqForCD(CBioseq& bioseq, const vector<string>& keptComments, bool keepPDBBlock)
{
    bool hasSource = false;
    bool hasTitle  = false;
    string newTitle = kEmptyStr;
    CSeq_descr& seqDescr = bioseq.SetDescr();

    if (seqDescr.IsSet()) {
        list< CRef< CSeqdesc > >& descrList = seqDescr.Set();
        list< CRef< CSeqdesc > >::iterator it = descrList.begin();
        
        //  See if we have a title present...
        while (!hasTitle && it != descrList.end()) {
            hasTitle = ((*it)->IsTitle());
            ++it;
        }

        //  Can't pre-compute descrList.end() since descrList may change within the while loop.
        it = descrList.begin();
        while (it != descrList.end()) {
            //only keep one source field
            if ((*it)->IsSource() && (!hasSource)) {
                hasSource = true;
                it++;
            } else if ((*it)->IsTitle()) {
                it++;
            } else if ((*it)->IsComment() && find(keptComments.begin(), keptComments.end(), (*it)->GetComment()) != keptComments.end()) {
                it++;
            } else if ((*it)->IsPdb()) {

                //  If there is no title, create one from the PDB-Block 'compound' if possible.
                const CPDB_block& pdbBlock = (*it)->GetPdb();
                if (!hasTitle && pdbBlock.CanGetCompound() && pdbBlock.GetCompound().size() > 0) {
                    newTitle = pdbBlock.GetCompound().front();
                    if (newTitle.length() > 0) {
                        CRef< CSeqdesc > addedTitle(new CSeqdesc);
                        addedTitle->SetTitle(newTitle);
                        descrList.push_back(addedTitle);
                        hasTitle = true;
                    }
                }
                if (keepPDBBlock) {
                    it++;
                } else {
                    it = descrList.erase(it);
                }
            } else {
                it = descrList.erase(it);
            }
        }
    }

    // reset annot field
    bioseq.ResetAnnot();   
}

void SimplifySeqEntryForCD(CRef< CSeq_entry >& seqEntry, const vector<string>& keptComments, bool keepPDBBlock)
{
    if (seqEntry.Empty()) return;

    if (seqEntry->IsSeq()) {
        SimplifyBioseqForCD(seqEntry->SetSeq(), keptComments, keepPDBBlock);
    } else if (seqEntry->IsSet()) {
        CBioseq_set::TSeq_set::iterator bssIt = seqEntry->SetSet().SetSeq_set().begin(), bssEnd = seqEntry->SetSet().SetSeq_set().end();
        for (; bssIt != bssEnd; ++bssIt) {
            SimplifySeqEntryForCD(*bssIt, keptComments, keepPDBBlock);
//            if ((*bssIt)->IsSeq()) {
//                SimplifyBioseqForCD((*bssIt)->SetSeq(), keptComment, keepPDBBlock);
//            }
        }
    }
}


string GetDbSourceForSeqId(const CRef< CSeq_id >& seqID)
{
    string acc, dbSource;
    GetAccessionAndDatabaseSource(seqID, acc, dbSource, false);
    return dbSource;
}

string GetAccessionForSeqId(const CRef< CSeq_id >& seqID)
{
    string acc, dbSource;
    GetAccessionAndDatabaseSource(seqID, acc, dbSource);
    return acc;
}


void GetAccessionAndDatabaseSource(const CRef< CSeq_id >& seqID, string& accession, string& dbSource, bool getGenericSource)
{
    dbSource = CCdDbPriority::GetSourceName(CCdDbPriority::eDPUnknown);
    accession = "unknown";
    if (!seqID) {
        return;
    }

    //  Only getting the generic source at this point.
    dbSource = CCdDbPriority::SeqIdTypeToSource((unsigned int) seqID->Which());

	if (seqID->IsGi()) {
        accession = NStr::NumericToString(seqID->GetGi());
	} 
    else if (seqID->IsPdb()) {
		const CPDB_seq_id& pPDB_ID = seqID->GetPdb();
#ifdef _STRUCTURE_USE_LONG_PDB_CHAINS_
        accession =  pPDB_ID.GetMol().Get() + " " + pPDB_ID.GetEffectiveChain_id();
#else
        accession =  pPDB_ID.GetMol().Get() + " " + (char) pPDB_ID.GetChain();
#endif
	}
	else if (seqID->IsLocal()) {
		const CObject_id& pLocal = seqID->GetLocal();
		if (pLocal.IsId()) {
            accession = NStr::IntToString(pLocal.GetId());
		}
		else if (pLocal.IsStr()) {
			accession = pLocal.GetStr();
		}
	}
	else if (seqID->IsGeneral()) {
		const CDbtag& pGeneral = seqID->GetGeneral();
		if (pGeneral.IsSetDb() && !getGenericSource) {  //  look for a specific type only if requested
            dbSource = dbSource + ": " + pGeneral.GetDb();
        }
		if (pGeneral.IsSetTag()) {
			if (pGeneral.GetTag().IsId()) {
                accession = NStr::IntToString(pGeneral.GetTag().GetId());
			}
			else if (pGeneral.GetTag().IsStr()) {
				accession = pGeneral.GetTag().GetStr();
			}
		}
	}
    //  Four unexpected Seq-id types added for completeness
    else if (seqID->IsGibbsq()) {
        accession = NStr::IntToString(seqID->GetGibbsq());
    }
    else if (seqID->IsGibbmt()) {
        accession = NStr::IntToString(seqID->GetGibbmt());
    }
    else if (seqID->IsGiim()) {
        if (seqID->GetGiim().CanGetDb()) {
            dbSource = seqID->GetGiim().GetDb();
        }
        accession = NStr::IntToString(seqID->GetGiim().GetId());
    }
    else if (seqID->IsPatent()) {
        accession = NStr::IntToString(seqID->GetPatent().GetSeqid());
    }

    //  The rest have a CTextseq_id type....
    else {
		const CTextseq_id* textseqId = seqID->GetTextseq_Id();
        if (!textseqId) return;

        //  Report the 'accession' field as the accession.
        //  If the accession field is not set, use the 'name' field if available, as some
        //  types (PRF, PIR) use the 'name' field in Entrez Genpept reports as an accession.
        string tidName = (textseqId->CanGetName()) ? textseqId->GetName() : "";
        accession = (textseqId->CanGetAccession()) ? textseqId->GetAccession() : tidName;

    }

    //  Use a specific source based on accession when requested...
    //  (as a special case, general IDs have the specific source set above)
    if (!getGenericSource && !seqID->IsGeneral()) {
        dbSource = CCdDbPriority::SeqIdTypeToSource((unsigned int) seqID->Which(), accession);
    }
}

bool extractBioseqInfo(const CRef< CBioseq > bioseq, BioseqInfo& info)
{
	info.acession.erase();
	const list< CRef< CSeq_id > >& seqIds = bioseq->GetId();
	for (list< CRef< CSeq_id > >::const_iterator cit = seqIds.begin();
		cit != seqIds.end(); cit++)
	{
		const CTextseq_id* textId = (*cit)->GetTextseq_Id();
		if (textId)
		{
			if (textId->CanGetAccession())
				info.acession = textId->GetAccession();
			if (info.acession.size() > 0)
			{
				if (textId->CanGetVersion())
					info.version = textId->GetVersion();	
				info.dbsource = (*cit)->Which();
				break;
			}
		}
	}
	if (bioseq->IsSetDescr()) 
	{
		list< CRef< CSeqdesc > >::const_iterator  dit;
        // look through the sequence descriptions
        for (dit=bioseq->GetDescr().Get().begin();
			dit!=bioseq->GetDescr().Get().end(); dit++) 
		{
            // if there's a title, return that description
			if ((*dit)->IsTitle()) 
				info.defline = ((*dit)->GetTitle());
        }
	}
	return !info.acession.empty();
}

END_SCOPE(cd_utils) // namespace ncbi::objects::
END_NCBI_SCOPE
