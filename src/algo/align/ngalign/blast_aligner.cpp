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
 * Author:  Nathan Bouk
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbi_system.hpp>
#include <math.h>

#include <algo/align/ngalign/blast_aligner.hpp>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objmgr/scope.hpp>
#include <algo/blast/api/blast_types.hpp>
#include <algo/blast/api/bl2seq.hpp>
#include <algo/blast/api/blast_options_handle.hpp>
#include <algo/blast/api/blast_nucl_options.hpp>
#include <algo/blast/api/disc_nucl_options.hpp>
#include <algo/align/util/score_builder.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objmgr/seq_vector.hpp>

#include <algo/blast/api/local_blast.hpp>
#include <algo/blast/blastinput/blastn_args.hpp>

#include <algo/align/ngalign/sequence_set.hpp>

BEGIN_SCOPE(ncbi)
USING_SCOPE(objects);
USING_SCOPE(blast);



void CBlastArgs::s_CreateBlastArgDescriptions(CArgDescriptions& ArgDesc)
{
    blast::CGenericSearchArgs search_args(false /* not protein */,
                                          false /* not RPS blast */,
                                          true /* show %identity */);
    blast::CFilteringArgs filter_args(false /* not protein */);
    blast::CNuclArgs nucl_args;
    blast::CGappedArgs gapped_args;
    blast::CHspFilteringArgs cull_args;
    blast::CBlastDatabaseArgs blastdb_args;
    blast::CWindowSizeArg window_args;

    search_args.SetArgumentDescriptions(ArgDesc);
    filter_args.SetArgumentDescriptions(ArgDesc);
    nucl_args.SetArgumentDescriptions(ArgDesc);
    gapped_args.SetArgumentDescriptions(ArgDesc);
    cull_args.SetArgumentDescriptions(ArgDesc);
    blastdb_args.SetArgumentDescriptions(ArgDesc);
    window_args.SetArgumentDescriptions(ArgDesc);
}


CRef<CBlastOptionsHandle> CBlastArgs::s_ExtractBlastArgs(CArgs& Args)
{

    blast::CGenericSearchArgs search_args(false /* not protein */,
                                          false /* not RPS blast */,
                                          true /* show %identity */);
    blast::CFilteringArgs filter_args(false /* not protein */);
    blast::CNuclArgs nucl_args;
    blast::CGappedArgs gapped_args;
    blast::CHspFilteringArgs cull_args;
    blast::CBlastDatabaseArgs blastdb_args;
    blast::CWindowSizeArg window_args;

    CRef<CBlastNucleotideOptionsHandle> NucOptions(new CBlastNucleotideOptionsHandle);
    CRef<CBlastOptionsHandle> Options(&*NucOptions);

    //string task_type = args["task"].AsString();
    //if (task_type == "megablast") {
    //    opts->SetTraditionalMegablastDefaults();
    //} else if (task_type == "blastn") {
    //    opts->SetTraditionalBlastnDefaults();
    //} else {
    //    NCBI_THROW(CException, eUnknown,
    //               "unknown task type: " + task_type);
    //}
    NucOptions->SetTraditionalMegablastDefaults();


    search_args.ExtractAlgorithmOptions(Args, Options->SetOptions());
    filter_args.ExtractAlgorithmOptions(Args, Options->SetOptions());
    nucl_args.ExtractAlgorithmOptions(Args, Options->SetOptions());
    gapped_args.ExtractAlgorithmOptions(Args, Options->SetOptions());
    cull_args.ExtractAlgorithmOptions(Args, Options->SetOptions());
    window_args.ExtractAlgorithmOptions(Args, Options->SetOptions());

    //cout << "GetEvalueThreshold: " << Options->GetEvalueThreshold() << endl;
    //cout << "GetEffectiveSearchSpace: " << Options->GetEffectiveSearchSpace() << endl;

    return Options;

}



CRef<CBlastOptionsHandle> CBlastArgs::s_CreateBlastOptions(const string& Params)
{

    vector<string> Tokens;
    Tokens.push_back("megablast");
    x_ParseOptionsString(Params, Tokens);

    CArgDescriptions ArgDesc;
    CBlastArgs::s_CreateBlastArgDescriptions(ArgDesc);

    try {
        auto_ptr<CArgs> Args(ArgDesc.CreateArgs(Tokens.size(), Tokens));

        return CBlastArgs::s_ExtractBlastArgs(*Args);

    } catch(CException& e) {
        string Message;
        ArgDesc.PrintUsage(Message, true);
        ERR_POST(Error << Message);
        ERR_POST(Error << e.ReportAll());
        throw e;
    }

    return CRef<CBlastOptionsHandle>();
}


