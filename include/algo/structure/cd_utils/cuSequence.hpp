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
 * Author:  Adapted from CDTree1 code by Chris Lanczycki
 *
 * File Description:
 *
 *       Functions for manipulating Bioseqs and other sequence representations
 *
 * ===========================================================================
 */

#ifndef CU_SEQUENCE_HPP
#define CU_SEQUENCE_HPP

// include ncbistd.hpp, ncbiobj.hpp, ncbi_limits.h, various stl containers
#include <corelib/ncbiargs.hpp>   
#include <corelib/ncbienv.hpp>
#include <corelib/ncbistre.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqblock/PDB_block.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

BEGIN_SCOPE(cd_utils)

// the taxid for environmental sequences
const  int ENVIRONMENTAL_SEQUENCE_TAX_ID = 256318;

NCBI_CDUTILS_EXPORT 
CRef< CSeq_id > CopySeqId(const CRef< CSeq_id >& seqId);

//  Wraps the CSeq_id.Match(id) method:  id1.Match(id2).
NCBI_CDUTILS_EXPORT 
bool SeqIdsMatch(const CRef< CSeq_id>& id1, const CRef< CSeq_id>& id2);  

//   Return 0 if Seq_id is not of proper type (e_General and database 'CDD')
NCBI_CDUTILS_EXPORT 
int  GetCDDPssmIdFromSeqId(const CRef< CSeq_id >& id);

//  Return -1 on failure; was FindMMDBIdInBioseq
NCBI_CDUTILS_EXPORT 
int    GetMMDBId (const CBioseq& bioseq);

//  Consistent w/ CTaxon1 class, return 0 if no tax id was found, 
//  or -(firstTaxId) if multiple tax ids found.
NCBI_CDUTILS_EXPORT 
int  GetTaxIdInBioseq(const CBioseq& bioseq);

NCBI_CDUTILS_EXPORT 
bool IsEnvironmentalSeq(const CBioseq& bioseq);

//  Return species description as a string.
//  Empty string returned on failure; was CCd::GetSpecies(...).
NCBI_CDUTILS_EXPORT 
string GetSpeciesFromBioseq(const CBioseq& bioseq);  

//  length = 0 if detect error condition.
//  Incorporates code from cdt_vutils & cdt_manipcd
NCBI_CDUTILS_EXPORT 
int    GetSeqLength(const CBioseq& bioseq);
NCBI_CDUTILS_EXPORT 
bool   GetSeqLength(const CRef< CSeq_entry >& seqEntry, int& len); 

NCBI_CDUTILS_EXPORT 
void   NcbistdaaToNcbieaaString(const vector< char >& vec, string* str);  //  StringFromStdaa(...)
NCBI_CDUTILS_EXPORT 
bool   GetNcbieaaString(const CBioseq& bioseq, string& str);
NCBI_CDUTILS_EXPORT
bool GetNcbistdSeq(const CBioseq& bioseq, vector<char>& seqData);
NCBI_CDUTILS_EXPORT 
bool   GetNcbieaaString(const CRef< CSeq_entry >& seqEntry, string& str);  //  from cdt_manipcd
NCBI_CDUTILS_EXPORT 
string GetRawSequenceString(const CBioseq& bioseq);

//  On failure, returns \0 (i.e., null character)
//  If zeroBased == true, first letter is at index 0, otherwise number residues from 1.
NCBI_CDUTILS_EXPORT 
char   GetResidueAtPosition(const CBioseq& bioseq, int pos, bool zeroBasedPos = true);
NCBI_CDUTILS_EXPORT 
char   GetResidueAtPosition(const CRef< CSeq_entry >& seqEntry, int pos, bool zeroBasedPos = true);

NCBI_CDUTILS_EXPORT 
bool IsConsensus(const CRef< CSeq_id >& seqId);
NCBI_CDUTILS_EXPORT 
bool GetAccAndVersion(const CRef< CBioseq > bioseq, string& acc, int& version, CRef< CSeq_id>& seqId);
NCBI_CDUTILS_EXPORT 
bool GetPDBBlockFromSeqEntry(CRef< CSeq_entry > seqEntry, CRef< CPDB_block >& pdbBlock);
NCBI_CDUTILS_EXPORT 
bool checkAndFixPdbBioseq(CRef< CBioseq > bioseq);

//  Return 'false' if the bioseq doesn't have a gi-typed seq-id.
//  Last arg tells which id to use if there are multiple gis.
NCBI_CDUTILS_EXPORT 
bool ExtractGi(const CRef<CBioseq>& bioseq, unsigned int& gi, unsigned int nth = 1);
NCBI_CDUTILS_EXPORT 
bool CopyGiSeqId(const CRef<CBioseq>& bioseq, CRef<CSeq_id>& giSeqId, unsigned int nth = 1);

//  Return 'false' if the bioseq doesn't have a pdb-typed seq-id.
//  Last arg tells which id to use if there are multiple pdbs.
NCBI_CDUTILS_EXPORT 
bool ExtractPdbMolChain(const CRef<CBioseq>& bioseq, string& pdbMol, string& pdbChain, unsigned int nth = 1);
NCBI_CDUTILS_EXPORT 
bool CopyPdbSeqId(const CRef<CBioseq>& bioseq, CRef<CSeq_id>& pdbSeqId, unsigned int nth = 1);

//  Returns number of ids of the requested type found.
//  Returned CSeq_id objects are copies of those found in the bioseq/seqEntry.
NCBI_CDUTILS_EXPORT 
unsigned int CopySeqIdsOfType(const CBioseq& bioseq, CSeq_id::E_Choice choice, list< CRef< CSeq_id > >& idsOfType);
NCBI_CDUTILS_EXPORT 
unsigned int CopySeqIdsOfType(const CRef< CSeq_entry >& seqEntry, CSeq_id::E_Choice choice, list< CRef< CSeq_id > >& idsOfType);

//  Return 'false' if the seqEntry doesn't have a bioseq containing a seq-id of the requested type.
//  Returned CBioseq object is a copy of that found in the bioseq/seqEntry.
NCBI_CDUTILS_EXPORT 
bool CopyBioseqWithType(const CRef< CSeq_entry >& seqEntry, CSeq_id::E_Choice choice, CRef< CBioseq >& seqEntryBioseq) ;

//  Return 'false' if the seqEntry doesn't have a bioseq containing a seq-id of the requested type.
//  Returned CBioseq object is an editable reference to the one in the CSeq_entry passed in.
NCBI_CDUTILS_EXPORT 
bool GetBioseqWithType(CRef< CSeq_entry >& seqEntry, CSeq_id::E_Choice choice, CRef< CBioseq >& seqEntryBioseq) ;




END_SCOPE(cd_utils) // namespace ncbi::objects::

END_NCBI_SCOPE


#endif // ALGSEQUENCE_HPP
