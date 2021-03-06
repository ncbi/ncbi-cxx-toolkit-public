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
 * Author:  .......
 *
 * File Description:
 *   .......
 *
 * Remark:
 *   This code was originally generated by application DATATOOL
 *   using specifications from the ASN data definition file
 *   'biblio.asn'.
 */

// standard includes

// generated includes
#include <ncbi_pch.hpp>
#include <objects/biblio/Cit_book.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

// destructor
CCit_book::~CCit_book(void)
{
}


bool CCit_book::GetLabelV1(string* label, TLabelFlags flags) const
{
    return x_GetLabelV1(label, (flags & fLabel_Unique) != 0, &GetAuthors(),
                        &GetImp(), &GetTitle(), this, 0);
}


// Based on FormatCitJour from the C Toolkit's api/asn2gnb5.c.
bool CCit_book::GetLabelV2(string* label, TLabelFlags flags) const
{
    const CImprint& imp = GetImp();

    MaybeAddSpace(label);
    string title = GetTitle().GetTitle();
    *label += "(in) " + NStr::ToUpper(title) + '.';

    if (imp.CanGetPub()) {
        *label += ' ';
        imp.GetPub().GetLabel(label, flags, eLabel_V1); // sic
        // "V1" taken over by MakeAffilStr translation
    }

    string year = GetParenthesizedYear(imp.GetDate());
    if ( !year.empty() ) {
        *label += ' ' + year;
    }

    if (imp.CanGetPrepub()  &&  imp.GetPrepub() == CImprint::ePrepub_in_press) {
        *label += ", In press";
    }

    return true;
}

END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

/* Original file checksum: lines: 61, chars: 1883, CRC32: a9269cf4 */
