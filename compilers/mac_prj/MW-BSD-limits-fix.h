/*
 * $Id$
 *
 * CodeWarrior prefix file for BSD builds to fix bug in Apple's machine/limits.h
 */
#ifdef __MWERKS__
# ifndef __CHAR_BIT__ 
#  define __CHAR_BIT__ 8
# endif
# ifndef __SCHAR_MAX__ 
#  define __SCHAR_MAX__ 255
# endif
#endif