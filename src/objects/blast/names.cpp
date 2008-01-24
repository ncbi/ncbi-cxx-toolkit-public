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

#include <ncbi_pch.hpp>
#include <objects/blast/names.hpp>
#include <objects/blast/blast__.hpp>
#include <objects/scoremat/scoremat__.hpp>

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

CBlast4Field::CBlast4Field()
    : m_Name("-"), m_Type(CBlast4_value::e_not_set)
{
}

CBlast4Field::CBlast4Field(const std::string& nm, CBlast4_value::E_Choice ty)
    : m_Name(nm), m_Type(ty)
{
    // Should this allow multiple types per key?  A simple method
    // would be to tack something like (":" + IntToString((int)ty)) to
    // the name to build the key here.
    
    m_Fields[nm] = *this;
}

const string & CBlast4Field::GetName() const
{
    return m_Name;
}

CBlast4_value::E_Choice CBlast4Field::GetType() const
{
    return m_Type;
}

bool CBlast4Field::Match(CBlast4_parameter & p) const
{
    return (p.CanGetName()        &&
            p.CanGetValue()       &&
            p.GetValue().Which() == m_Type  &&
            p.GetName() == m_Name);
}

string CBlast4Field::GetString(CBlast4_parameter & p) const
{
    _ASSERT(Match(p));
    return p.GetValue().GetString();
}

bool CBlast4Field::GetBoolean(CBlast4_parameter & p) const
{
    _ASSERT(Match(p));
    return p.GetValue().GetBoolean();
}

Int8 CBlast4Field::GetBig_integer(CBlast4_parameter & p) const
{
    _ASSERT(Match(p));
    return p.GetValue().GetBig_integer();
}

CRef<CBlast4_cutoff> CBlast4Field::GetCutoff(CBlast4_parameter & p) const
{
    _ASSERT(Match(p));
    return CRef<CBlast4_cutoff>(& p.SetValue().SetCutoff());
}

int CBlast4Field::GetInteger(CBlast4_parameter & p) const
{
    _ASSERT(Match(p));
    return p.GetValue().GetInteger();
}

list<int> CBlast4Field::GetIntegerList (CBlast4_parameter & p) const
{
    _ASSERT(Match(p));
    return p.GetValue().GetInteger_list();
}

CRef<CPssmWithParameters> CBlast4Field::GetMatrix(CBlast4_parameter & p) const
{
    _ASSERT(Match(p));
    return CRef<CPssmWithParameters>(& p.SetValue().SetMatrix());
}

CRef<CBlast4_mask> CBlast4Field::GetQueryMask(CBlast4_parameter & p) const
{
    _ASSERT(Match(p));
    return CRef<CBlast4_mask>(& p.SetValue().SetQuery_mask());
}

double CBlast4Field::GetReal(CBlast4_parameter & p) const
{
    _ASSERT(Match(p));
    return p.GetValue().GetReal();
}

EBlast4_strand_type CBlast4Field::GetStrandType(CBlast4_parameter & p) const
{
    _ASSERT(Match(p));
    return p.GetValue().GetStrand_type();
}

bool CBlast4Field::KnownField(const string& name)
{
    return m_Fields.find(name) != m_Fields.end();
}

CBlast4Field::TFieldMap CBlast4Field::m_Fields;

typedef CBlast4Field TField;

