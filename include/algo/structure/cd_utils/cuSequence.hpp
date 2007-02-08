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
END_SCOPE(cd_utils) // namespace ncbi::objects::

END_NCBI_SCOPE


#endif // ALGSEQUENCE_HPP

/* 
 * ===========================================================================
 *
 * $Log$
 * Revision 1.8  2006/11/16 20:21:41  cliu
 * get ncbistd from bioseq
 *
 * Revision 1.7  2006/08/29 18:43:41  cliu
 * no message
 *
 * Revision 1.6  2006/08/09 18:41:24  lanczyck
 * add export macros for ncbi_algo_structure.dll
 *
 * Revision 1.5  2006/08/02 14:04:51  cliu
 * add function GetAccAndVersion
 *
 * Revision 1.4  2005/06/30 23:58:42  lanczyck
 * move a few includes from .cpp to .hpp; add an IsEnvironmentalSeq method
 *
 * Revision 1.3  2005/06/30 17:54:55  lanczyck
 * change return value in GetTaxIdInBioseq to reflect conventions in CTaxon1 class
 *
 * Revision 1.2  2005/06/30 15:38:54  lanczyck
 * add method GetTaxIdInBioseq
 *
 * Revision 1.1  2005/04/19 14:28:01  lanczyck
 * initial version under algo/structure
 *
 *
 * ===========================================================================
 */
