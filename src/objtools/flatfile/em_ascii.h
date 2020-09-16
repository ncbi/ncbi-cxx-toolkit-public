/* em_ascii.h
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
 * File Name:  em_ascii.h
 *
 * Author: Alexey Dobronadezhdin
 *
 * File Description:
 * -----------------

 */

#ifndef EM_ASCII_H
#define EM_ASCII_H

#include <objects/seq/Seq_descr.hpp>

BEGIN_NCBI_SCOPE

typedef std::list<std::string> TStringList;

CRef<objects::CEMBL_block> XMLGetEMBLBlock(ParserPtr pp, char* entry, objects::CMolInfo& mol_info,
                                                       char** gbdiv, objects::CBioSource* bio_src,
                                                       TStringList& dr_ena, TStringList& dr_biosample);

bool EmblAscii(ParserPtr pp);
void fta_build_ena_user_object(objects::CSeq_descr::Tdata& descrs, TStringList& dr_ena,
                                TStringList& dr_biosample,
                                CRef<objects::CUser_object>& dbuop);

END_NCBI_SCOPE
#endif // EM_ASCII_H
