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
 *   validation of seq_desc
 *   .......
 *
 */
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistr.hpp>
#include "validatorp.hpp"

#include <objects/general/User_object.hpp>
#include <objects/general/Object_id.hpp>

#include <objects/seq/Seqdesc.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(validator)


CValidError_desc::CValidError_desc(CValidError_imp& imp) :
    CValidError_base(imp)
{
}


CValidError_desc::~CValidError_desc(void)
{
}


void CValidError_desc::ValidateSeqDesc(const CSeqdesc& desc)
{
    // switch on type, e.g., call ValidateBioSource or ValidatePubdesc
    switch (desc.Which ()) {
        case CSeqdesc::e_not_set:
            break;
        case CSeqdesc::e_Mol_type:
            break;
        case CSeqdesc::e_Modif:
            break;
        case CSeqdesc::e_Method:
            break;
        case CSeqdesc::e_Name:
            break;
        case CSeqdesc::e_Title:
            break;
        case CSeqdesc::e_Org:
            break;
        case CSeqdesc::e_Comment:
            break;
        case CSeqdesc::e_Num:
            break;
        case CSeqdesc::e_Maploc:
            break;
        case CSeqdesc::e_Pir:
            break;
        case CSeqdesc::e_Genbank:
            break;
        case CSeqdesc::e_Pub:
        {
            const CPubdesc& pub = desc.GetPub ();
            m_Imp.ValidatePubdesc (pub, desc);
            break;
        }
        case CSeqdesc::e_Region:
            break;
        case CSeqdesc::e_User:
        {
            const CUser_object& usr = desc.GetUser ();
            const CObject_id& oi = usr.GetType();
            if (!oi.IsStr()) {
                break;
            }
            if (!NStr::CompareNocase(oi.GetStr(), "TpaAssembly")) {
                try {
                    PostErr(eDiag_Error, eErr_SEQ_DESCR_InvalidForType,
                        "Non-TPA record should not have TpaAssembly object", desc);
                } catch (const runtime_error&) {
                    return;
                }
            }
            break;
        }
        case CSeqdesc::e_Sp:
            break;
        case CSeqdesc::e_Dbxref:
            break;
        case CSeqdesc::e_Embl:
            break;
        case CSeqdesc::e_Create_date:
            break;
        case CSeqdesc::e_Update_date:
            break;
        case CSeqdesc::e_Prf:
            break;
        case CSeqdesc::e_Pdb:
            break;
        case CSeqdesc::e_Het:
            break;
        case CSeqdesc::e_Source:
        {
            const CBioSource& src = desc.GetSource ();
            m_Imp.ValidateBioSource (src, desc);
            break;
        }
        case CSeqdesc::e_Molinfo:
            break;
        default:
            break;
    }
}

END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.2  2002/12/24 16:53:35  shomrat
* Changes to include directives
*
* Revision 1.1  2002/12/23 20:16:22  shomrat
* Initial submission after splitting former implementation
*
*
* ===========================================================================
*/
