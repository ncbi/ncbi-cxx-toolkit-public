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
 * Author:  Kevin Bealer
 *
 * File Description:
 *   Define names and value types for options known to blast4.
 *
 */

/// @file names.hpp
/// Names used in blast4 network communications.
///
/// This file declares string objects corresponding to names used when
/// specifying options for blast4 network communications protocol.

#ifndef OBJECTS_BLAST_NAMES_HPP
#define OBJECTS_BLAST_NAMES_HPP

#include <utility>
#include <string>
#include <corelib/ncbistl.hpp>
#include <objects/blast/NCBI_Blast4_module.hpp>
#include <objects/blast/Blast4_value.hpp>

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

class CBlast4Field {
public:
    CBlast4Field();
    
    CBlast4Field(std::string nm, CBlast4_value::E_Choice ch);
    
    const string & GetName() const
    {
        return m_Name;
    }
    
    CBlast4_value::E_Choice GetType() const
    {
        return m_Type;
    }
    
private:
    string                  m_Name;
    CBlast4_value::E_Choice m_Type;
    
    static map<string, CBlast4Field> m_Fields;
};

extern CBlast4Field B4Param_CompositionBasedStats;
extern CBlast4Field B4Param_Culling;
extern CBlast4Field B4Param_CutoffScore;
extern CBlast4Field B4Param_DbGeneticCode;
extern CBlast4Field B4Param_DbLength;
extern CBlast4Field B4Param_EffectiveSearchSpace;
extern CBlast4Field B4Param_EntrezQuery;
extern CBlast4Field B4Param_EvalueThreshold;
extern CBlast4Field B4Param_FilterString;
extern CBlast4Field B4Param_FinalDbSeq;
extern CBlast4Field B4Param_FirstDbSeq;
extern CBlast4Field B4Param_GapExtensionCost;
extern CBlast4Field B4Param_GapOpeningCost;
extern CBlast4Field B4Param_GiList;
extern CBlast4Field B4Param_HitlistSize;
extern CBlast4Field B4Param_HspRangeMax;
extern CBlast4Field B4Param_InclusionThreshold;
extern CBlast4Field B4Param_LCaseMask;
extern CBlast4Field B4Param_MatchReward;
extern CBlast4Field B4Param_MatrixName;
extern CBlast4Field B4Param_MatrixTable;
extern CBlast4Field B4Param_MismatchPenalty;
extern CBlast4Field B4Param_PercentIdentity;
extern CBlast4Field B4Param_PHIPattern;
extern CBlast4Field B4Param_PseudoCountWeight;
extern CBlast4Field B4Param_QueryGeneticCode;
extern CBlast4Field B4Param_RequiredEnd;
extern CBlast4Field B4Param_RequiredStart;
extern CBlast4Field B4Param_StrandOption;
extern CBlast4Field B4Param_MBTemplateLength;
extern CBlast4Field B4Param_MBTemplateType;
extern CBlast4Field B4Param_OutOfFrameMode;
extern CBlast4Field B4Param_UngappedMode;
extern CBlast4Field B4Param_UseRealDbSize;
extern CBlast4Field B4Param_WindowSize;
extern CBlast4Field B4Param_WordSize;
extern CBlast4Field B4Param_WordThreshold;

END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.1  2006/12/13 18:15:32  bealer
* - Define standard set of parameters for blast4.
*
*
* ===========================================================================
*/

#endif // OBJECTS_BLAST_NAMES_HPP
