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
* Author:  Christiam Camacho
*
* File Description:
*   Class to encapsulate all NewBlast options
*
* ===========================================================================
*/

/// @file blast_options_cxx.cpp
/// Implements the CBlastOptions class, which encapsulates options structures
/// from algo/blast/core

#include <ncbi_pch.hpp>
#include <algo/blast/api/blast_options.hpp>
#include "blast_setup.hpp"

#include <algo/blast/core/blast_extend.h>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/blast/Blast4_cutoff.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

#ifndef SKIP_DOXYGEN_PROCESSING

CBlastOptions::CBlastOptions(EAPILocality locality)
    : m_Local (0),
      m_Remote(0)
{
    if (locality != eRemote) {
        m_Local = new CBlastOptionsLocal();
    }
    if (locality != eLocal) {
        m_Remote = new CBlastOptionsRemote();
    }
}

CBlastOptions::~CBlastOptions()
{
    if (m_Local) {
        delete m_Local;
    }
    if (m_Remote) {
        delete m_Remote;
    }
}

// Note: only some of the options are supported for the remote case;
// An exception is thrown if the option is not available.

void CBlastOptionsRemote::SetValue(EBlastOptIdx opt, const EProgram & v)
{
    switch(opt) {
    case eBlastOpt_Program:
        return;
        
    default:
        break;
    }
    
    char errbuf[1024];
    
    sprintf(errbuf, "tried to set option (%d) and value (%d), line (%d).",
            int(opt), v, __LINE__);
    
    x_Throwx(string("err:") + errbuf);
}

void CBlastOptionsRemote::SetValue(EBlastOptIdx opt, const int & v)
{
    switch(opt) {
    case eBlastOpt_WordSize:
        x_SetParam("WordSize", v);
        return;
        
    case eBlastOpt_StrandOption:
        {
            typedef objects::EBlast4_strand_type TSType;
            TSType strand;
            bool set_strand = true;
            
            switch(v) {
            case 1:
                strand = eBlast4_strand_type_forward_strand;
                break;
                
            case 2:
                strand = eBlast4_strand_type_reverse_strand;
                break;
                
            case 3:
                strand = eBlast4_strand_type_both_strands;
                break;
                
            default:
                set_strand = false;
            }
            
            if (set_strand) {
                x_SetParam("StrandOption", strand);
                return;
            }
        }
        
    case eBlastOpt_WindowSize:
        x_SetParam("WindowSize", v);
        return;
        
    case eBlastOpt_GapOpeningCost:
        x_SetParam("GapOpeningCost", v);
        return;
        
    case eBlastOpt_GapExtensionCost:
        x_SetParam("GapExtensionCost", v);
        return;
        
    case eBlastOpt_HitlistSize:
        x_SetParam("HitlistSize", v);
        return;
        
    case eBlastOpt_PrelimHitlistSize:
        x_SetParam("PrelimHitlistSize", v);
        return;
        
    case eBlastOpt_CutoffScore:
        if (0) {
            typedef objects::CBlast4_cutoff TCutoff;
            CRef<TCutoff> cutoff(new TCutoff);
            cutoff->SetRaw_score(v);
            
            x_SetParam("CutoffScore", cutoff);
        }
        return;
        
    case eBlastOpt_MatchReward:
        x_SetParam("MatchReward", v);
        return;
        
    case eBlastOpt_MismatchPenalty:
        x_SetParam("MismatchPenalty", v);
        return;

    case eBlastOpt_WordThreshold:
        x_SetParam("WordThreshold", v);
        return;
        
    case eBlastOpt_PseudoCount:
        x_SetParam("PseudoCountWeight", v);
        return;
        
    default:
        break;
    }
    
    char errbuf[1024];
    
    sprintf(errbuf, "tried to set option (%d) and value (%d), line (%d).",
            int(opt), v, __LINE__);
    
    x_Throwx(string("err:") + errbuf);
}

