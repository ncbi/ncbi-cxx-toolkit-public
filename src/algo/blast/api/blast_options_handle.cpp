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
 * Authors:  Christiam Camacho
 *
 */

/// @file blast_options_handle.cpp
/// Implementation for the CBlastOptionsHandle and the
/// CBlastOptionsFactory classes.

#include <ncbi_pch.hpp>
#include <algo/blast/api/blast_exception.hpp>
#include <algo/blast/api/blast_options_handle.hpp>
#include <algo/blast/api/blast_prot_options.hpp>
#include <algo/blast/api/blastx_options.hpp>
#include <algo/blast/api/tblastn_options.hpp>
#include <algo/blast/api/rpstblastn_options.hpp>
#include <algo/blast/api/tblastx_options.hpp>
#include <algo/blast/api/blast_nucl_options.hpp>
#include <algo/blast/api/disc_nucl_options.hpp>
#include <algo/blast/api/blast_rps_options.hpp>
#include <algo/blast/api/blast_advprot_options.hpp>
#include <algo/blast/api/psiblast_options.hpp>
#include <algo/blast/api/phiblast_nucl_options.hpp>
#include <algo/blast/api/phiblast_prot_options.hpp>
#include <sstream>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

CBlastOptionsHandle::CBlastOptionsHandle(EAPILocality locality)
    : m_DefaultsMode(false)
{
    m_Opts.Reset(new CBlastOptions(locality));
}

void
CBlastOptionsHandle::SetDefaults()
{
    if (m_Opts->GetLocality() != CBlastOptions::eRemote) {
        m_Opts->SetDefaultsMode(true);
        SetLookupTableDefaults();
        SetQueryOptionDefaults();
        SetInitialWordOptionsDefaults();
        SetGappedExtensionDefaults();
        SetScoringOptionsDefaults();
        SetHitSavingOptionsDefaults();
        SetEffectiveLengthsOptionsDefaults();
        SetSubjectSequenceOptionsDefaults();
        m_Opts->SetDefaultsMode(false);
    }
    SetRemoteProgramAndService_Blast3();
}

bool
CBlastOptionsHandle::Validate() const
{
    return m_Opts->Validate();
}

CBlastOptionsHandle*
CBlastOptionsFactory::Create(EProgram program, EAPILocality locality)
{
    CBlastOptionsHandle* retval = NULL;

    switch (program) {
    case eBlastn:
	{
		CBlastNucleotideOptionsHandle* opts = 
			new CBlastNucleotideOptionsHandle(locality);
		opts->SetTraditionalBlastnDefaults();
		retval = opts;
        break;
	}

    case eBlastp:
        retval = new CBlastAdvancedProteinOptionsHandle(locality);
        break;

    case eBlastx:
        retval = new CBlastxOptionsHandle(locality);
        break;

    case eTblastn:
        retval = new CTBlastnOptionsHandle(locality);
        break;

    case eTblastx:
        retval = new CTBlastxOptionsHandle(locality);
        break;

    case eMegablast:
	{
		CBlastNucleotideOptionsHandle* opts = 
			new CBlastNucleotideOptionsHandle(locality);
		opts->SetTraditionalMegablastDefaults();
		retval = opts;
        break;
	}

    case eDiscMegablast:
        retval = new CDiscNucleotideOptionsHandle(locality);
        break;

    case eRPSBlast:
        retval = new CBlastRPSOptionsHandle(locality);
        break;

    case eRPSTblastn:
        retval = new CRPSTBlastnOptionsHandle(locality);
        break;
        
    case ePSIBlast:
        retval = new CPSIBlastOptionsHandle(locality);
        break;

    case ePHIBlastp:
        retval = new CPHIBlastProtOptionsHandle(locality);
        break;        

    case ePHIBlastn:
        retval = new CPHIBlastNuclOptionsHandle(locality);
        break;

    case eBlastNotSet:
        NCBI_THROW(CBlastException, eInvalidArgument,
                   "eBlastNotSet may not be used as argument");
        break;

    default:
        abort();    // should never happen
    }

    return retval;
}

