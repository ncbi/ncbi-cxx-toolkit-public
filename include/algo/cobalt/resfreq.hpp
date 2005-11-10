/* $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's offical duties as a United States Government employee and
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
* ===========================================================================*/

/*****************************************************************************

File name: resfreq.hpp

Author: Jason Papadopoulos

Contents: Interface for CProfileData

******************************************************************************/

#ifndef _ALGO_COBALT_RESFREQ_HPP_
#define _ALGO_COBALT_RESFREQ_HPP_

#include <corelib/ncbifile.hpp>
#include <algo/cobalt/base.hpp>

/// @file resfreq.hpp
/// Interface for CProfileData

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cobalt)

class CProfileData {
public:
    typedef enum {
        eGetResFreqs,
        eGetPssm
    } EMapChoice;

    CProfileData() {}
    ~CProfileData() { Clear(); }

    Int4 * GetSeqOffsets() const { return m_SeqOffsets; }
    Int4 ** GetPssm() const { return m_PssmRows; }
    double ** GetResFreqs() const { return m_ResFreqRows; }

    void Load(EMapChoice choice, 
              string dbname,
              string resfreq_file = "");
    void Clear();

private:
    CMemoryFile *m_ResFreqMmap;
    CMemoryFile *m_PssmMmap;
    Int4 *m_SeqOffsets;
    Int4 **m_PssmRows;
    double **m_ResFreqRows;
};

END_SCOPE(cobalt)
END_NCBI_SCOPE

#endif /* _ALGO_COBALT_RESFREQ_HPP_ */

/*--------------------------------------------------------------------
  $Log$
  Revision 1.1  2005/11/10 15:37:59  papadopo
  Initial version

--------------------------------------------------------------------*/
