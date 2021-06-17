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

// This file contains both ordinary comments and Doxygen-style comments.
// Ordinary comments, such as this one, are not used by doxygen because they
// don't have a special comment block such as the triple-slash '///'.
// See the Doxygen manual at http://www.stack.nl/~dimitri/doxygen/manual.html
// for complete information on Doxygen-style comments and commands.


// A Doxygen-style file comment is mandatory if you want to add Doxygen
// comments for global objects (functions, typedefs, enums, macros, etc):

/// @file header_template.hpp
/// Brief description of file: this should be a single sentence ending
/// in a period.
///
/// Detailed file description: zero or more sentences about file follow
/// here. If you want, you can leave a blank comment line after the brief
/// description. Another detailed statement, etc.
/// Please note that you must replace header_template.hpp with the 
/// actual name of your header file. 


#include <corelib/ncbistd.hpp>
#include <dir/dir/dir/some_other_header.hpp>


// Change 'Miscellaneous' to be the module name that your header file belongs to
// on the main Doxygen Modules page. See the list of available groups in
// include/common/metamodules_doxygen.h
/** @addtogroup Miscellaneous
 *
 * @{
 */


BEGIN_NCBI_SCOPE


// Note that global objects like the following macro will not be documented
// by doxygen unless the file itself is documented with the @file command.

/// Optional brief description of macro.
#define NCBI_MACRO 1


/// Brief description of macro -- it must be ended with a period.
/// Optional detailed description of a macro.
/// Continuing with detailed description of macro.
#define NCBI_MACRO_ANOTHER 2


/////////////////////////////////////////////////////////////////////////////
///
/// CMyClass
///
/// A brief description of the class (or class template, struct, union) --
/// it must be ended with an empty new line.
///
/// A detailed description of the class -- it follows after an empty
/// line from the above brief description. Note that comments can
/// span several lines and that the three /// are required.

class CMyClass
{
public:
    // Public types

    /// A brief description of an enumerator.
    ///
    /// A more detailed enum description here. Note that you can comment
    /// as shown over here before the class/function/member description or
    /// alternatively follow a member definition with a single line comment
    /// preceded by ///<. Use the style that makes the most sense for the
    /// code.
    enum EMyEnum {
        eVal1,  ///< EMyEnum value eVal1 description. Note the use of ///<
        eVal2,  ///< EMyEnum value eVal2 description.
        eVal3   ///< EMyEnum value eVal3 description.
    };

    /// An enumerator used as a bitmask.
    enum EMyBitEnum {
        eValName1 = (1 << 0),  ///< description for value 0x01.
        eValName2 = (1 << 1),  ///< description for value 0x02.
        eValName3 = (1 << 2)   ///< description for value 0x04.
    };
    typedef unsigned int TMyBitMask;  ///< bit-wise OR of "EMyBitEnum"

    /// Brief description of a function pointer type.
    ///
    /// Detailed description of the function pointer type.
    typedef char* (*FHandler)
        (int start,  ///< argument description 1 -- what the start means
         int stop    ///< argument description 2 -- what the stop  means
         );

    CMyClass(); // trivial constructors typically don't need doxygen comments

    /// A brief description of a non-trivial constructor.
    ///
    /// A detailed description of the constructor. More details.
    /// @param param1
    ///   First parameter description.
    /// @param param2
    ///   Second parameter description.
    CMyClass(int param1, int param2);

    /// A brief description of another constructor.
    CMyClass(TMyBitMask init_mask); ///< parameter description

    ~CMyClass(); // destructors typically don't need doxygen comments

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
    (char          par1,  ///< short description for par1
     const string& par2   ///< short description for par2
     ) = 0;

    // (NOTE:  The use of public data members is
    //         strictly discouraged!
    //         If used they should be well documented!)
    ///  Describe public member here, explain why it's public.
    int    m_PublicData;

protected:
    /// Brief description of a data member -- notice no details are here
    /// since brief description is adequate.
    double m_FooBar;

    /// Brief function description here.
    /// Detailed description here. More description.
    /// @return Return value description here.
    static int ProtectedFunc(char ch); ///< parameter description

private:
    /// Brief member description here.
    int    m_PrivateData;

    /// Brief static member description here.
    static int    sm_PrivateStaticData;

    /// Brief function description here.
    /// Detailed description here. More description.
    /// @return Return value description here.
    double x_PrivateFunc(int some_int = 1); ///< describe parameter here

    // Friends - Doxygen comments for friends should be in their header files.
    friend bool  SomeFriendFunc(void);
    friend class CSomeFriendClass;

    // Prohibit default initialization and assignment
    // -- e.g. when the member-by-member copying is dangerous.

    /// This method is declared as private but is not
    /// implemented to prevent member-wise copying.
    CFooClass(const CFooClass&);

    /// This method is declared as private but is not
    /// implemented to prevent member-wise copying.
    CFooClass& operator= (const CFooClass&);
};


END_NCBI_SCOPE


/* @} */

#endif  /* DIR_DIR_DIR___HEADER_TEMPLATE__HPP */
