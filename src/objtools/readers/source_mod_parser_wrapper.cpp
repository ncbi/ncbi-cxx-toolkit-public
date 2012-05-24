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
* Authors:  Michael Kornbluh
*
* File Description:
*   Wraps CSourceModParser calls for CBioseq_Handles and such.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <objtools/readers/source_mod_parser_wrapper.hpp>
#include <objtools/readers/source_mod_parser.hpp>

#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_annot_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objects/misc/sequence_macros.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

// static
void CSourceModParserWrapper::ExtractTitleAndApplyAllMods(CBioseq_Handle& bsh, CTempString organism)
{
    // add subtypes and many other things based on the title
    CSeqdesc_CI title_desc(bsh, CSeqdesc::e_Title);
    if( title_desc ) {
        CSourceModParser smp;
        string& title(const_cast<string&>(title_desc->GetTitle()));
        title = smp.ParseTitle(title, bsh.GetInitialSeqIdOrNull() );
        x_ApplyAllMods(smp, bsh, organism);
        smp.GetLabel(&title, CSourceModParser::fUnusedMods);
    }

    // need to create update date (or create date if there's no create date )
    {
        CRef<CSeqdesc> new_seqdesc( new CSeqdesc );
        CRef<CDate> todays_date( new CDate(CTime(CTime::eCurrent), CDate::ePrecision_day) );

        CSeqdesc_CI create_date_desc(bsh, CSeqdesc::e_Create_date);
        if( create_date_desc ) {
            // add update date
            new_seqdesc->SetUpdate_date( *todays_date );
        } else {
            // add create date
            new_seqdesc->SetCreate_date( *todays_date );
        }

        CBioseq_EditHandle(bsh).AddSeqdesc( *new_seqdesc );
    }
}

// static 
void CSourceModParserWrapper::x_ApplyAllMods(
    CSourceModParser &smp, CBioseq_Handle& bsh, CTempString organism)
{
    // pull Bioseq out of the object manager so we can edit it
    CSeq_entry_EditHandle seeh = CSeq_entry_EditHandle(bsh.GetParentEntry()); // TODO: what if there's no parent?
    CRef<CBioseq> bioseq(const_cast<CBioseq*>(&*bsh.GetCompleteBioseq()));
    seeh.SelectNone(); // the bioseq is removed from OM

    // call underlying function
    smp.ApplyAllMods( *bioseq, organism );

    // put Bioseq back into the object manager
    bsh = seeh.SelectSeq(*bioseq);
}

END_SCOPE(objects)
END_NCBI_SCOPE
