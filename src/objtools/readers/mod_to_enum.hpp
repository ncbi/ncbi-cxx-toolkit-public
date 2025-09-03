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
 * Authors:  Justin Foley
 *
 */

#ifndef _MOD_TO_ENUM_HPP_
#define _MOD_TO_ENUM_HPP_

#include <objects/seq/Seq_inst.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seq/MolInfo.hpp>
#include <unordered_map>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

template<typename TEnum>
using TStringToEnumMap = unordered_map<string,TEnum>;

string g_GetNormalizedModVal(const string& unnormalized);


TStringToEnumMap<COrgMod::ESubtype>
g_InitModNameOrgSubtypeMap(void);


TStringToEnumMap<CSubSource::ESubtype>
g_InitModNameSubSrcSubtypeMap(void);


TStringToEnumMap<CBioSource::EGenome>
g_InitModNameGenomeMap(void);


TStringToEnumMap<CBioSource::EOrigin>
g_InitModNameOriginMap(void);


extern const
TStringToEnumMap<CMolInfo::TBiomol>
g_BiomolStringToEnum;


extern const
TStringToEnumMap<CSeq_inst::EMol>
g_BiomolStringToInstMolEnum;

END_SCOPE(objects)
END_NCBI_SCOPE

#endif // _MOD_TO_ENUM_HPP_
