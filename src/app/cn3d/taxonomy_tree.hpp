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
* Authors:  Paul Thiessen
*
* File Description:
*      taxonomy tree stuff
*
* ===========================================================================
*/

#ifndef CN3D_TAXONOMY_TREE__HPP
#define CN3D_TAXONOMY_TREE__HPP

#include <corelib/ncbistd.hpp>

#include <objects/taxon1/taxon1.hpp>
#include <objects/taxon1/Taxon2_data.hpp>

#include <list>
#include <map>

class wxFrame;


BEGIN_SCOPE(Cn3D)

class BlockMultipleAlignment;
class Sequence;
class MoleculeIdentifier;
class TaxonomyWindow;

class TaxonomyTree
{
public:
    TaxonomyTree(void);
    ~TaxonomyTree(void);

    void ShowTreeForAlignment(wxFrame *parent,
        const BlockMultipleAlignment *alignment, bool abbreviated);

private:
    // cached taxid's by identifier
    typedef std::map < const MoleculeIdentifier * , int > TaxonomyIDMap;
    TaxonomyIDMap taxonomyIDs;

    // cached taxonomy info by taxid
    typedef std::map < int , ncbi::CRef < ncbi::objects::CTaxon2_data > > TaxonomyInfoMap;
    TaxonomyInfoMap taxonomyInfo;

    // cached taxid parents (child->parent)
    typedef std::map < int , int > TaxonomyParentMap;
    TaxonomyParentMap taxonomyParents;

    // taxonomy server class
    ncbi::objects::CTaxon1 taxonomyServer;

    // init server connection if not already established
    bool Init(void);

    // tool windows
    typedef std::list < TaxonomyWindow ** > TaxonomyWindowList;
    TaxonomyWindowList taxonomyWindows;

    // get integer taxid for a sequence
    int GetTaxIDForSequence(const Sequence *seq);

    // get info for taxid
    const ncbi::objects::CTaxon2_data * GetTaxInfoForTaxID(int taxid);

    // get parent for taxid
    int GetParentTaxID(int taxid);
};

END_SCOPE(Cn3D)

#endif // CN3D_TAXONOMY_TREE__HPP
