#ifndef VALIDATOR___VALIDATOR_BARCODE__HPP
#define VALIDATOR___VALIDATOR_BARCODE__HPP

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
 *   Validates CSeq_entries and CSeq_submits
 *   .......
 *
 */
#include <corelib/ncbistd.hpp>
#include <corelib/ncbidiag.hpp>
#include <serial/objectinfo.hpp>
#include <serial/serialbase.hpp>
#include <objects/general/Name_std.hpp>
#include <objects/biblio/Author.hpp>
#include <objects/valerr/ValidErrItem.hpp>
#include <objects/valerr/ValidError.hpp>
#include <objects/taxon3/itaxon3.hpp>
#include <objmgr/scope.hpp>

#include <map>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_entry;
class CSeq_entry_Handle;
class CSeq_submit;
class CSeq_annot;
class CSeq_annot_Handle;
class CSeq_feat;
class CBioSource;
class CPubdesc;
class CBioseq;
class CSeqdesc;
class CObjectManager;
class CScope;

BEGIN_SCOPE(validator)

struct SBarcode
{
    CBioseq_Handle bsh;
    string barcode;
    string genbank;
    bool length;
    bool primers;
    bool country;
    bool voucher;
    bool structured_voucher;
    string percent_n;
    bool collection_date;
    bool order_assignment;
    bool low_trace;
    bool frame_shift;
    bool has_keyword;
};

typedef vector<SBarcode> TBarcodeResults;

string GetSeqTitle(CBioseq_Handle bsh);
string GetBarcodeId(CBioseq_Handle bsh);

bool NCBI_VALIDATOR_EXPORT BarcodeTestFails(const SBarcode& b);
TBarcodeResults NCBI_VALIDATOR_EXPORT GetBarcodeValues(CSeq_entry_Handle seh);


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* VALIDATOR___VALIDATOR_BARCODE__HPP */