void CBlastOptionsRemote::SetValue(EBlastOptIdx opt, const double & v)
{
    switch(opt) {
    case eBlastOpt_EvalueThreshold:
        {
            typedef objects::CBlast4_cutoff TCutoff;
            CRef<TCutoff> cutoff(new TCutoff);
            cutoff->SetE_value(v);
            
            x_SetParam("EvalueThreshold", cutoff);
        }
        return;
        
    case eBlastOpt_PercentIdentity:
        x_SetParam("PercentIdentity", v);
        return;
        
    default:
        break;
    }
    
    char errbuf[1024];
    
    sprintf(errbuf, "tried to set option (%d) and value (%f), line (%d).",
            int(opt), v, __LINE__);
    
    x_Throwx(string("err:") + errbuf);
}

void CBlastOptionsRemote::SetValue(EBlastOptIdx opt, const char * v)
{
    switch(opt) {
    case eBlastOpt_FilterString:
        x_SetParam("FilterString", v);
        return;
        
    case eBlastOpt_MatrixName:
        x_SetParam("MatrixName", v);
        return;
        
    default:
        break;
    }
    
    char errbuf[1024];
    
    sprintf(errbuf, "tried to set option (%d) and value (%.20s), line (%d).",
            int(opt), v, __LINE__);
    
    x_Throwx(string("err:") + errbuf);
}

void CBlastOptionsRemote::SetValue(EBlastOptIdx opt, const TSeqLocVector & v)
{
    char errbuf[1024];
    
    sprintf(errbuf, "tried to set option (%d) and TSeqLocVector (size %zd), line (%d).",
            int(opt), v.size(), __LINE__);
    
    x_Throwx(string("err:") + errbuf);
}

void CBlastOptionsRemote::SetValue(EBlastOptIdx opt, const ESeedContainerType & v)
{
    char errbuf[1024];
    
    sprintf(errbuf, "tried to set option (%d) and value (%d), line (%d).",
            int(opt), v, __LINE__);
    
    x_Throwx(string("err:") + errbuf);
}

void CBlastOptionsRemote::SetValue(EBlastOptIdx opt, const ESeedExtensionMethod & v)
{
    char errbuf[1024];
    
    sprintf(errbuf, "tried to set option (%d) and value (%d), line (%d).",
            int(opt), v, __LINE__);
    
    x_Throwx(string("err:") + errbuf);
}

void CBlastOptionsRemote::SetValue(EBlastOptIdx opt, const bool & v)
{
    switch(opt) {
    case eBlastOpt_GappedMode:
        {
            bool ungapped = ! v;
            x_SetParam("UngappedMode", ungapped); // inverted
            return;
        }
        
    case eBlastOpt_OutOfFrameMode:
        x_SetParam("OutOfFrameMode", v);
        return;
        
    case eBlastOpt_UseRealDbSize:
        x_SetParam("UseRealDbSize", v);
        return;
        
    default:
        break;
    }
    
    char errbuf[1024];
    
    sprintf(errbuf, "tried to set option (%d) and value (%s), line (%d).",
            int(opt), (v ? "true" : "false"), __LINE__);
    
    x_Throwx(string("err:") + errbuf);
}

void CBlastOptionsRemote::SetValue(EBlastOptIdx opt, const Int8 & v)
{
    switch(opt) {
    case eBlastOpt_EffectiveSearchSpace:
        x_SetParam("EffectiveSearchSpace", v);
        return;
        
    default:
        break;
    }
    
    char errbuf[1024];
    
    sprintf(errbuf, "tried to set option (%d) and value (%f), line (%d).",
            int(opt), double(v), __LINE__);
    
    x_Throwx(string("err:") + errbuf);
}


CBlastOptionsLocal::CBlastOptionsLocal()
{
    QuerySetUpOptions* query_setup = NULL;
    BlastQuerySetUpOptionsNew(&query_setup);
    m_QueryOpts.Reset(query_setup);
    m_InitWordOpts.Reset((BlastInitialWordOptions*)calloc(1, sizeof(BlastInitialWordOptions)));
    m_LutOpts.Reset((LookupTableOptions*)calloc(1, sizeof(LookupTableOptions)));
    m_ExtnOpts.Reset((BlastExtensionOptions*)calloc(1, sizeof(BlastExtensionOptions)));
    m_HitSaveOpts.Reset((BlastHitSavingOptions*)calloc(1, sizeof(BlastHitSavingOptions)));
    m_ScoringOpts.Reset((BlastScoringOptions*)calloc(1, sizeof(BlastScoringOptions)));
    m_EffLenOpts.Reset((BlastEffectiveLengthsOptions*)calloc(1, sizeof(BlastEffectiveLengthsOptions)));
    m_DbOpts.Reset((BlastDatabaseOptions*)calloc(1, sizeof(BlastDatabaseOptions)));
    m_PSIBlastOpts.Reset((PSIBlastOptions*)calloc(1, sizeof(PSIBlastOptions)));
}