TField B4Param_CompositionBasedStats ("CompositionBasedStats", CBlast4_value::e_Integer);
TField B4Param_Culling               ("Culling",               CBlast4_value::e_Boolean);
TField B4Param_CullingLimit          ("Culling",               CBlast4_value::e_Integer);
TField B4Param_CutoffScore           ("CutoffScore",           CBlast4_value::e_Cutoff);
TField B4Param_DbGeneticCode         ("DbGeneticCode",         CBlast4_value::e_Integer);
TField B4Param_DbLength              ("DbLength",              CBlast4_value::e_Big_integer);
TField B4Param_DustFiltering         ("DustFiltering",         CBlast4_value::e_Boolean);
TField B4Param_DustFilteringLevel    ("DustFilteringLevel",    CBlast4_value::e_Integer);
TField B4Param_DustFilteringWindow   ("DustFilteringWindow",   CBlast4_value::e_Integer);
TField B4Param_DustFilteringLinker   ("DustFilteringLinker",   CBlast4_value::e_Integer);
TField B4Param_EffectiveSearchSpace  ("EffectiveSearchSpace",  CBlast4_value::e_Big_integer);
TField B4Param_EntrezQuery           ("EntrezQuery",           CBlast4_value::e_String);
TField B4Param_EvalueThreshold       ("EvalueThreshold",       CBlast4_value::e_Cutoff);
TField B4Param_FilterString          ("FilterString",          CBlast4_value::e_String);
TField B4Param_FinalDbSeq            ("FinalDbSeq",            CBlast4_value::e_Integer);
TField B4Param_FirstDbSeq            ("FirstDbSeq",            CBlast4_value::e_Integer);
TField B4Param_GapExtensionCost      ("GapExtensionCost",      CBlast4_value::e_Integer);
TField B4Param_GapExtnAlgorithm      ("GapExtnAlgorithm",      CBlast4_value::e_Integer);
TField B4Param_GapOpeningCost        ("GapOpeningCost",        CBlast4_value::e_Integer);
TField B4Param_GapTracebackAlgorithm ("GapTracebackAlgorithm", CBlast4_value::e_Integer);
TField B4Param_GiList                ("GiList",                CBlast4_value::e_Integer_list);
TField B4Param_NegativeGiList        ("NegativeGiList",        CBlast4_value::e_Integer_list);
TField B4Param_HitlistSize           ("HitlistSize",           CBlast4_value::e_Integer);
TField B4Param_HspRangeMax           ("HspRangeMax",           CBlast4_value::e_Integer);
TField B4Param_InclusionThreshold    ("InclusionThreshold",    CBlast4_value::e_Real);
TField B4Param_LCaseMask             ("LCaseMask",             CBlast4_value::e_Query_mask);
TField B4Param_MaskAtHash            ("MaskAtHash",            CBlast4_value::e_Boolean);
TField B4Param_MatchReward           ("MatchReward",           CBlast4_value::e_Integer);
TField B4Param_MatrixName            ("MatrixName",            CBlast4_value::e_String);
TField B4Param_MatrixTable           ("MatrixTable",           CBlast4_value::e_Matrix);
TField B4Param_MBTemplateLength      ("MBTemplateLength",      CBlast4_value::e_Integer);
TField B4Param_MBTemplateType        ("MBTemplateType",        CBlast4_value::e_Integer);
TField B4Param_MismatchPenalty       ("MismatchPenalty",       CBlast4_value::e_Integer);
TField B4Param_OutOfFrameMode        ("OutOfFrameMode",        CBlast4_value::e_Boolean);
TField B4Param_PercentIdentity       ("PercentIdentity",       CBlast4_value::e_Real);
TField B4Param_PHIPattern            ("PHIPattern",            CBlast4_value::e_String);
TField B4Param_PseudoCountWeight     ("PseudoCountWeight",     CBlast4_value::e_Integer);
TField B4Param_QueryGeneticCode      ("QueryGeneticCode",      CBlast4_value::e_Integer);
TField B4Param_RepeatFiltering       ("RepeatFiltering",       CBlast4_value::e_Boolean);
TField B4Param_RepeatFilteringDB     ("RepeatFilteringDB",     CBlast4_value::e_String);
TField B4Param_RequiredEnd           ("RequiredEnd",           CBlast4_value::e_Integer);
TField B4Param_RequiredStart         ("RequiredStart",         CBlast4_value::e_Integer);
TField B4Param_SegFiltering          ("SegFiltering",          CBlast4_value::e_Boolean);
TField B4Param_SegFilteringWindow    ("SegFilteringWindow",    CBlast4_value::e_Integer);
TField B4Param_SegFilteringLocut     ("SegFilteringLocut",     CBlast4_value::e_Real);
TField B4Param_SegFilteringHicut     ("SegFilteringHicut",     CBlast4_value::e_Real);
TField B4Param_StrandOption          ("StrandOption",          CBlast4_value::e_Strand_type);
TField B4Param_UngappedMode          ("UngappedMode",          CBlast4_value::e_Boolean);
TField B4Param_UseRealDbSize         ("UseRealDbSize",         CBlast4_value::e_Boolean);
TField B4Param_WindowSize            ("WindowSize",            CBlast4_value::e_Integer);
TField B4Param_WordSize              ("WordSize",              CBlast4_value::e_Integer);
TField B4Param_WordThreshold         ("WordThreshold",         CBlast4_value::e_Integer);
TField B4Param_SumStatistics         ("SumStatistics",         CBlast4_value::e_Boolean);
TField B4Param_LongestIntronLength   ("LongestIntronLength",   CBlast4_value::e_Integer);
TField B4Param_GapTrigger            ("GapTrigger",            CBlast4_value::e_Real);
TField B4Param_GapXDropoff           ("GapXDropoff",           CBlast4_value::e_Real);
TField B4Param_GapXDropoffFinal      ("GapXDropoffFinal",      CBlast4_value::e_Real);
TField B4Param_SmithWatermanMode     ("SmithWatermanMode",     CBlast4_value::e_Boolean);

END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE
