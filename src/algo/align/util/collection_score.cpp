/* 
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
 * Author:  Alex Kotliarov
 *
 * File Description:
 */

#include <ncbi_pch.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>

#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seq/seq_id_handle.hpp>

#include <algo/align/util/algo_align_util_exceptions.hpp>
#include <algo/align/util/named_collection_score_impl.hpp>
#include <algo/align/util/collection_score.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

static vector<pair<string, TNamedScoreFactory> > GetAlignmentCollectionScoreFactoryTable();

class CAlignmentCollectionScore : public IAlignmentCollectionScore
{
    CRef<CScope> m_Scope; 
    map<string, CIRef<INamedAlignmentCollectionScore> > m_Scores;

    void x_LoadNamedScores()
    {
        vector<pair<string, TNamedScoreFactory> > table = GetAlignmentCollectionScoreFactoryTable();
        for ( vector<pair<string, TNamedScoreFactory> >::const_iterator i = table.begin(); i != table.end(); ++i ) {
            m_Scores.insert(make_pair(i->first, i->second()));
        }
    }

public:
    CAlignmentCollectionScore(CScope& scope)
    : m_Scope(&scope)
    {
        x_LoadNamedScores();
    }
    
    CAlignmentCollectionScore()
    : m_Scope(new CScope(*CObjectManager::GetInstance()))
    {
        x_LoadNamedScores();
    }

    vector<CScoreValue> Get(string score_name, CSeq_align_set const& coll) const
    {
        map<string, CIRef<INamedAlignmentCollectionScore> >::const_iterator i = m_Scores.find(score_name);
        if ( i == m_Scores.end() ) {
            NCBI_THROW(CAlgoAlignUtilException, eScoreNotFound, score_name);
        }        

        if ( !coll.IsSet() || coll.Get().empty() ) {
            return vector<CScoreValue>();
        }

        return i->second->Get(const_cast<CScope&>(*m_Scope), coll);
    }
};

CRef<IAlignmentCollectionScore> IAlignmentCollectionScore::GetInstance(CScope& scope)
{
    return CRef<IAlignmentCollectionScore>( new CAlignmentCollectionScore(scope) );
}

CRef<IAlignmentCollectionScore> IAlignmentCollectionScore::GetInstance()
{
    return CRef<IAlignmentCollectionScore>( new CAlignmentCollectionScore() );
}

// Add named score description to the @Factory table.
static struct tagNamedScoreFactory {
    char const* name;
    TNamedScoreFactory factory;
}
Factory[] = {
    {CScoreSeqCoverage::Name, CScoreSeqCoverage::Create},
    {CScoreUniqSeqCoverage::Name, CScoreUniqSeqCoverage::Create},
    {0, 0}
}; 

vector<pair<string, TNamedScoreFactory> > GetAlignmentCollectionScoreFactoryTable()
{
    vector<pair<string, TNamedScoreFactory> > table;

    for ( struct tagNamedScoreFactory* entry = &Factory[0]; entry->name != 0; ++entry ) {
        table.push_back(make_pair(entry->name, entry->factory));
    }       

    return table;
}

END_NCBI_SCOPE
