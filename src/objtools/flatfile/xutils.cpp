/* xutils.c
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
* File Name:  xutils.c
*
* Author:  Alexey Dobronadezhdin
*
*/

#include <ncbi_pch.hpp>

#include <objects/general/Date_std.hpp>
#include <objects/general/Date.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Seq_literal.hpp>

#ifdef THIS_FILE
#    undef THIS_FILE
#endif
#define THIS_FILE "xutils.cpp"

#include <objtools/flatfile/ftacpp.hpp>
#include "ftaerr.hpp"
#include "xgbparint.h"
#include "xutils.h"

BEGIN_NCBI_SCOPE

/**********************************************************
*
*   NLM_EXTERN bool LIBCALL ISAGappedSeqLoc(slp):
*
*      Looks at a single SeqLoc item. If it has the SeqId
*   of type GENERAL with Dbtag.db == $(seqlitdbtag) and
*   Dbtag.tag.id == 0, then returns TRUE, otherwise
*   returns FALSE.
*
**********************************************************/
static bool XISAGappedSeqLoc(const objects::CSeq_loc& loc)
{
    const objects::CSeq_id* id = loc.GetId();
    if (id == nullptr || !id->IsGeneral() || !id->GetGeneral().IsSetDb() || !id->GetGeneral().IsSetTag())
        return false;

    
    if ((id->GetGeneral().GetDb() == seqlitdbtag || id->GetGeneral().GetDb() == unkseqlitdbtag) && id->GetGeneral().GetTag().GetId() == 0)
        return true;

    return false;
}

/**********************************************************
*
*   NLM_EXTERN DeltaSeqPtr LIBCALL GappedSeqLocsToDeltaSeqs(slp):
*
*      This functions is used only in the case, if ISAGappedSeqLoc()
*   has returned TRUE.
*      Converts SeqLoc set to the sequence of DeltaSeqs.
*   Gbtag'ed SeqLocs it turns into SeqLits with the only "length"
*   element. The regular SeqLocs saves as they are. Returns
*   obtained DeltaSeq.
*
**********************************************************/
void XGappedSeqLocsToDeltaSeqs(const TSeqLocList& locs, TDeltaList& deltas)
{
    ITERATE(TSeqLocList, loc, locs)
    {
        CRef<objects::CDelta_seq> delta(new objects::CDelta_seq);
        if (XISAGappedSeqLoc(*(*loc)))
        {
            const objects::CSeq_interval& interval = (*loc)->GetInt();
            delta->SetLiteral().SetLength(interval.GetTo() - interval.GetFrom() + 1);

            const objects::CSeq_id* id = (*loc)->GetId();
            if (id != nullptr)
            {
                const objects::CDbtag& tag = id->GetGeneral();
                if (tag.GetDb() == unkseqlitdbtag)
                    delta->SetLiteral().SetFuzz().SetLim();
            }
        }
        else
            delta->SetLoc().Assign(*(*loc));

        deltas.push_back(delta);
    }
}

/**********************************************************/
int XDateCheck(const objects::CDate_std& date)
{
    Int2 	day, month, year, last;
    static Uint1 days[12] = { 31, 28, 31, 30, 31,
        30, 31, 31, 30, 31, 30, 31 };

    if (!date.IsSetYear())
        return 3;

    year = date.GetYear();

    if (!date.IsSetMonth())
        return -2;

    month = date.GetMonth();
    if (month > 12)
        return 2;

    if (!date.IsSetDay())
        return -1;

    day = date.GetDay();

    if (month > 12)
        return false;
    last = days[month - 1];

    if ((month == 2) && (year % 4 == 0) && (year != 2000)) {
        last = 29;
    }
    if (day > last) {
        return 1;
    }

    return 0;
}

END_NCBI_SCOPE
