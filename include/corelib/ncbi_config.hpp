#ifndef CORELIB___NCBI_CONFIG__HPP
#define CORELIB___NCBI_CONFIG__HPP

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
 */
/// @file ncbi_config.hpp
/// Parameters initialization model

#include <corelib/ncbi_tree.hpp>

BEGIN_NCBI_SCOPE

/** @addtogroup ModuleConfig
 *
 * @{
 */


/// Instantiation parameters tree.
/// 
/// Plugin manager's intantiation model is based on class factories.
/// Recursive class factory calls are modeled as tree, where specific
/// subtree is responsible for CF parameters
///
typedef CTreePairNode<string, string>  TParamTree;

class CNcbiRegistry;

/// Reconstruct param tree from the application registry
/// @param reg
///     Application registry (loaded from the INI file)
/// @return 
///     Reconstructed tree (caller is responsible for deletion)
///
NCBI_XNCBI_EXPORT
TParamTree* ParamTree_ConvertRegToTree(const CNcbiRegistry&  reg);


/* @} */


END_NCBI_SCOPE

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.3  2004/09/22 18:04:36  kuznets
 * Put doxygen tags in place
 *
 * Revision 1.2  2004/09/22 15:33:30  kuznets
 * MAGIC: rename ncbi_paramtree->ncbi_config
 *
 * Revision 1.1  2004/09/22 13:54:11  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */


#endif  /* CORELIB___NCBI_PARAMTREE__HPP */
