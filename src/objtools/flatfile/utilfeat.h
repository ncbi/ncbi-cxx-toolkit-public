/* utilfeat.h
 *
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
 * File Name:  utilfeat.h
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 * -----------------
 */

#ifndef _UTILFEAT_
#define _UTILFEAT_

#define ftable 1                        /* Seq-feat type */

#include "loadfeat.h"

BEGIN_NCBI_SCOPE

bool SeqLocHaveFuzz(const objects::CSeq_loc& loc);

char* CpTheQualValue(const TQualVector& qlist, const char *qual);
char* GetTheQualValue(TQualVector& qlist, const char *qual);
bool    DeleteQual(TQualVector& qlist, const char *qual);

Uint1   GetQualValueAa(char* qval, bool checkseq);
bool    GetGenomeInfo(objects::CBioSource& bsp, const char* bptr);
void    MaybeCutGbblockSource(TEntryList& seq_entries);

void MakeLocStrCompatible(std::string& str);
char* location_to_string(const objects::CSeq_loc& loc);

END_NCBI_SCOPE

#endif
