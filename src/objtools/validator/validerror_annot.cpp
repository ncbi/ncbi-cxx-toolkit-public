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
 * Author:  Jonathan Kans, Clifford Clausen, Aaron Ucko......
 *
 * File Description:
 *   validation of seq_annot
 *   .......
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include "validatorp.hpp"

#include <objects/general/User_object.hpp>
#include <objects/general/Object_id.hpp>

#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seq/Annot_descr.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(validator)


CValidError_annot::CValidError_annot(CValidError_imp& imp) :
    CValidError_base(imp)
{
}


CValidError_annot::~CValidError_annot(void)
{
}


void CValidError_annot::ValidateSeqAnnot(const CSeq_annot& annot)
{
    if ( !annot.GetData().IsAlign() ) return;
    if ( !annot.IsSetDesc() ) return;
    
    ITERATE( list< CRef< CAnnotdesc > >, iter, annot.GetDesc().Get() ) {
        
        if ( (*iter)->IsUser() ) {
            const CObject_id& oid = (*iter)->GetUser().GetType();
            if ( oid.IsStr() ) {
                if ( oid.GetStr() == "Blast Type" ) {
                    PostErr(eDiag_Error, eErr_SEQ_ALIGN_BlastAligns,
                        "Record contains BLAST alignments", annot); // !!!
                    
                    break;
                }
            }
        }
    } // iterate
}



END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.6  2004/05/21 21:42:56  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.5  2003/04/24 16:16:00  vasilche
* Added missing includes and forward class declarations.
*
* Revision 1.4  2003/03/31 14:40:49  shomrat
* $id: -> $id$
*
* Revision 1.3  2003/03/11 16:04:09  kuznets
* iterate -> ITERATE
*
* Revision 1.2  2002/12/24 16:52:53  shomrat
* Changes to include directives
*
* Revision 1.1  2002/12/23 20:16:04  shomrat
* Initial submission after splitting former implementation
*
*
* ===========================================================================
*/
