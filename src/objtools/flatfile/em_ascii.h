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
 * File Name: em_ascii.h
 *
 * Author: Alexey Dobronadezhdin
 *
 * File Description:
 *
 */

#ifndef EM_ASCII_H
#define EM_ASCII_H
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include "mapped_input2asn.hpp" 

BEGIN_NCBI_SCOPE


namespace objects {
    class CEMBL_block;
    class CMolInfo;
    class CBioSource;
    class CUser_object;
    class CSeqdesc;
}

class CEmbl2Asn : public CMappedInput2Asn
{
public:
    using CMappedInput2Asn::CMappedInput2Asn; // inherit constructors
    void PostTotals() override;
private:
    CRef<objects::CSeq_entry> xGetEntry() override;
};

using TStringList = std::list<std::string>;

CRef<objects::CEMBL_block> XMLGetEMBLBlock(Parser* pp, const char* entry, objects::CMolInfo& mol_info, string& gbdiv, objects::CBioSource* bio_src, TStringList& dr_ena, TStringList& dr_biosample, TStringList& dr_pubmed);

void fta_build_ena_user_object(list<CRef<objects::CSeqdesc>>& descrs, TStringList& dr_ena, TStringList& dr_biosample, CRef<objects::CUser_object>& dbuop);

END_NCBI_SCOPE
#endif // EM_ASCII_H
