#ifndef OBJTOOLS__PUBSEQ_GATEWAY__CASSANDRA__PSG_SCOPE_HPP_
#define OBJTOOLS__PUBSEQ_GATEWAY__CASSANDRA__PSG_SCOPE_HPP_

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
 * Authors: Dmitrii Saprykin
 *
 */

#include <corelib/ncbistl.hpp>

#define NCBI_NS_PSG NCBI_NS_NCBI::psg

#define BEGIN_PSG_SCOPE BEGIN_SCOPE(NCBI_NS_PSG)
#define END_PSG_SCOPE END_SCOPE(NCBI_NS_PSG)
#define USING_PSG_SCOPE USING_SCOPE(NCBI_NS_PSG)

#endif  // OBJTOOLS__PUBSEQ_GATEWAY__CASSANDRA__PSG_SCOPE_HPP_