void CBlastArgs::x_ParseOptionsString(const string& Params, vector<string>& Tokens)
{
    string Delims = " \n\r\t";
    string Quotes = "'\"";

    string Curr;
    bool InQuotes = false;

    Curr.reserve(32);

    for(unsigned int Index = 0; Index < Params.length(); Index++) {
        if(InQuotes) {
            if(Quotes.find(Params[Index]) != NPOS) {
                if(!Curr.empty())
                    Tokens.push_back(Curr);
                Curr.clear();
                InQuotes = false;
            }
            else {
                Curr.push_back(Params[Index]);
            }
        }
        else { // !InQuotes
            if(Delims.find(Params[Index]) != NPOS) {
                if(!Curr.empty())
                    Tokens.push_back(Curr);
                Curr.clear();
            }
            else if(Quotes.find(Params[Index]) != NPOS) {
                if(!Curr.empty())
                    Tokens.push_back(Curr);
                Curr.clear();
                InQuotes = true;
            }
            else {
                Curr.push_back(Params[Index]);
            }
        }
    }

}


TAlignResultsRef CBlastAligner::GenerateAlignments(CScope& Scope,
                                                   ISequenceSet* QuerySet,
                                                   ISequenceSet* SubjectSet,
                                                   TAlignResultsRef AccumResults)
{
    TAlignResultsRef Results(new CAlignResultsSet);

    CRef<IQueryFactory> Querys;
    CRef<CLocalDbAdapter> Subjects;

    if(m_Filter  &&  dynamic_cast<CBlastDbSet*>(SubjectSet)) {
        CBlastDbSet* BlastSubjectSet = dynamic_cast<CBlastDbSet*>(SubjectSet);
        BlastSubjectSet->SetSoftFiltering(m_Filter);
    }

    Querys = QuerySet->CreateQueryFactory(Scope, *m_BlastOptions, *AccumResults, m_Threshold);
    Subjects = SubjectSet->CreateLocalDbAdapter(Scope, *m_BlastOptions);

    if(Querys.IsNull()) {
        ERR_POST(Info << "Blast Warning: Empty Query Set");
        return Results;
    }

    ERR_POST(Info << "Running Blast");
    CLocalBlast Blast(Querys, m_BlastOptions, Subjects);

    CRef<CSearchResultSet> BlastResults;
    try {
        BlastResults = Blast.Run();
    } catch(CException& e) {
        ERR_POST(Error << "Blast.Run() error: " << e.ReportAll());
        return Results;
    }

    ITERATE(CSearchResultSet, SetIter, *BlastResults) {
        const CSearchResults& Results = **SetIter;
        TQueryMessages Errors = Results.GetErrors(eBlastSevInfo);
        //ITERATE(TQueryMessages, ErrIter, Errors) {
        //    cerr << "BlastMsg: " << (*ErrIter)->GetMessage() << endl;
        //}
        if(Results.HasErrors()) {
            ERR_POST(Error << "BLAST: " << Results.GetErrorStrings());
        }
        if(Results.HasWarnings()) {
            ERR_POST(Warning << "BLAST: " << Results.GetWarningStrings());
        }
    }

    TSeqPos AlignCount = 0;
    NON_CONST_ITERATE(CSearchResultSet, SetIter, *BlastResults) {
        CSearchResults& Results = **SetIter;
        CConstRef<CSeq_align_set> AlignSet = Results.GetSeqAlign();
        if(!AlignSet->IsEmpty()) {
            ITERATE(CSeq_align_set::Tdata, AlignIter, AlignSet->Get()) {
                CRef<CSeq_align> Align = *AlignIter;
                Align->SetNamedScore(GetName(), 1);
                AlignCount++;
            }
        }
    }

    if(AlignCount == 0) {
        ERR_POST(Warning << "CBlastAligner found no hits this run.");
    }

    Results->Insert(*BlastResults);

    return Results;
}


list<CBlastAligner::TBlastAlignerRef>
CBlastAligner::CreateBlastAligners(list<TBlastOptionsRef>& Options, int Threshold)
{
    list<TBlastAlignerRef> Aligners;
    NON_CONST_ITERATE(list<TBlastOptionsRef>, OptionsIter, Options) {
        TBlastAlignerRef CurrAligner;
        CurrAligner.Reset(new CBlastAligner(**OptionsIter, Threshold));
        Aligners.push_back(CurrAligner);
    }
    return Aligners;
}


list<CBlastAligner::TBlastAlignerRef>
CBlastAligner::CreateBlastAligners(const list<string>& Params, int Threshold)
{
    list<TBlastOptionsRef> BlastOptions;
    ITERATE(list<string>, ParamsIter, Params) {
        TBlastOptionsRef CurrOptions;
        CurrOptions = CBlastArgs::s_CreateBlastOptions(*ParamsIter);
        BlastOptions.push_back(CurrOptions);
    }

    return CBlastAligner::CreateBlastAligners(BlastOptions, Threshold);
}




END_SCOPE(ncbi)
//end
