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
*      new C++ PSSM construction
*
* ===========================================================================
*/

#ifndef CN3D_PSSM__HPP
#define CN3D_PSSM__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistre.hpp>

#include <vector>

#include <objects/scoremat/PssmWithParameters.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)
class PssmMaker;
END_SCOPE(cd_utils)
END_NCBI_SCOPE


BEGIN_SCOPE(Cn3D)

class BlockMultipleAlignment;

class PSSMWrapper{
private:
    const BlockMultipleAlignment *multiple;
    ncbi::CRef < ncbi::objects::CPssmWithParameters > pssm;

    // store an unpacked matrix for efficient access (e.g. for GetPSSMScore())
    typedef std::vector < int > Column;
    std::vector < Column > scaledMatrix;
    int scalingFactor;
    std::vector < int > master2consensus, consensus2master;
    void UnpackMatrix(ncbi::cd_utils::PssmMaker& pm);

public:
    PSSMWrapper(const BlockMultipleAlignment *bma);

    int GetPSSMScore(unsigned char ncbistdaa, unsigned int realMasterIndex) const;
    void OutputPSSM(ncbi::CNcbiOstream& os, const std::string& title) const;

    const ncbi::objects::CPssmWithParameters& GetPSSM(void) const { return *pssm; }
    int MapConsensusToMaster(unsigned int consensusIndex) const { return consensus2master[consensusIndex]; }
};

END_SCOPE(Cn3D)

#endif // CN3D_PSSM__HPP

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.7  2006/02/10 14:14:59  thiessen
* add user title to pssm
*
* Revision 1.6  2005/12/07 18:58:17  thiessen
* toss my BMA->PSIMsa conversion, use PssmMaker instead to generate consensus-based PSSMs
*
* Revision 1.5  2005/11/05 12:09:40  thiessen
* special handling of B,Z,U
*
* Revision 1.4  2005/11/04 20:45:31  thiessen
* major reorganization to remove all C-toolkit dependencies
*
* Revision 1.3  2005/11/01 02:44:07  thiessen
* fix GCC warnings; switch threader to C++ PSSMs
*
* Revision 1.2  2005/06/07 12:18:52  thiessen
* add PSSM export
*
* Revision 1.1  2005/03/08 17:22:31  thiessen
* apparently working C++ PSSM generation
*
*/