CBlastOptionsLocal::~CBlastOptionsLocal()
{
}

#define GENCODE_STRLEN 64

void 
CBlastOptionsLocal::SetDbGeneticCode(int gc)
{

    m_DbOpts->genetic_code = gc;

    if (m_DbOpts->gen_code_string) 
        sfree(m_DbOpts->gen_code_string);

    m_DbOpts->gen_code_string = (Uint1*)
        BlastMemDup(FindGeneticCode(gc).get(), GENCODE_STRLEN);
}

EBlastProgramType 
CBlastOptionsLocal::GetProgramType() const
{
    switch (m_Program) {
    case eBlastn: case eMegablast: case eDiscMegablast:
        return eBlastTypeBlastn;
    case eBlastp: 
        return eBlastTypeBlastp;
    case eBlastx:
        return eBlastTypeBlastx;
    case eTblastn:
        return eBlastTypeTblastn;
    case eTblastx:
        return eBlastTypeTblastx;
    case eRPSBlast:
        return eBlastTypeRpsBlast;
    case eRPSTblastn:
        return eBlastTypeRpsTblastn;
    case ePSIBlast:
        return eBlastTypePsiBlast;
    default:
        return eBlastTypeUndefined;
    }
}

static void 
BlastMessageToException(Blast_Message** blmsg_ptr, const string& default_msg)
{
    if (!blmsg_ptr || *blmsg_ptr == NULL)
        return;

    Blast_Message* blmsg = *blmsg_ptr;
    string msg = blmsg ? blmsg->message : default_msg;

    *blmsg_ptr = Blast_MessageFree(blmsg);

    if (msg != NcbiEmptyString)
        NCBI_THROW(CBlastException, eBadParameter, msg.c_str());
}

bool
CBlastOptionsLocal::Validate() const
{
    Blast_Message* blmsg = NULL;
    string msg;
    EBlastProgramType program = GetProgramType();

    if (BlastScoringOptionsValidate(program, m_ScoringOpts, &blmsg)) {
        BlastMessageToException(&blmsg, "Scoring options validation failed");
    }

    if (LookupTableOptionsValidate(program, m_LutOpts, &blmsg)) {
        BlastMessageToException(&blmsg, "Lookup table options validation failed");
    }

    if (BlastInitialWordOptionsValidate(program, m_InitWordOpts, &blmsg)) {
        BlastMessageToException(&blmsg, "Word finder options validation failed");
    }

    if (BlastHitSavingOptionsValidate(program, m_HitSaveOpts, &blmsg)) {
        BlastMessageToException(&blmsg, "Hit saving options validation failed");
    }

    if (BlastExtensionOptionsValidate(program, m_ExtnOpts, &blmsg)) {
        BlastMessageToException(&blmsg, "Extension options validation failed");
    }

    return true;
}

void
CBlastOptionsLocal::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
    ddc.SetFrame("CBlastOptions");
    DebugDumpValue(ddc,"m_Program", m_Program);
    m_QueryOpts.DebugDump(ddc, depth);
    m_LutOpts.DebugDump(ddc, depth);
    m_InitWordOpts.DebugDump(ddc, depth);
    m_ExtnOpts.DebugDump(ddc, depth);
    m_HitSaveOpts.DebugDump(ddc, depth);
    m_PSIBlastOpts.DebugDump(ddc, depth);
    m_DbOpts.DebugDump(ddc, depth);
    m_ScoringOpts.DebugDump(ddc, depth);
    //m_EffLenOpts.DebugDump(ddc, depth);
}

inline int
x_safe_strcmp(const char* a, const char* b)
{
    if (a != b) {
        if (a != NULL && b != NULL) {
            return strcmp(a,b);
        } else {
            return 1;
        }
    }
    return 0;
}

