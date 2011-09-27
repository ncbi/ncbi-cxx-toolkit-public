#ifndef GPIPE_COMMON___SCORE_LOOKUP__HPP
#define GPIPE_COMMON___SCORE_LOOKUP__HPP

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
 * Authors:  Eyal Mozes
 *
 * File Description:
 *
 */

#include <corelib/ncbiobj.hpp>

#include <algo/align/util/score_builder.hpp>

#include <objmgr/scope.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
    class CSeq_align;
END_SCOPE(objects)


class NCBI_XALGOALIGN_EXPORT CScoreLookup : public CObject
{
public:
    class IScore : public CObject
    {
    public:
        enum EComplexity {
            eEasy,
            eHard
        };

        virtual ~IScore() {}
        virtual double Get(const objects::CSeq_align& align,
                           objects::CScope* scope) const = 0;
        virtual void PrintHelp(CNcbiOstream &ostr) const = 0;
        virtual EComplexity GetComplexity() const { return eEasy; };

        /// For any IScore subclasses that have an internal state, this
        /// function will be called to update it for any alignment that
        /// matches the filter
        virtual void UpdateState(const objects::CSeq_align& align) {}
    };

    CScoreLookup() { x_Init(); }

    CScoreLookup(enum blast::EProgram program_type)
        : m_ScoreBuilder(program_type) { x_Init(); }

    CScoreLookup(blast::CBlastOptionsHandle& options)
        : m_ScoreBuilder(options) { x_Init(); }

    void SetEffectiveSearchSpace(Int8 searchsp) // required for blast e-values
    { m_ScoreBuilder.SetEffectiveSearchSpace(searchsp); }

    void  SetSubstMatrix (const string &name) 
    { m_ScoreBuilder.SetSubstMatrix(name); }

    /// CScoreLookup uses a scope internally.  You can set a scope yourself;
    /// alternatively, the scope used internally will be a default scope
    void SetScope(objects::CScope& scope)
    { m_Scope.Reset(&scope); }

    /// Print out the dictionary of recognized score names
    void PrintDictionary(CNcbiOstream&);

    /// Help text for score
    string HelpText(const string &score_name);

    /// Get requested score for alignment
    double GetScore(const objects::CSeq_align& align,
                    const string &score_name);

    void UpdateState(const objects::CSeq_align& align);

private:
    void x_Init();

    void x_PrintDictionaryEntry(CNcbiOstream &ostr,
                                const string &score_name);

    CRef<objects::CScope> m_Scope;

    objects::CScoreBuilder m_ScoreBuilder;

    typedef map<string, CIRef<IScore> > TScoreDictionary;
    TScoreDictionary m_Scores;

    set<string> m_ScoresUsed;
};



END_NCBI_SCOPE


#endif  // GPIPE_COMMON___SCORE_LOOKUP__HPP
