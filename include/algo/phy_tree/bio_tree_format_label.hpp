#ifndef ALGO_PHY_TREE___BIO_TREE_FORMAT_LABEL__HPP
#define ALGO_PHY_TREE___BIO_TREE_FORMAT_LABEL__HPP

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
* Authors:  Vladislav Evgeniev, Vladimir Tereshkov
*
* File Description:  Generates node labels as a mix of dictionary ids and (optional) text in between
*
*/

#include <string>
#include <vector>
#include <algo/phy_tree/bio_tree.hpp>

BEGIN_NCBI_SCOPE

class NCBI_XALGOPHYTREE_EXPORT CBioTreeFormatLabel
{
public:
    CBioTreeFormatLabel(const CBioTreeFeatureDictionary& dict, const std::string &format);
    std::string FormatLabel(const CBioTreeFeatureList& features) const;
private:
    // Labels can consist of a mix of terms found in dictionary and text between those terms. 
    struct LabelElt {
        LabelElt() : m_ID(-1) {}

        TBioTreeFeatureId m_ID;
        std::string m_Value;
    };
    std::vector<LabelElt> m_LabelElements;
};

class NCBI_XALGOPHYTREE_EXPORT CBioTreeDynamicLabelFormatter :
    public IBioTreeDynamicLabelFormatter
{
public:
    CBioTreeDynamicLabelFormatter(const CBioTreeFeatureDictionary& dict, const std::string &format) :
        m_LabelFormat(dict, format)
    {}
    std::string GetLabelForNode(const CBioTreeDynamic::TBioTreeNode &node) const
    {
        return m_LabelFormat.FormatLabel(node.GetValue().features);
    }
private:
    CBioTreeFormatLabel m_LabelFormat;
};

END_NCBI_SCOPE

#endif
