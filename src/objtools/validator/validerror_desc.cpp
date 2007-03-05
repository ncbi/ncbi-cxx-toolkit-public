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
 *   validation of seq_desc
 *   .......
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistr.hpp>
#include "validatorp.hpp"

#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Object_id.hpp>

#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/MolInfo.hpp>


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


/**
 * Validate descriptors as stand alone objects (no context)
 **/
void CValidError_desc::ValidateSeqDesc
(const CSeqdesc& desc,
 const CSeq_entry& ctx)
{
    m_Ctx.Reset(&ctx);

    // switch on type, e.g., call ValidateBioSource, ValidatePubdesc, ...
    switch ( desc.Which() ) {

        case CSeqdesc::e_Mol_type:
        case CSeqdesc::e_Modif:
        case CSeqdesc::e_Method:
            PostErr(eDiag_Info, eErr_SEQ_DESCR_Obsolete,
                desc.SelectionName(desc.Which()) + " is obsolete", *m_Ctx, desc);
            break;

        case CSeqdesc::e_Comment:
            ValidateComment(desc.GetComment(), desc);
            break;

        case CSeqdesc::e_Pub:
            m_Imp.ValidatePubdesc(desc.GetPub(), desc);
            break;

        case CSeqdesc::e_User:
            ValidateUser(desc.GetUser(), desc);
            break;

        case CSeqdesc::e_Source:
            m_Imp.ValidateBioSource (desc.GetSource(), desc);
            break;
        
        case CSeqdesc::e_Molinfo:
            ValidateMolInfo(desc.GetMolinfo(), desc);
            break;

        case CSeqdesc::e_not_set:
            break;
        case CSeqdesc::e_Name:
            break;
        case CSeqdesc::e_Title:
            break;
        case CSeqdesc::e_Org:
            break;
        case CSeqdesc::e_Num:
            break;
        case CSeqdesc::e_Maploc:
            break;
        case CSeqdesc::e_Pir:
            break;
        case CSeqdesc::e_Genbank:
            break;
        case CSeqdesc::e_Region:
            break;
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
        default:
            break;
    }

    m_Ctx.Reset();
}


void CValidError_desc::ValidateComment
(const string& comment,
 const CSeqdesc& desc)
{
    if ( m_Imp.IsSerialNumberInComment(comment) ) {
        PostErr(eDiag_Info, eErr_SEQ_DESCR_SerialInComment,
            "Comment may refer to reference by serial number - "
            "attach reference specific comments to the reference "
            "REMARK instead.", *m_Ctx, desc);
    }
}


void CValidError_desc::ValidateUser
(const CUser_object& usr,
 const CSeqdesc& desc)
{
    if ( !usr.CanGetType() ) {
        return;
    }
    const CObject_id& oi = usr.GetType();
    if ( !oi.IsStr() ) {
        return;
    }
    if ( NStr::CompareNocase(oi.GetStr(), "TpaAssembly") == 0 ) {
        if ( !m_Imp.IsTPA() ) {
            PostErr(eDiag_Error, eErr_SEQ_DESCR_InvalidForType,
                "Non-TPA record should not have TpaAssembly object", *m_Ctx, desc);
        }
    } else if ( NStr::CompareNocase(oi.GetStr(), "RefGeneTracking") == 0 ) {
        bool has_ref_track_status = false;
        ITERATE(CUser_object::TData, field, usr.GetData()) {
            if ( (*field)->CanGetLabel() ) {
                const CObject_id& obj_id = (*field)->GetLabel();
                if ( !obj_id.IsStr() ) {
                    continue;
                }
                if ( NStr::CompareNocase(obj_id.GetStr(), "Status") == 0 ) {
                    has_ref_track_status = true;
                    break;
                }
            }
        }
        if ( !has_ref_track_status ) {
            PostErr(eDiag_Error, eErr_SEQ_DESCR_RefGeneTrackingWithoutStatus,
                "RefGeneTracking object needs to have Status set", *m_Ctx, desc);
        }
    }
}


void CValidError_desc::ValidateMolInfo
(const CMolInfo& minfo,
 const CSeqdesc& desc)
{
    if ( !minfo.IsSetBiomol() ) {
        return;
    }

    int biomol = minfo.GetBiomol();

    switch ( biomol ) {
    case CMolInfo::eBiomol_unknown:
        PostErr(eDiag_Error, eErr_SEQ_DESCR_InvalidForType,
            "Molinfo-biomol unknown used", *m_Ctx, desc);
        break;

    case CMolInfo::eBiomol_other:
        if ( !m_Imp.IsXR() ) {
            PostErr(eDiag_Warning, eErr_SEQ_DESCR_InvalidForType,
                "Molinfo-biomol other used", *m_Ctx, desc);
        }
        break;

    default:
        break;
    }
}


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE
