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

#include <algo/blast/api/blast_option.hpp>
#include "blast_setup.hpp"

// NewBlast includes
#include <algo/blast/core/blast_extend.h>
#include <algo/blast/core/blast_gapalign.h>

#include <objects/seqloc/Seq_loc.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

CBlastOptions::CBlastOptions()
{
    m_QueryOpts.Reset((QuerySetUpOptions*)calloc(1, sizeof(QuerySetUpOptions)));
    m_InitWordOpts.Reset((BlastInitialWordOptions*)calloc(1, sizeof(BlastInitialWordOptions)));
    m_LutOpts.Reset((LookupTableOptions*)calloc(1, sizeof(LookupTableOptions)));
    m_ExtnOpts.Reset((BlastExtensionOptions*)calloc(1, sizeof(BlastExtensionOptions)));
    m_HitSaveOpts.Reset((BlastHitSavingOptions*)calloc(1, sizeof(BlastHitSavingOptions)));
    m_ScoringOpts.Reset((BlastScoringOptions*)calloc(1, sizeof(BlastScoringOptions)));
    m_EffLenOpts.reset((BlastEffectiveLengthsOptions*)calloc(1, sizeof(BlastEffectiveLengthsOptions)));
    m_DbOpts.Reset((BlastDatabaseOptions*)calloc(1, sizeof(BlastDatabaseOptions)));
    m_ProtOpts.Reset((PSIBlastOptions*)calloc(1, sizeof(PSIBlastOptions)));
}

CBlastOptions::~CBlastOptions()
{
}

bool
CBlastOptions::Validate() const
{
    Blast_Message* blmsg = NULL;
    string msg;

    if (BlastScoringOptionsValidate(m_Program, m_ScoringOpts, &blmsg)) {
        msg = blmsg ? blmsg->message : "Scoring options validation failed";
        NCBI_THROW(CBlastException, eBadParameter, msg.c_str());
    }

    if (LookupTableOptionsValidate(m_Program, m_LutOpts, &blmsg)) {
        msg = blmsg ? blmsg->message : "Lookup table options validation failed";
        NCBI_THROW(CBlastException, eBadParameter, msg.c_str());
    }

    if (BlastHitSavingOptionsValidate(m_Program, m_HitSaveOpts, &blmsg)) {
        msg = blmsg ? blmsg->message : "Hit saving options validation failed";
        NCBI_THROW(CBlastException, eBadParameter, msg.c_str());
    }

    if (BlastExtensionOptionsValidate(m_Program, m_ExtnOpts, &blmsg)) {
        msg = blmsg ? blmsg->message : "Extension options validation failed";
        NCBI_THROW(CBlastException, eBadParameter, msg.c_str());
    }

    return true;
}

void
CBlastOptions::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
    ddc.SetFrame("CBlastOptions");
    DebugDumpValue(ddc,"m_Program", m_Program);
    m_QueryOpts.DebugDump(ddc, depth);
    m_LutOpts.DebugDump(ddc, depth);
    m_InitWordOpts.DebugDump(ddc, depth);
    m_ExtnOpts.DebugDump(ddc, depth);
    m_HitSaveOpts.DebugDump(ddc, depth);
    m_ProtOpts.DebugDump(ddc, depth);
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
            return std::memcmp(a, b, size);
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
    if (a->alphabet_size != b->alphabet_size) return false;
    if (a->mb_template_length != b->mb_template_length) return false;
    if (a->mb_template_type != b->mb_template_type) return false;
    if (a->scan_step != b->scan_step) return false;
    if (a->max_num_patterns != b->max_num_patterns) return false;
    if (a->use_pssm != b->use_pssm) return false;
    if (x_safe_strcmp(a->phi_pattern, b->phi_pattern) != 0) return false;
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
operator==(const CBlastOptions& lhs, const CBlastOptions& rhs)
{
    if (&lhs == &rhs)
        return true;

    if (lhs.m_Program != rhs.m_Program)
        return false;

    if ( !x_QuerySetupOptions_cmp(lhs.m_QueryOpts, rhs.m_QueryOpts) )
        return false;

    if ( !x_LookupTableOptions_cmp(lhs.m_LutOpts, rhs.m_LutOpts) )
        return false;

    void *a, *b;

    a = static_cast<void*>( (BlastInitialWordOptions*) lhs.m_InitWordOpts);
    b = static_cast<void*>( (BlastInitialWordOptions*) rhs.m_InitWordOpts);
    if ( x_safe_memcmp(a, b, sizeof(BlastInitialWordOptions)) != 0 )
         return false;

    a = static_cast<void*>( (BlastExtensionOptions*) lhs.m_ExtnOpts);
    b = static_cast<void*>( (BlastExtensionOptions*) rhs.m_ExtnOpts);
    if ( x_safe_memcmp(a, b, sizeof(BlastExtensionOptions)) != 0 )
         return false;

    a = static_cast<void*>( (BlastHitSavingOptions*) lhs.m_HitSaveOpts);
    b = static_cast<void*>( (BlastHitSavingOptions*) rhs.m_HitSaveOpts);
    if ( x_safe_memcmp(a, b, sizeof(BlastHitSavingOptions)) != 0 )
         return false;

    a = static_cast<void*>( (PSIBlastOptions*) lhs.m_ProtOpts);
    b = static_cast<void*>( (PSIBlastOptions*) rhs.m_ProtOpts);
    if ( x_safe_memcmp(a, b, sizeof(PSIBlastOptions)) != 0 )
         return false;

    if ( !x_BlastDatabaseOptions_cmp(lhs.m_DbOpts, rhs.m_DbOpts) )
        return false;

    if ( !x_BlastScoringOptions_cmp(lhs.m_ScoringOpts, rhs.m_ScoringOpts) )
        return false;
    
    a = static_cast<void*>( (BlastEffectiveLengthsOptions*)
                            lhs.m_EffLenOpts.get());
    b = static_cast<void*>( (BlastEffectiveLengthsOptions*)
                            rhs.m_EffLenOpts.get());
    if ( x_safe_memcmp(a, b, sizeof(BlastEffectiveLengthsOptions)) != 0 )
         return false;
    
    return true;
}

bool
operator!=(const CBlastOptions& lhs, const CBlastOptions& rhs)
{
    return !(lhs == rhs);
}

END_SCOPE(blast)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
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
