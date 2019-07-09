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
 *       Various utility functions for working with CD objects
 *
 * ===========================================================================
 */

#ifndef CU_UTILS_HPP
#define CU_UTILS_HPP

#include <corelib/ncbiargs.hpp>   
#include <corelib/ncbienv.hpp>
#include <corelib/ncbistre.hpp>
#include <algo/structure/cd_utils/cuCdCore.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)

extern const int  CDTreeColorCycle[];
extern const int  kNumColorsInCDTreeColorCycle;

NCBI_CDUTILS_EXPORT 
string GetSeqIDStr(const CRef< CSeq_id >& SeqID);

// uses a space to delimit molecule and chain ids
NCBI_CDUTILS_EXPORT 
string Make_SeqID_String(const CRef< CSeq_id > SeqID, bool Pad, int Len);

// uses an underscore to delimit molecule and chain ids
NCBI_CDUTILS_EXPORT 
void Make_GI_or_PDB_String_CN3D(const CRef< CSeq_id > SeqID, std::string& Str);

NCBI_CDUTILS_EXPORT 
string CddIdString(const CCdd& cdd);    //  give all Cdd_id's for this Cd
NCBI_CDUTILS_EXPORT 
string CddIdString(const CCdd_id& id);
NCBI_CDUTILS_EXPORT 
bool   SameCDAccession(const CCdd_id& id1, const CCdd_id& id2);

NCBI_CDUTILS_EXPORT 
bool Prosite2Regex(const std::string& prosite, std::string* regex, std::string* errString);

END_SCOPE(cd_utils)
END_NCBI_SCOPE


#endif // CU_UTILS_HPP
