/* $Id$
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
 * Author:  Chris Lanczycki
 *
 * File Description:
 *
 *      Concrete subclasses for representing classical guide alignments.
 *      Simplest subclass assumes that masters can be used for two given CDs.
 *
 * ===========================================================================
 */

#ifndef CU_CLASSICALGUIDEALIGNMENT_HPP
#define CU_CLASSICALGUIDEALIGNMENT_HPP

#include <corelib/ncbistd.hpp>
#include <algo/structure/cd_utils/cuGuideAlignment.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)

class MultipleAlignment;

//  This class encapsulates the simplest case:  the creation of a guide alignment between
//  the *masters* of two CDs in a single *validated* classical hierarchy:  this should 
//  always be possible.  The resulting guide is between the masters of
//  the two CDs, mapped to the alignment of their first common ancestor (or the family
//  root if 'MakeGuideToRoot' is used).  The IsOK() method will return 'false' if
//  the guide could not be made.  
//  Consensus masters are not treated specially --> the friend factory class helps hide the
//  details of managing consensus, or getting a guide between a pair of consensus masters.

//  In most cases, it is best to use the CGuideAlignmentFactory class to instantiate 
//  an object of this class.

class NCBI_CDUTILS_EXPORT CMastersClassicalGuideAlignment : public CGuideAlignment_Base
{
    friend class CGuideAlignmentFactory;

//    friend ostream& operator <<(ostream& os, const CGuideAlignment& ga);

public:

    CMastersClassicalGuideAlignment() {
        Initialize();
    }

    virtual ~CMastersClassicalGuideAlignment() {
        Cleanup();
    }

    //  This class deals only with classical hierarchies.
    virtual CDomain_parent::EParent_type GetType() const { return CDomain_parent::eParent_type_classical;}

    //  This specialized class ignores everything except the 'cd' in the guideInputs; 
    //  always uses the master of the specified CDs.  Returns false if the families
    //  in the two SGuideInput arguments differ.
    virtual bool Make(const SGuideInput& guideInput1, const SGuideInput& guideInput2);

    //  Overrides base-class version.
    //  Never uses the cache as the cached alignment may represent a different subfamily than needed.
    virtual bool Make(const CCdCore* cd1, const CCdCore* cd2, CDFamily* family);

    //  Convenience method for when one of the two CDs is the family's root node.
    //  Caches the multiple alignment object if 'cache' is true.
    bool MakeGuideToRoot(const CCdCore* cd, CDFamily* family, bool cache = false);
    static void ClearAlignmentCache();

protected:

    static map<CDFamily*, MultipleAlignment*> m_alignmentCache;

    //  Build the TGuideChain structures.
    virtual void MakeChains(const SGuideInput& guideInput1, const SGuideInput& guideInput2, const CCdCore* commonCd);

private:

    void MakeChains(const CCdCore* cd1, const CCdCore* cd2, const CCdCore* commonCd, CDFamily* family);

    //  In cases where the cache is used, pass the cached alignment.
    bool Make(const CCdCore* cd1, const CCdCore* cd2, MultipleAlignment* ma);

};


END_SCOPE(cd_utils)
END_NCBI_SCOPE


#endif // CU_CLASSICALGUIDEALIGNMENT_HPP