set<string>
CBlastOptionsFactory::GetTasks()
{
    set<string> retval;
    retval.insert("blastn");
    retval.insert("blastn-short");
    retval.insert("megablast");
    retval.insert("dc-megablast");
    retval.insert("blastp");
    retval.insert("blastp-short");
    retval.insert("psiblast");
    retval.insert("phiblast");
    retval.insert("rpsblast");
    retval.insert("blastx");
    retval.insert("tblastn");
    retval.insert("tblastx");
    return retval;
}


CBlastOptionsHandle*
CBlastOptionsFactory::CreateTask(string task, EAPILocality locality)
{
    CBlastOptionsHandle* retval = NULL;

    // Sanity check to force updating of GetTasks() method
    set<string> allowed_tasks = GetTasks();
    string lc_task(NStr::ToLower(task));
    if (allowed_tasks.find(lc_task) == allowed_tasks.end()) {
        ostringstream os;
        os << "'" << task << "' is not a supported task";
        NCBI_THROW(CBlastException, eInvalidArgument, os.str());
    }

    if (!NStr::CompareNocase(task, "blastn") || !NStr::CompareNocase(task, "blastn-short"))
    {
        CBlastNucleotideOptionsHandle* opts = 
             dynamic_cast<CBlastNucleotideOptionsHandle*> (CBlastOptionsFactory::Create(eBlastn, locality));
        ASSERT(opts);
        if (!NStr::CompareNocase(task, "blastn"))
        {
             opts->SetMatchReward(2);
             opts->SetMismatchPenalty(-3);
             opts->SetMaskAtHash(true);
        }
        else
        {
             opts->SetMatchReward(1);
             opts->SetMismatchPenalty(-3);
             opts->SetEvalueThreshold(50);
             opts->SetWordSize(7);
             opts->ClearFilterOptions();
        }
        retval = opts;
    }
    else if (!NStr::CompareNocase(task, "megablast"))
    {
        CBlastNucleotideOptionsHandle* opts = 
             dynamic_cast<CBlastNucleotideOptionsHandle*> (CBlastOptionsFactory::Create(eMegablast, locality));
        ASSERT(opts);
        opts->SetMatchReward(1);
        opts->SetMismatchPenalty(-2);
        opts->SetMaskAtHash(true);
        retval = opts;
    }
    else if (!NStr::CompareNocase(task, "dc-megablast"))
    {
        CDiscNucleotideOptionsHandle* opts = 
             dynamic_cast<CDiscNucleotideOptionsHandle*> (CBlastOptionsFactory::Create(eDiscMegablast, locality));
        ASSERT(opts);
        opts->SetMatchReward(2);
        opts->SetMismatchPenalty(-3);
        opts->SetMaskAtHash(true);
        retval = opts;
    }
    else if (!NStr::CompareNocase(task, "blastp") || !NStr::CompareNocase(task, "blastp-short"))
    {
         CBlastAdvancedProteinOptionsHandle* opts =
               dynamic_cast<CBlastAdvancedProteinOptionsHandle*> (CBlastOptionsFactory::Create(eBlastp, locality));
         if (task == "blastp-short")
         {
            opts->SetMatrixName("PAM30");
            opts->SetGapOpeningCost(9);
            opts->SetGapExtensionCost(1);
            opts->SetEvalueThreshold(20000);
            opts->SetWordSize(2);
            opts->ClearFilterOptions();
         }
         retval = opts;
    }
    else if (!NStr::CompareNocase(task, "psiblast"))
    {
         retval = CBlastOptionsFactory::Create(ePSIBlast, locality);
    }
    else if (!NStr::CompareNocase(task, "phiblast"))
    {
         retval = CBlastOptionsFactory::Create(ePHIBlastp, locality);
    }
    else if (!NStr::CompareNocase(task, "rpsblast"))
    {
         retval = CBlastOptionsFactory::Create(eRPSBlast, locality);
    }
    else if (!NStr::CompareNocase(task, "blastx"))
    {
         retval = CBlastOptionsFactory::Create(eBlastx, locality);
    }
    else if (!NStr::CompareNocase(task, "tblastn"))
    {
         retval = CBlastOptionsFactory::Create(eTblastn, locality);
    }
    else if (!NStr::CompareNocase(task, "tblastx"))
    {
         retval = CBlastOptionsFactory::Create(eTblastx, locality);
    }
    else
    {
        abort();    // should never get here
    }
    return retval;
}

END_SCOPE(blast)
END_NCBI_SCOPE


/* @} */
