/*  $Id:
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
 * Author:  Jonathan Kans, Clifford Clausen, Aaron Ucko......
 *
 * File Description:
 *   validation of seq_descr
 *   .......
 *
 */
#include <corelib/ncbistd.hpp>
#include <validatorp.hpp>

#include <serial/iterator.hpp>

#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Seq_descr.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(validator)


CValidError_descr::CValidError_descr(CValidError_imp& imp) :
    CValidError_base(imp)
{
}


CValidError_descr::~CValidError_descr(void)
{
}


void CValidError_descr::ValidateSeqDescr(const CSeq_descr& descr)
{
    int num_sources = 0;
    int num_titles = 0;
    const CSeqdesc* lastsource = 0;
    const CSeqdesc* lasttitle = 0;

    for (CTypeConstIterator <CSeqdesc> dt (descr); dt; ++dt) {
        switch (dt->Which ()) {
            case CSeqdesc::e_Title:
                num_titles++;
                lasttitle = &(*dt);
                break;
            case CSeqdesc::e_Source:
                num_sources++;
                lastsource = &(*dt);
                break;
            default:
                break;
        }
    }
    if ( num_sources > 1 ) {
        PostErr(eDiag_Error, eErr_SEQ_DESCR_MultipleBioSources,
            "Multiple BioSource blocks", *lastsource);
    }
    if ( num_titles > 1 ) {
        PostErr(eDiag_Error, eErr_SEQ_DESCR_MultipleTitles,
            "Multiple Title blocks", *lasttitle);
    }
}


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.1  2002/12/23 20:16:26  shomrat
* Initial submission after splitting former implementation
*
*
* ===========================================================================
*/
