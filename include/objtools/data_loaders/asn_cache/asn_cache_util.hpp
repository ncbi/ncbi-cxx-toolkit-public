#ifndef ___ASN_CACHE_UTIL__HPP
#define ___ASN_CACHE_UTIL__HPP

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
 * Authors:  Mike DiCuccio Cheinan Marks
 *
 * File Description:
 *
 */

#include <corelib/ncbistd.hpp>
#include <objects/seq/seq_id_handle.hpp>

BEGIN_NCBI_SCOPE


void GetNormalizedSeqId(const objects::CSeq_id_Handle& id,
                        string& id_str, Uint4& version);
void GetNormalizedSeqId(const objects::CSeq_id& id,
                        string& id_str, Uint4& version);

void    ReadThroughFile( const std::string & file_path );

CConstRef<objects::CBioseq> ExtractBioseq(CConstRef<objects::CSeq_entry> entry,
                                          const objects::CSeq_id_Handle & id);

END_NCBI_SCOPE


#endif  // ___ASN_CACHE_UTIL__HPP