inline int
x_safe_memcmp(const void* a, const void* b, size_t size)
{
    if (a != b) {
        if (a != NULL && b != NULL) {
            return memcmp(a, b, size);
        } else {
            return 1;
        }
    }
    return 0;
}

bool
x_QuerySetupOptions_cmp(const QuerySetUpOptions* a, const QuerySetUpOptions* b)
{
    if (x_safe_strcmp(a->filter_string, b->filter_string) != 0) {
        return false;
    }
    if (a->strand_option != b->strand_option) return false;
    if (a->genetic_code != b->genetic_code) return false;
    return true;
}

bool
x_LookupTableOptions_cmp(const LookupTableOptions* a, 
                         const LookupTableOptions* b)
{
    if (a->threshold != b->threshold) return false;
    if (a->lut_type != b->lut_type) return false;
    if (a->word_size != b->word_size) return false;
    if (a->mb_template_length != b->mb_template_length) return false;
    if (a->mb_template_type != b->mb_template_type) return false;
    if (a->max_num_patterns != b->max_num_patterns) return false;
    if (a->use_pssm != b->use_pssm) return false;
    if (x_safe_strcmp(a->phi_pattern, b->phi_pattern) != 0) return false;
    if (a->variable_wordsize != b->variable_wordsize) return false;
    return true;
}

bool
x_BlastDatabaseOptions_cmp(const BlastDatabaseOptions* a,
                           const BlastDatabaseOptions* b)
{
    if (a->genetic_code != b->genetic_code) return false;
    if (x_safe_memcmp((void*)a->gen_code_string, 
                      (void*)b->gen_code_string, GENCODE_STRLEN) != 0)
        return false;
    return true;
}

bool
x_BlastScoringOptions_cmp(const BlastScoringOptions* a,
                          const BlastScoringOptions* b)
{
    if (x_safe_strcmp(a->matrix, b->matrix) != 0) return false;
    if (x_safe_strcmp(a->matrix_path, b->matrix_path) != 0) return false;
    if (a->reward != b->reward) return false;
    if (a->penalty != b->penalty) return false;
    if (a->gapped_calculation != b->gapped_calculation) return false;
    if (a->gap_open != b->gap_open) return false;
    if (a->gap_extend != b->gap_extend) return false;
    if (a->decline_align != b->decline_align) return false;
    if (a->is_ooframe != b->is_ooframe) return false;
    if (a->shift_pen != b->shift_pen) return false;
    return true;
}

bool
CBlastOptionsLocal::operator==(const CBlastOptionsLocal& rhs) const
{
    if (this == &rhs)
        return true;

    if (m_Program != rhs.m_Program)
        return false;

    if ( !x_QuerySetupOptions_cmp(m_QueryOpts, rhs.m_QueryOpts) )
        return false;

    if ( !x_LookupTableOptions_cmp(m_LutOpts, rhs.m_LutOpts) )
        return false;

    void *a, *b;

    a = static_cast<void*>( (BlastInitialWordOptions*) m_InitWordOpts);
    b = static_cast<void*>( (BlastInitialWordOptions*) rhs.m_InitWordOpts);
    if ( x_safe_memcmp(a, b, sizeof(BlastInitialWordOptions)) != 0 )
         return false;

    a = static_cast<void*>( (BlastExtensionOptions*) m_ExtnOpts);
    b = static_cast<void*>( (BlastExtensionOptions*) rhs.m_ExtnOpts);
    if ( x_safe_memcmp(a, b, sizeof(BlastExtensionOptions)) != 0 )
         return false;

    a = static_cast<void*>( (BlastHitSavingOptions*) m_HitSaveOpts);
    b = static_cast<void*>( (BlastHitSavingOptions*) rhs.m_HitSaveOpts);
    if ( x_safe_memcmp(a, b, sizeof(BlastHitSavingOptions)) != 0 )
         return false;

    a = static_cast<void*>( (PSIBlastOptions*) m_PSIBlastOpts);
    b = static_cast<void*>( (PSIBlastOptions*) rhs.m_PSIBlastOpts);
    if ( x_safe_memcmp(a, b, sizeof(PSIBlastOptions)) != 0 )
         return false;

    if ( !x_BlastDatabaseOptions_cmp(m_DbOpts, rhs.m_DbOpts) )
        return false;

    if ( !x_BlastScoringOptions_cmp(m_ScoringOpts, rhs.m_ScoringOpts) )
        return false;
    
    a = static_cast<void*>( (BlastEffectiveLengthsOptions*) m_EffLenOpts);
    b = static_cast<void*>( (BlastEffectiveLengthsOptions*) rhs.m_EffLenOpts);
    if ( x_safe_memcmp(a, b, sizeof(BlastEffectiveLengthsOptions)) != 0 )
         return false;
    
    return true;
}

