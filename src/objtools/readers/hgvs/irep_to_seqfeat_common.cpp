#include <ncbi_pch.hpp>
#include <objects/seqfeat/Delta_item.hpp>
#include <objects/general/Int_fuzz.hpp>
#include <objtools/readers/hgvs/irep_to_seqfeat_common.hpp>
#include <objtools/readers/hgvs/irep_to_seqfeat_errors.hpp>

BEGIN_NCBI_SCOPE
//BEGIN_objects_SCOPE
USING_SCOPE(objects);

CRef<CDelta_item> CDeltaHelper::CreateSSR(const CCount& count, 
        CRef<CSeq_literal> seq_literal,
        CVariationIrepMessageListener& listener) 
{
    auto delta = Ref(new CDelta_item());
    if (seq_literal.IsNull()) {
        delta->SetSeq().SetThis();
    } 
    else {
        delta->SetSeq().SetLiteral(*seq_literal);
    }

    if ( count.IsRange() ) {
        const auto& range = count.GetRange();
        if ( !range.IsSetStart() || 
             !range.IsSetStop()  ||
             (range.GetStart().IsUnknown() && range.GetStop().IsUnknown()) 
             )
        {
            NCBI_THROW(CVariationIrepException, eInvalidCount, "Unknown range");
        }

        const auto& start = range.GetStart();
        const auto& stop = range.GetStop();

        if ( start.IsUnknown() ) {
            delta->SetMultiplier_fuzz().SetLim(CInt_fuzz::eLim_lt);
            delta->SetMultiplier() = stop.GetVal();
        } 
        else if ( stop.IsUnknown() ) {
            delta->SetMultiplier_fuzz().SetLim(CInt_fuzz::eLim_gt);
            delta->SetMultiplier() = start.GetVal();
        } else {
            auto min = start.GetVal();
            auto max = stop.GetVal();

            if (max < min) { 
                string err_string = "Reversed range limits";
                CVariationIrepMessage msg(err_string, eDiag_Warning);
                msg.SetRange(count.GetRange());
                listener.Post(msg);
                const auto temp = min;
                min = max;
                max = temp;
            }

            delta->SetMultiplier() = min;
            delta->SetMultiplier_fuzz().SetRange().SetMin(min);
            delta->SetMultiplier_fuzz().SetRange().SetMax(max);
        }
    } 
    else { // integer or fuzzy integer
        TSignedSeqPos multiplier = 1;
        if (count.IsVal()) {
            multiplier = count.GetVal();
        } 
        else if (count.IsFuzzy_val()) {
            multiplier = count.GetFuzzy_val();
            delta->SetMultiplier_fuzz().SetLim(CInt_fuzz::eLim_unk); // unknown limit
        }
        if ( multiplier < 1 ) {
            NCBI_THROW(CVariationIrepException, eInvalidCount, "Multiplier < 1");
        }
        delta->SetMultiplier() = multiplier;
    }
    return delta;
}


//END_objects_SCOPE
END_NCBI_SCOPE

