#ifndef DIR_DIR_DIR___HEADER_TEMPLATE__HPP
#define DIR_DIR_DIR___HEADER_TEMPLATE__HPP

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
 * Authors:  Firstname Lastname, Firstname Lastname, ....
 *
 */

/// @file header_template.hpp
/// Brief statement about file ending in a period.
///
/// One or more detailed statements about file follow here. If you want, you
/// can leave a blank comment line after the brief statement that ends in a
/// period. Another detailed statement, etc.
/// Please note that you must replace header_template.hpp with the 
/// actual name of your header file. 


#include <corelib/ncbistd.hpp>
#include <dir/dir/dir/some_other_header.hpp>


/** @addtogroup Miscellaneous
 *
 * @{
 */


BEGIN_NCBI_SCOPE


// Ordinary comment not used by doxygen, because it does not have a special
// comment block such as the ///


/// Optional brief description of macro.
#define NCBI_MACRO 1


/// Brief description of macro -- it must be ended with a period.
/// Optional detailed description of a macro.
/// Continuing with detailed description of macro.
#define NCBI_MACRO_ANOTHER 2


/// A brief description of the class (or class template, struct, union) --
/// it must be ended with an empty new line.
///
/// A detailed description of the class -- it follows after an empty
/// line from the above brief description. Note that comments can
/// span several lines and that the three /// are required.

class CMyClass
{
public:
    /// A brief description of the constructor.
    ///
    /// A detailed description of the constructor.
    CMyClass();

    /// A brief description of the destructor.
    ///
    /// A detailed description of the destructor.
    ~CMyClass();

    /// A brief description of TestMe.
    ///
    /// A detailed description of TestMe. Use the following when parameter
    /// descriptions are going to be long, and you are describing a
    /// complex method:
    /// @param foo
    ///   An int value meaning something.
    /// @param bar
    ///   A constant character pointer meaning something.
    /// @return
    ///   The TestMe() results.
    /// @sa CMyClass(), ~CMyClass() and TestMeToo() - see also.
    int TestMe(int foo, const char* bar);

    /// A brief description of TestMeToo.
    ///
    /// Details for TestMeToo. Use this style if the parameter descriptions
    /// are going to be on one line each:
    /// @sa TestMe()
    virtual void TestMeToo
    (char         par1,  ///< short description for par1
     unsigned int par2   ///< short description for par2
     ) = 0;

    /// Brief description of a function pointer type.
    ///
    /// Detailed description of the function pointer type.
    typedef char* (*FHandler)
        (int start,  ///< argument description 1 -- what the start means
         int stop    ///< argument description 2 -- what the stop  means
         );

private:
    /// Brief description of a data member -- notice no details are here
    /// since brief description is adequate.
    double m_FooBar;

    /// A brief description of an enumerator.
    ///
    /// A more detailed enum description over here. Note that you can comment
    /// as shown over here before the class/function/member description or
    /// alternatively follow a member definition with a single line comment
    /// preceded by ///<. Use the style which makes the best sense for the
    /// code.
    enum EMyEnum {
        eVal1,  ///< EMyEnum value eVal1 description. Note the use of ///<
        eVal2,  ///< EMyEnum value eVal2 description.
        eVal3   ///< EMyEnum value eVal3 description.
    };

    /// A pointer to EMyEnum -- brief description.
    ///
    /// Detailed description of m_Foo.
    EMyEnum* m_Foo;

    EMyEnum  m_Bar;  ///< m_Bar brief description - follows definition.
};


END_NCBI_SCOPE


/* @} */

#endif  /* DIR_DIR_DIR___HEADER_TEMPLATE__HPP */