bool
CBlastOptionsLocal::operator!=(const CBlastOptionsLocal& rhs) const
{
    return !(*this== rhs);
}

bool
CBlastOptions::operator==(const CBlastOptions& rhs) const
{
    if (m_Local && rhs.m_Local) {
        return (*m_Local == *rhs.m_Local);
    } else {
        NCBI_THROW(CBlastException, eInternal, 
                   "Equality operator unsupported for arguments");
    }
}

bool
CBlastOptions::operator!=(const CBlastOptions& rhs) const
{
    return !(*this == rhs);
}

#endif /* SKIP_DOXYGEN_PROCESSING */

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

/*
* ===========================================================================
*
* $Log$
* Revision 1.59  2005/02/24 13:46:54  madden
* Changes to use structured filteing options instead of string
*
* Revision 1.58  2005/01/26 18:13:16  camacho
* Fix compiler warning
*
* Revision 1.57  2005/01/10 13:36:06  madden
* Removed deleted options from x_LookupTableOptions_cmp
*
* Revision 1.56  2004/12/28 16:42:32  camacho
* Consistently use the RAII idiom for C structures using wrapper classes in CBlastOptions
*
* Revision 1.55  2004/11/09 20:07:35  camacho
* Fix CBlastOptionsLocal::GetProgramType for PSI-BLAST
*
* Revision 1.54  2004/10/04 18:24:46  bealer
* - Add Pseudo Count constant to remote blast switches.
*
* Revision 1.53  2004/09/08 18:32:31  camacho
* Doxygen fixes
*
* Revision 1.52  2004/08/30 16:53:43  dondosha
* Added E in front of enum names ESeedExtensionMethod and ESeedContainerType
*
* Revision 1.51  2004/08/26 20:54:10  dondosha
* Removed printouts; throw exception when setting of unavailable option is attempted for remote handle
*
* Revision 1.50  2004/08/18 18:14:13  camacho
* Remove GetProgramFromBlastProgramType, add ProgramNameToEnum
*
* Revision 1.49  2004/08/17 15:13:00  ivanov
* Moved GetProgramFromBlastProgramType() from blast_setup_cxx.cpp
* to blast_options_cxx.cpp
*
* Revision 1.48  2004/08/13 17:56:45  dondosha
* Memory leak fix in Validate method: throw exception after first failure
*
* Revision 1.47  2004/08/03 20:22:08  dondosha
* Added initial word options validation
*
* Revision 1.46  2004/07/06 15:48:40  dondosha
* Use EBlastProgramType enumeration type instead of EProgram when calling C code
*
* Revision 1.45  2004/06/08 15:20:22  dondosha
* Skip traceback option has been moved to the traceback extension method enum
*
* Revision 1.44  2004/05/21 21:41:02  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.43  2004/05/17 18:12:29  bealer
* - Add PSI-Blast support.
*
* Revision 1.42  2004/05/17 15:31:31  madden
* Removed unneeded blast_gapalign.h include
*
* Revision 1.41  2004/03/24 22:12:46  dondosha
* Fixed memory leaks
*
* Revision 1.40  2004/03/19 19:22:55  camacho
* Move to doxygen group AlgoBlast, add missing CVS logs at EOF
*
* Revision 1.39  2004/02/17 23:53:31  dondosha
* Added setting of preliminary hitlist size
*
* Revision 1.38  2004/02/04 22:33:36  bealer
* - Add 'noop' default cases to eliminate compiler warnings.
*
* Revision 1.37  2004/01/20 17:53:01  bealer
* - Add SkipTraceback option.
*
* Revision 1.36  2004/01/20 17:06:50  camacho
* Made operator== a member function
*
* Revision 1.35  2004/01/20 15:59:40  camacho
* Added missing implementations of overloaded operators
*
* Revision 1.34  2004/01/17 00:52:32  ucko
* Remove redundant default argument specification.
*
* Revision 1.33  2004/01/16 21:49:26  bealer
* - Add locality flag for Blast4 API
*
* Revision 1.32  2003/12/04 18:35:33  dondosha
* Correction in assigning the genetic code string option
*
* Revision 1.31  2003/12/03 16:41:16  dondosha
* Added SetDbGeneticCode implementation, to set both integer and string
*
* Revision 1.30  2003/11/26 19:37:59  camacho
* Fix windows problem with std::memcmp
*
* Revision 1.29  2003/11/26 18:36:45  camacho
* Renaming blast_option*pp -> blast_options*pp
*
* Revision 1.28  2003/11/26 18:23:59  camacho
* +Blast Option Handle classes
*
* Revision 1.27  2003/10/30 19:37:36  dondosha
* Removed extra stuff accidentally committed
*
* Revision 1.26  2003/10/30 19:34:53  dondosha
* Removed gapped_calculation from BlastHitSavingOptions structure
*
* Revision 1.25  2003/10/21 22:15:33  camacho
* Rearranging of C options structures, fix seed extension method
*
* Revision 1.24  2003/10/21 09:41:14  camacho
* Initialize variable_wordsize for eBlastn
*
* Revision 1.23  2003/10/17 18:21:53  dondosha
* Use separate variables for different initial word extension options
*
* Revision 1.22  2003/10/02 22:10:46  dondosha
* Corrections for one-strand translated searches
*
* Revision 1.21  2003/09/11 17:45:03  camacho
* Changed CBlastOption -> CBlastOptions
*
* Revision 1.20  2003/09/09 22:13:36  dondosha
* Added SetDbGeneticCodeAndStr method to set both integer and string genetic code in one call
*
* Revision 1.19  2003/09/09 12:57:15  camacho
* + internal setup functions, use smart pointers to handle memory mgmt
*
* Revision 1.18  2003/09/03 19:36:27  camacho
* Fix include path for blast_setup.hpp
*
* Revision 1.17  2003/09/03 18:45:34  camacho
* Fixed small memory leak, removed unneeded function
*
* Revision 1.16  2003/09/02 21:15:11  camacho
* Fix small memory leak
*
* Revision 1.15  2003/08/27 15:05:56  camacho
* Use symbolic name for alphabet sizes
*
* Revision 1.14  2003/08/21 19:32:08  dondosha
* Call SetDbGeneticCodeStr when creating a database gen. code string, to avoid code duplication
*
* Revision 1.13  2003/08/19 20:28:10  dondosha
* EProgram enum type is no longer part of CBlastOptions class
*
* Revision 1.12  2003/08/19 13:46:13  dicuccio
* Added 'USING_SCOPE(objects)' to .cpp files for ease of reading implementation.
*
* Revision 1.11  2003/08/18 20:58:57  camacho
* Added blast namespace, removed *__.hpp includes
*
* Revision 1.10  2003/08/14 19:07:32  dondosha
* Added BLASTGetEProgram function to convert from Uint1 to enum type
*
* Revision 1.9  2003/08/11 15:17:39  dondosha
* Added algo/blast/core to all #included headers
*
* Revision 1.8  2003/08/08 19:43:07  dicuccio
* Compilation fixes: #include file rearrangement; fixed use of 'list' and
* 'vector' as variable names; fixed missing ostrea<< for __int64
*
* Revision 1.7  2003/08/01 22:34:11  camacho
* Added accessors/mutators/defaults for matrix_path
*
* Revision 1.6  2003/07/31 19:45:33  camacho
* Eliminate Ptr notation
*
* Revision 1.5  2003/07/30 15:00:01  camacho
* Do not use Malloc/MemNew/MemFree
*
* Revision 1.4  2003/07/24 18:24:17  camacho
* Minor
*
* Revision 1.3  2003/07/23 21:29:37  camacho
* Update BlastDatabaseOptions
*
* Revision 1.2  2003/07/15 19:22:04  camacho
* Fix setting of scan step in blastn
*
* Revision 1.1  2003/07/10 18:34:19  camacho
* Initial revision
*
*
* ===========================================================================
*/
