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
 * Author:  Mati Shomrat
 *
 * File Description:
 *   
 */   
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiexpt.hpp>

#include <util/sequtil/sequtil.hpp>
#include <util/sequtil/sequtil_expt.hpp>


BEGIN_NCBI_SCOPE

const size_t CSeqUtil::kNumCodings = 10;


CSeqUtil::ECodingType CSeqUtil::GetCodingType(TCoding coding)
{
    switch ( coding )
        {
        case e_Iupacna:
        case e_Ncbi2na:
        case e_Ncbi2na_expand:
        case e_Ncbi4na:
        case e_Ncbi4na_expand:
        case e_Ncbi8na:
            return e_CodingType_Na;
        
        case e_Iupacaa:
        case e_Ncbi8aa:
        case e_Ncbieaa:
        case e_Ncbistdaa:
            return e_CodingType_Aa;

        case e_not_set:
            break;
        }

    NCBI_THROW(CSeqUtilException, eBadParameter, "");
}



END_NCBI_SCOPE


 /*
* ===========================================================================
*
* $Log$
* Revision 1.3  2004/08/11 15:24:31  vakatov
* Compilation warning fix (unhandled variant in switch, e_not_set)
*
* Revision 1.2  2004/05/17 21:08:53  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.1  2003/10/08 13:34:10  shomrat
* Initial version
*
*
* ===========================================================================
*/
