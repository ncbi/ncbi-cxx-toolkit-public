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
* Author:  Aaron Ucko, NCBI
*          Mati Shomrat
*
* File Description:
*           
*
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/general/Int_fuzz.hpp>
#include <objtools/format/ftable_formatter.hpp>
#include <objmgr/util/sequence.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CFtableFormatter::CFtableFormatter(void) 
{
}


///////////////////////////////////////////////////////////////////////////
//
// REFERENCE

void CFtableFormatter::FormatReference
(const CReferenceItem& ref,
 IFlatTextOStream& text_os)
{
}


///////////////////////////////////////////////////////////////////////////
//
// FEATURES

// Fetures Header

void CFtableFormatter::FormatFeatHeader
(const CFeatHeaderItem& fh,
 IFlatTextOStream& text_os)
{
    const CSeq_id* id = &fh.GetId();
    if ( id->IsGi() ) {
        // !!! Get id for GI (need support from objmgr)
    }

    if ( id != 0 ) {
        list<string> l;
        l.push_back(">Feature " + id->AsFastaString());
        text_os.AddParagraph(l);
    }
}

// Source and "regular" features

void CFtableFormatter::FormatFeature
(const CFeatureItemBase& f,
 IFlatTextOStream& text_os)
{
    list<string> l;
    const CFlatFeature& feat = *f.Format();
    CBioseqContext& bctx = *f.GetContext();

    x_FormatLocation(f.GetLoc(), feat.GetKey(), bctx, l);
    x_FormatQuals(feat.GetQuals(), bctx, l);
    text_os.AddParagraph(l);
}


bool s_IsBetween(const CSeq_loc& loc)
{
    return loc.IsPnt()  &&
           loc.GetPnt().IsSetFuzz()  &&
           loc.GetPnt().GetFuzz().IsLim()  &&
           loc.GetPnt().GetFuzz().GetLim() == CInt_fuzz::eLim_tr;
}


void CFtableFormatter::x_FormatLocation
(const CSeq_loc& loc,
 const string& key,
 CBioseqContext& ctx,
 list<string>& l)
{
    bool need_key = true;
    for (CSeq_loc_CI it(loc); it; ++it) {
        const CSeq_loc& curr = it.GetSeq_loc();
        bool is_between = s_IsBetween(curr);
      
        CSeq_loc_CI::TRange range = it.GetRange();
        TSeqPos start, stop;
        if ( range.IsWhole() ) {
            start = 1;
            stop  = sequence::GetLength(it.GetSeq_loc(), &ctx.GetScope()) + 1;
        } else {
            start = range.GetFrom() + 1;
            stop  = range.GetTo() + 1;
        }
        if ( is_between ) {
            ++stop;
        }
        string left, right;
       
        if ( curr.IsPartialStart(eExtreme_Biological) ) {
            left = '<';
        }
        left += NStr::IntToString(start);
        if ( is_between ) {
            left += '^';
        }
        if ( curr.IsPartialStop(eExtreme_Biological) ) {
            right = '>';
        }
        right += NStr::IntToString(stop);

        string line;
        if ( it.GetStrand() == eNa_strand_minus ) {
            line = right + '\t' + left;
        } else {
            line = left + '\t' + right;
        }
        if ( need_key ) {
            line += '\t' + key;
            need_key = false;
        }
        l.push_back(line);
    }
}


void CFtableFormatter::x_FormatQuals
(const CFlatFeature::TQuals& quals,
 CBioseqContext& ctx,
 list<string>& l)
{
    string line;
    ITERATE (CFlatFeature::TQuals, it, quals) {
        line = "\t\t\t" + (*it)->GetName();
        if ((*it)->GetStyle() != CFormatQual::eEmpty) {
            string value;
            NStr::Replace((*it)->GetValue(), " \b", kEmptyStr, value);
            line += '\t' + value;
        }
        l.push_back(line);
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
