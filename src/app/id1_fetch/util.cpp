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
* Author:  Aaron Ucko
*
* File Description:
*   Utilities shared by Genbank and non-Genbank code.
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2001/09/04 16:20:54  ucko
* Dramatically fleshed out id1_fetch
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include "util.hpp"

#include <objects/seq/IUPACaa.hpp> // temp
#include <objects/seq/IUPACna.hpp> // temp
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/seqport_util.hpp>

// (BEGIN_NCBI_SCOPE must be followed by END_NCBI_SCOPE later in this file)
BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


#ifndef USE_SEQ_VECTOR
string ToASCII(const CSeq_inst& seq_inst)
{
    if (!seq_inst.IsSetSeq_data()) {
        return kEmptyStr;
    }
    const CSeq_data& data = seq_inst.GetSeq_data();
    CSeq_data out;
    unsigned int length = seq_inst.GetLength();
    switch (data.Which()) {
    case CSeq_data::e_Iupacna:
        return data.GetIupacna().Get();
    case CSeq_data::e_Iupacaa:
        return data.GetIupacaa().Get();

    case CSeq_data::e_Ncbi2na:
    case CSeq_data::e_Ncbi4na:
    case CSeq_data::e_Ncbi8na:
    case CSeq_data::e_Ncbipna:
        CSeqportUtil::Convert(data, &out,
                              CSeq_data::e_Iupacna, 0, length);
        return out.GetIupacna().Get();

    case CSeq_data::e_Ncbi8aa:
    case CSeq_data::e_Ncbieaa:
    case CSeq_data::e_Ncbipaa:
    case CSeq_data::e_Ncbistdaa:
        CSeqportUtil::Convert(data, &out,
                              CSeq_data::e_Iupacaa, 0, length);
        return out.GetIupacaa().Get();

    default:
        return kEmptyStr;
    }
}
#endif


// (END_NCBI_SCOPE must be preceded by BEGIN_NCBI_SCOPE)
END_NCBI_SCOPE
