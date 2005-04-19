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
 *   Read and write a SeqTree.
 */

#include <ncbi_pch.hpp>
#include <algo/structure/cd_utils/cuSeqTreeStream.hpp>
#include <string>
#include <iostream>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)

const int CdTreeStream::NESTED_INDENT = 4;

bool isDelimiter(char ch);

bool isDelimiter(char ch)
{
	return (ch == '(') || (ch == ')') ||(ch == ',') ||(ch == ';');
};

bool CdTreeStream::readFromFile(std::ifstream& ifs, SeqTree& seqTree)
{
    return read(ifs, seqTree);
}

bool CdTreeStream::read(std::istream& is, SeqTree& seqTree)
{
	if (!is.good())
		return false;
	char ch;
	is.get(ch);
	// skip to the first (
	while((ch != '(') && (is.good()))
	{
		is.get(ch);
	}
	if (!is.good())
		return false;
	SeqItem item;
	
	SeqTree::iterator top = seqTree.insert(seqTree.begin(),item); 
	SeqTree::iterator cursor = top;
	std::string nameDist;
	std::string distance;

	std::string delimiters = "(),";
	while(is.get(ch))
	{
		if (!isspace(ch))
		{
			SeqItem tmp1;
			switch (ch)
			{
			case ';':  //end seq tree data
				return true;
				//break;
			case '(':  // start a node
				cursor = seqTree.append_child(cursor, tmp1);
				break;
			case ',':
				// end a sibling, nothing needs done, skip it.
				break;
			case ')':  //end a node; read and set the distance
				readToDelimiter(is,distance);
				if (distance.size() >0)
				{
                    //  allow interior nodes to be named 
                    int colon_loc = distance.find_first_of(":");
                    if (colon_loc > 0) {
                        cursor->name = distance.substr(0, colon_loc);
                        distance = distance.erase(0, colon_loc);
                    } 

					//skip the leading :
                    //if not present, check if dealing w/ a final named internal node
                    if (distance[0] != ':') {
                        int semi_loc = distance.find_first_of(";");
                        if (semi_loc > 0) {
                            cursor->name = distance.substr(0, semi_loc);
                            is.putback(distance[semi_loc]);
                        } else {
                            std::cout<<"length missing";
                        }			
                    }
					else
					{
						distance.erase(0,1);
						double dist = atof(distance.c_str());

                        if (dist < 0.0) {
                            std::cout << "Warning:  negative branch length! " 
                                      << cursor->name << ", D = " << distance << std::endl;
                        }
						//set distance
						cursor->distance = dist;
						if (cursor == top)
						{
							std::cout<<"Warning:  already reached top before processing )";
							return false;
						}
						else
							cursor = seqTree.parent(cursor);
					}
				}
				distance.erase();
				break;
			default:  // non-delimiter starts a new leaf node
				nameDist += ch;
				readToDelimiter(is, nameDist);
				tmp1 = SeqItem(nameDist);
				seqTree.append_child(cursor,tmp1); //don't move cursor
				//reset the word
				nameDist.erase();
			}
		}
	}
	return true;
}

void CdTreeStream::fromString(const std::string& str, SeqTree& stree)
{
    ncbi::CNcbiIstrstream iss(str.c_str(), str.length()+1);

	read(iss, stree);
}

std::string CdTreeStream::toString(const SeqTree& stree)
{
    ncbi::CNcbiOstrstream oss;

	write(oss, stree, stree.begin());
    return ncbi::CNcbiOstrstreamToString(oss);
}

std::string CdTreeStream::toNestedString(const SeqTree& stree)
{
    int nesting_level = 0;
    std::string newString;
    std::string spacer, line;
    std::string s = toString(stree);
    std::string::const_iterator sci = s.begin();

    //  skip to opening paren
    while (*sci != '(' || sci == s.end()) {
        ++sci;
    }

    while (sci != s.end()) {

        //  print '(' on its own line properly indented, outputting any
        //  pre-exiting text first.
        if (*sci == '(') {
            if (nesting_level != 0 && !line.empty()) {
                newString.append(line + "\n");
                line.erase();
            }
            newString.append(std::string(NESTED_INDENT*nesting_level, ' ') + *sci + "\n");
            ++nesting_level;

        //  print ')' followed by any siblings on the same line, outputting
        //  any pre-existing text first.
            
        } else if (*sci == ')') {
            if (!line.empty()) {
                newString.append(line + "\n");
                line.erase();
            }
            --nesting_level;
            line = std::string(NESTED_INDENT*nesting_level, ' ') + *sci;

        //  print any other character; place space after comma for readability
        } else {
            if (line.length() == 0 && nesting_level > 0) {
                line = std::string(NESTED_INDENT*(nesting_level-1) + 2, ' ');
            }
            line += *sci;
            if (*sci == ',') {
                line += ' ';
            }
        }

        //  final closing ';'
        if (*sci == ';') {
            newString.append(line);
            line.erase();
            break;
        }
        ++sci;
    }

    //  if the ';' was forgotten, print out final text
    if (newString[newString.length() - 1] != ';') {
        if (line.length()==0 || line[line.length() - 1] != ';') {
            line.append(";");
        }
        newString.append(line);
    }

    return newString;
}

bool CdTreeStream::writeToFile(std::ofstream&ofs, const SeqTree& stree)
{
	return write(ofs,stree, stree.begin());
}

bool CdTreeStream::write(std::ostream&os, const SeqTree& stree,const SeqTree::iterator& cursor)
{
    double d;
	if (!os.good())
		return false;
	// if leaf, print leaf and return
	if (cursor.number_of_children() == 0)
	{
		//if there is a valid row id, print it first
		if (cursor->rowID >= 0)
			os<<cursor->rowID<<'_';
		os<<cursor->name.c_str()<<':'<<cursor->distance;
		// if this node has sibling, print ","
		if (stree.number_of_siblings(cursor) > 1)
			os<<',';
		return true;
	}
	else
	{ 
		// print (
		os<<'(';
		// print each child
		SeqTree::sibling_iterator sib = cursor.begin();
		while (sib != cursor.end())
		{
			CdTreeStream::write(os,stree,sib);  //recursive
			++sib;
		}
		// print ) <name>:<dist>;  accept negative distances
        d = cursor->distance;
		if (d > 0 || d < 0)
		{
			os<<") " << cursor->name.c_str() << ":"<<d;
			// if this node has sibling, print ","
			if (stree.number_of_siblings(cursor) > 1)
				os<<',';
 		}
		else  //root node has no distance
			os<<") " << cursor->name.c_str() << ";";
	}
	return true;
}
 
void CdTreeStream::readToDelimiter(std::istream& is, std::string& str)
{
	//str.erase();
	char ch;
	while (is.get(ch) && (!isDelimiter(ch)))
		//if (!isspace(ch))
			str += ch;
	if (isDelimiter(ch))
		is.putback(ch);
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CdTreeStream::CdTreeStream()
{

}

CdTreeStream::~CdTreeStream()
{

}

END_SCOPE(cd_utils)
END_NCBI_SCOPE
/*
 * ===========================================================================
 * 
 * $Log$
 * Revision 1.1  2005/04/19 14:27:18  lanczyck
 * initial version under algo/structure
 *
 * 
 * ===========================================================================
 */
