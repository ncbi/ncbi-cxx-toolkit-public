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
 * Authors:  Anatoliy Kuznetsov
 *
 * File Description:  Things for representing and manipulating bio trees
 *
 */

/// @file bio_tree.cpp
/// Things for representing and manipulating bio trees

#include <ncbi_pch.hpp>
#include <algo/phy_tree/bio_tree.hpp>

BEGIN_NCBI_SCOPE

/*
IBioTreeNode::~IBioTreeNode()
{}
*/


CBioTreeFeatureList::CBioTreeFeatureList()
{
}

CBioTreeFeatureList::CBioTreeFeatureList(const CBioTreeFeatureList& flist)
 : m_FeatureList(flist.m_FeatureList)
{}

CBioTreeFeatureList& 
CBioTreeFeatureList::operator=(const CBioTreeFeatureList& flist)
{
    m_FeatureList.assign(flist.m_FeatureList.begin(), 
                         flist.m_FeatureList.end());
    return *this;
}

void CBioTreeFeatureList::SetFeature(TBioTreeFeatureId id, 
                                     const string&     value)
{
    NON_CONST_ITERATE(TFeatureList, it, m_FeatureList) {
        if (it->id == id) {
            it->value = value;
            return;
        }
    }
    m_FeatureList.push_back(CBioTreeFeaturePair(id, value));
}

const string& 
CBioTreeFeatureList::GetFeatureValue(TBioTreeFeatureId id) const
{
    ITERATE(TFeatureList, it, m_FeatureList) {
        if (it->id == id) {
            return it->value;
        }
    }
    return kEmptyStr;
}

void CBioTreeFeatureList::RemoveFeature(TBioTreeFeatureId id)
{
    NON_CONST_ITERATE(TFeatureList, it, m_FeatureList) {
        if (it->id == id) {
            m_FeatureList.erase(it);
            return;
        }
    }
}






CBioTreeFeatureDictionary::CBioTreeFeatureDictionary()
 : m_IdCounter(0)
{
}


CBioTreeFeatureDictionary::CBioTreeFeatureDictionary(
                                      const CBioTreeFeatureDictionary& btr)
 : m_Dict(btr.m_Dict),
   m_Name2Id(btr.m_Name2Id),
   m_IdCounter(btr.m_IdCounter)
{
}

CBioTreeFeatureDictionary& 
CBioTreeFeatureDictionary::operator=(const CBioTreeFeatureDictionary& btr)
{
    Clear();

    ITERATE(TFeatureDict, it, btr.m_Dict) {
        m_Dict.insert(*it);
    }

    ITERATE(TFeatureNameIdx, it, btr.m_Name2Id) {
        m_Name2Id.insert(*it);
    }

    return *this;
}

void CBioTreeFeatureDictionary::Clear()
{
    m_Dict.clear();
    m_Name2Id.clear();
    m_IdCounter = 0;
}

bool 
CBioTreeFeatureDictionary::HasFeature(const string& feature_name) 
{
    TFeatureNameIdx::const_iterator it = m_Name2Id.find(feature_name);
    return (it != m_Name2Id.end());
}

bool 
CBioTreeFeatureDictionary::HasFeature(TBioTreeFeatureId id)
{
    TFeatureDict::const_iterator it = m_Dict.find(id);
    return (it != m_Dict.end());
}

TBioTreeFeatureId 
CBioTreeFeatureDictionary::Register(const string& feature_name)
{
    Register(++m_IdCounter, feature_name);

    return m_IdCounter;
}

void CBioTreeFeatureDictionary::Register(TBioTreeFeatureId id, 
										 const string& feature_name)
{
    m_Dict.insert(
           pair<TBioTreeFeatureId, string>(id, feature_name));
    m_Name2Id.insert(
           pair<string, TBioTreeFeatureId>(feature_name, id));

}


TBioTreeFeatureId 
CBioTreeFeatureDictionary::GetId(const string& feature_name) const
{
    TFeatureNameIdx::const_iterator it = m_Name2Id.find(feature_name);
    if (it == m_Name2Id.end()) {
        return 0;
    }
    return it->second;
}

END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.5  2004/06/07 15:22:13  kuznets
 * + Register
 *
 * Revision 1.4  2004/05/21 21:41:03  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.3  2004/05/19 12:45:18  kuznets
 * + CBioTreeFeatureDictionary::Clear()
 *
 * Revision 1.2  2004/05/10 15:46:35  kuznets
 * Minor changes.
 *
 * Revision 1.1  2004/04/06 17:58:31  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */


