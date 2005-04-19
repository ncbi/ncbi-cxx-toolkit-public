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
 * Author:  Charlie Liu
 *
 * File Description:
 *   Define SeqTree and how to read and write it.
 */

#if !defined(CU_TREESTREAM_HPP)
#define CU_TREESTREAM_HPP

//#include <corelib/ncbistre.hpp>
#include <algo/structure/cd_utils/cuSeqtree.hpp>
#include <string>
#include <iostream>
#include <fstream>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)

class CdTreeStream  
{

public:
	static bool readFromFile(std::ifstream& ifs, SeqTree& seqTree);
	static bool writeToFile(std::ofstream&ofs, const SeqTree& seqTree);

    const static int NESTED_INDENT;

	static void fromString(const std::string& strTree, SeqTree& seqTree);
	static std::string toString(const SeqTree& seqTree);
	static std::string toNestedString(const SeqTree& seqTree);

	static void readToDelimiter(std::istream& is, std::string& str);
	CdTreeStream();
	virtual ~CdTreeStream();
private:
    static bool read(std::istream&is, SeqTree& seqTree);
	static bool write(std::ostream&os, const SeqTree& stree,const SeqTree::iterator& cursor);
};

END_SCOPE(cd_utils)
END_NCBI_SCOPE
#endif 

/*
 * ===========================================================================
 * 
 * $Log$
 * Revision 1.1  2005/04/19 14:28:01  lanczyck
 * initial version under algo/structure
 *
 *
 * =========================================================================== 
 */
