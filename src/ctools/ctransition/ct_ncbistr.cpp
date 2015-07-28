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
* File Name:  ncbistr.c
*
* Author:  Gish, Kans, Ostell, Schuler, Brylawski, Vakatov, Lavrentiev
*
* Version Creation Date:   3/4/91
*
* C Tolkit Revision: 6.20
*
* File Description: 
*   	portable string routines
*
* ==========================================================================
*/

#include <ncbi_pch.hpp>
#include <ctools/ctransition/ncbistr.hpp>
#include <ctools/ctransition/ncbimem.hpp>


BEGIN_CTRANSITION_SCOPE


/* ClearDestString clears the destination string if the source is NULL. */
static Nlm_CharPtr NEAR Nlm_ClearDestString (Nlm_CharPtr to, size_t max)
{
  if (to != NULL && max > 0) {
    Nlm_MemSet (to, 0, max);
    *to = '\0';
  }
  return to;
}

NLM_EXTERN size_t LIBCALL  Nlm_StringLen (const char *str)
{
	return str ? StrLen (str) : 0;
}

NLM_EXTERN Nlm_CharPtr LIBCALL  Nlm_StringCpy (char FAR *to, const char FAR *from)
{
	return (to && from) ? StrCpy (to, from) : Nlm_ClearDestString (to, 1);
}

NLM_EXTERN Nlm_CharPtr LIBCALL  Nlm_StringNCpy (char FAR *to, const char FAR *from, size_t max)
{
	return (to && from) ? StrNCpy (to, from, max) : Nlm_ClearDestString (to, max);
}

NLM_EXTERN Nlm_CharPtr LIBCALL  Nlm_StringNCpy_0 (char FAR *to, const char FAR *from, size_t max)
{
    if (!to || !max) {
        return to;
    }
    if (max > 0) {
        to[0] = '\0';
    }
    if (from) {
        StrNCat(to, from, max - 1);
    }
    return to;
}

NLM_EXTERN Nlm_CharPtr LIBCALL  Nlm_StringCat (char FAR *to, const char FAR *from)
{
	return (to && from) ? StrCat (to, from) : to;
}

NLM_EXTERN Nlm_CharPtr LIBCALL  Nlm_StringNCat (char FAR *to, const char FAR *from, size_t max)
{       
    return (to && from) ? StrNCat (to, from, max) : to;
}

NLM_EXTERN int LIBCALL  Nlm_StringCmp (const char FAR *a, const char FAR *b)
{
	return (a && b) ? StrCmp(a, b) : (a ? 1 : (b ? -1 : 0));
}

NLM_EXTERN int LIBCALL  Nlm_StringNCmp (const char FAR *a, const char FAR *b, size_t max)
{
	return (a && b) ? StrNCmp(a, b, max) : (a ? 1 : (b ? -1 : 0));
}

NLM_EXTERN int LIBCALL  Nlm_StringICmp (const char FAR *a, const char FAR *b)
{
	return (a && b) ? Nlm_StrICmp(a, b) : (a ? 1 : (b ? -1 : 0));
}

NLM_EXTERN int LIBCALL  Nlm_StringNICmp (const char FAR *a, const char FAR *b, size_t max)
{
	return (a && b) ? Nlm_StrNICmp(a, b, max) : (a ? 1 : (b ? -1 : 0));
}

NLM_EXTERN Nlm_CharPtr LIBCALL  Nlm_StringChr (const char FAR *str, int chr)
{
	return (Nlm_CharPtr) (str ? Nlm_StrChr(str,chr) : 0);
}

NLM_EXTERN Nlm_CharPtr LIBCALL  Nlm_StringRChr (const char FAR *str, int chr)
{
	return (Nlm_CharPtr) (str ? Nlm_StrRChr(str,chr) : 0);
}

NLM_EXTERN size_t LIBCALL  Nlm_StringSpn (const char FAR *a, const char FAR *b)
{
	return (a && b) ? Nlm_StrSpn (a, b) : 0;
}

NLM_EXTERN size_t LIBCALL  Nlm_StringCSpn (const char FAR *a, const char FAR *b)
{
	return (a && b) ? Nlm_StrCSpn (a, b) : 0;
}

NLM_EXTERN Nlm_CharPtr LIBCALL  Nlm_StringPBrk (const char FAR *a, const char FAR *b)
{
	return (Nlm_CharPtr) ((a && b) ? Nlm_StrPBrk (a, b) : 0);
}

NLM_EXTERN Nlm_CharPtr LIBCALL  Nlm_StringStr (const char FAR *str1, const char FAR *str2)
{
	return (Nlm_CharPtr) ((str1 && str2) ? Nlm_StrStr(str1,str2) : 0);
}

NLM_EXTERN Nlm_CharPtr LIBCALL  Nlm_StringTok (char FAR *str1, const char FAR *str2)
{
    return str2 ? Nlm_StrTok(str1,str2) : 0;
}

NLM_EXTERN Nlm_CharPtr LIBCALL  Nlm_StringTokMT (char FAR *str1, const char FAR *str2, char FAR **tmp)
{
	char ch;
	char FAR *ptr;
	char FAR *rsult = NULL;

	if (str2 == NULL || tmp == NULL) return NULL;
	if (str1 != NULL) {
  	  *tmp = str1;
    }
    ptr = *tmp;
    if (ptr == NULL) return NULL;
    ch = *ptr;
    while (ch != '\0' && strchr (str2, ch) != NULL) {
      ptr++;
      ch = *ptr;
    }
    if (ch == '\0') {
      *tmp = NULL;
      return NULL;
    }
    rsult = ptr;
    while (ch != '\0' && strchr (str2, ch) == NULL) {
      ptr++;
      ch = *ptr;
    }
    if (ch == '\0') {
      *tmp = NULL;
    } else {
      *ptr = '\0';
      ptr++;
      *tmp = ptr;
    }
    return rsult; 
}

NLM_EXTERN Nlm_CharPtr LIBCALL  Nlm_StringMove (char FAR *to, const char FAR *from)
{
    return (to && from) ? Nlm_StrMove (to, from) : to;
}

NLM_EXTERN Nlm_CharPtr LIBCALL  Nlm_StringSave (const char FAR *from)
{
    return from ? Nlm_StrSave (from) : 0;
}

NLM_EXTERN Nlm_CharPtr LIBCALL  Nlm_StringSaveNoNull (const char FAR *from)
{
    return (from  && *from) ? Nlm_StrSave(from) : 0;
}

NLM_EXTERN size_t LIBCALL  Nlm_StringCnt (const char FAR *str, const char FAR *x_list)
{
    return (str && x_list) ? Nlm_StrCnt(str,x_list) : 0;
}

NLM_EXTERN char * LIBCALL Nlm_StringUpper (char *string)
{
	return (string == NULL) ? 0 : StrUpper(string);
}

NLM_EXTERN char * LIBCALL Nlm_StringLower (char *string)
{
	return (string == NULL) ? 0 : StrLower(string);
}





NLM_EXTERN int LIBCALL  Nlm_StrICmp (const char FAR *a, const char FAR *b)
{
	int diff, done;

    if (a == b)   return 0;

	done = 0;
	while (! done)
	{
		diff = TO_UPPER(*a) - TO_UPPER(*b);
		if (diff)
			return (Nlm_Int2) diff;
		if (*a == '\0')
			done = 1;
		else
		{
			a++; b++;
		}
	}
	return 0;
}

NLM_EXTERN int LIBCALL  Nlm_StrIPCmp (const char FAR *a, const char FAR *b)
{
  int diff, done;
  
  if (a == b)   return 0;
  
  while (*a && !isalnum(*a))
    a++;
  
  while (*b && !isalnum(*b))
    b++;
  
  done = 0;
  while (! done)
    {
      if (!isalnum(*a) && !isalnum(*b))
	{
	  while (*a && !isalnum(*a))
	    a++;
	  
	  while (*b && !isalnum(*b))
	    b++;
	}
      
      diff = TO_UPPER(*a) - TO_UPPER(*b);
      if (diff)
	return (Nlm_Int2) diff;
      if (*a == '\0')
	done = 1;
      else
	{
	  a++; b++;
	}
    }
  return 0;
}

NLM_EXTERN int LIBCALL  Nlm_StrNICmp (const char FAR *a, const char FAR *b, size_t max)
{
	int diff, done;
	
    if (a == b)   return 0;

	done = 0;
	while (! done)
	{
		diff = TO_UPPER(*a) - TO_UPPER(*b);
		if (diff)
			return (Nlm_Int2) diff;
		if (*a == '\0')
			done = 1;
		else
		{
			a++; b++; max--;
			if (! max)
				done = 1;
		}
	}
	return 0;
}

NLM_EXTERN int LIBCALL  Nlm_StrNIPCmp (const char FAR *a, const char FAR *b, size_t max)
{
  int diff, done;
  
  if (a == b)   return 0;
  
  while (*a && !isalnum(*a))
    a++;
  
  while (*b && !isalnum(*b))
    b++;
  
  done = 0;
  while (! done)
    {
      if (!isalnum(*a) && !isalnum(*b))
	{
	  while (*a && !isalnum(*a))
	    {
	      a++; max--;
	      if (!max)
		{
		  done = 1;
		  break;
		}
	    }
	  
	  while (*b && !isalnum(*b))
	    b++;
	}
      
      
      diff = TO_UPPER(*a) - TO_UPPER(*b);
      if (diff)
	return (Nlm_Int2) diff;
      if (*a == '\0')
	done = 1;
      else
	{
	  a++; b++; max--;
	  if (! max)
	    done = 1;
	}
    }
  return 0;
}

NLM_EXTERN Nlm_CharPtr LIBCALL  Nlm_StrSave (const char FAR *from)
{
	size_t len;
	Nlm_CharPtr to;

	len = Nlm_StringLen(from);
	if ((to = (Nlm_CharPtr) Nlm_MemGet(len +1, FALSE)) != NULL) {
    	Nlm_MemCpy (to, from, len +1);
	}
	return to;
}

NLM_EXTERN Nlm_CharPtr LIBCALL  Nlm_StrMove (char FAR *to, const char FAR *from)
{
	while (*from != '\0')
	{
		*to = *from;
		to++; from++;
	}
	*to = '\0';
	return to;
}

NLM_EXTERN Nlm_Boolean LIBCALL Nlm_StringHasNoText (const char FAR *str)
{
  if (str) {
    while (*str) {
      if ((unsigned char)(*str++) > ' ')
        return FALSE;
    }
  }
  return TRUE;
}

NLM_EXTERN Nlm_Boolean LIBCALL Nlm_StringDoesHaveText (const char FAR *str)
{
  return ! Nlm_StringHasNoText (str);
}

NLM_EXTERN Nlm_CharPtr LIBCALL Nlm_TrimSpacesAroundString (Nlm_CharPtr str)

{
  Nlm_Uchar    ch;	/* to use 8bit characters in multibyte languages */
  Nlm_CharPtr  dst;
  Nlm_CharPtr  ptr;

  if (str != NULL && str [0] != '\0') {
    dst = str;
    ptr = str;
    ch = *ptr;
    while (ch != '\0' && ch <= ' ') {
      ptr++;
      ch = *ptr;
    }
    while (ch != '\0') {
      *dst = ch;
      dst++;
      ptr++;
      ch = *ptr;
    }
    *dst = '\0';
    dst = NULL;
    ptr = str;
    ch = *ptr;
    while (ch != '\0') {
      if (ch > ' ') {
        dst = NULL;
      } else if (dst == NULL) {
        dst = ptr;
      }
      ptr++;
      ch = *ptr;
    }
    if (dst != NULL) {
      *dst = '\0';
    }
  }
  return str;
}

NLM_EXTERN Nlm_CharPtr LIBCALL Nlm_CompressSpaces (Nlm_CharPtr str)

{
  Nlm_Char     ch;
  Nlm_CharPtr  dst;
  Nlm_Char     last;
  Nlm_CharPtr  ptr;

  if (str != NULL && str [0] != '\0') {
    dst = str;
    ptr = str;
    ch = *ptr;
    while (ch != '\0' && ch <= ' ') {
      ptr++;
      ch = *ptr;
    }
    while (ch != '\0') {
      *dst = ch;
      dst++;
      ptr++;
      last = ch;
      ch = *ptr;
      if (ch != '\0' && ch < ' ') {
        *ptr = ' ';
        ch = *ptr;
      }
      while (ch != '\0' && last <= ' ' && ch <= ' ') {
        ptr++;
        ch = *ptr;
      }
    }
    *dst = '\0';
    dst = NULL;
    ptr = str;
    ch = *ptr;
    while (ch != '\0') {
      if (ch != ' ') {
        dst = NULL;
      } else if (dst == NULL) {
        dst = ptr;
      }
      ptr++;
      ch = *ptr;
    }
    if (dst != NULL) {
      *dst = '\0';
    }
  }
  return str;
}

NLM_EXTERN size_t LIBCALL Nlm_StrCnt(const char FAR *s, const char FAR *x_list)
{
	size_t	cmap[1<<CHAR_BIT];
	Nlm_Byte	c;
	size_t	u, cnt;
	const Nlm_Byte *bs = (const Nlm_Byte*)s;
	const Nlm_Byte *blist = (const Nlm_Byte*)x_list;

	if (s == NULL || x_list == NULL)
		return 0;

	for (u = 0; u < DIM(cmap); ++u)
		cmap[u] = 0;
	while (*blist != '\0')
		++cmap[*blist++];

	blist = (Nlm_BytePtr)cmap;

	cnt = 0;
	while ((c = *bs++) != '\0')
		cnt += blist[c];

	return cnt;
}

#ifndef COMP_MSC
NLM_EXTERN char * LIBCALL Nlm_StrUpper (char *string)
{
	register char *p = string;
	_ASSERT(string != NULL);
	while (*p)
	{
		if (isalpha(*p))
			*p = (char)toupper(*p);
		++p;
	}
	return string;
}
#endif

#ifndef COMP_MSC
NLM_EXTERN char * LIBCALL Nlm_StrLower (char *string)
{
	register char *p = string;
	_ASSERT(string != NULL);
	while (*p)
	{
		if (isalpha(*p))
			*p = (char)tolower(*p);
		++p;
	}
	return string;
}
#endif

static int Nlm_DigitRunLength (Nlm_CharPtr str)

{
  Nlm_Char  ch;
  int   len = 0;

  if (str == NULL) return 0;

  ch = *str;
  while (IS_DIGIT (ch)) {
    len++;
    str++;
    ch = *str;
  }

  return len;
}

static int LIBCALL Nlm_NaturalStringCmpEx (Nlm_CharPtr str1, Nlm_CharPtr str2, Nlm_Boolean caseInsensitive)

{
  Nlm_Char  ch1, ch2;
  int   len1, len2;

  if (str1 == NULL) {
    if (str2 == NULL) return 0;
    return 1;
  }
  if (str2 == NULL) return -1;

  ch1 = *str1;
  ch2 = *str2;
  if (caseInsensitive) {
    ch1 = TO_LOWER (ch1);
    ch2 = TO_LOWER (ch2);
  }

  while (ch1 != '\0' && ch2 != '\0') {
    if (IS_DIGIT (ch1) && IS_DIGIT (ch2)) {
      len1 = Nlm_DigitRunLength (str1);
      len2 = Nlm_DigitRunLength (str2);
      if (len1 > len2) {
        return 1;
      } else if (len1 < len2) {
        return -1;
      }
      while (len1 > 0) {
        if (ch1 > ch2) {
          return 1;
        } else if (ch1 < ch2) {
          return -1;
        }

        str1++;
        str2++;
        ch1 = *str1;
        ch2 = *str2;

        if (caseInsensitive) {
          ch1 = TO_LOWER (ch1);
          ch2 = TO_LOWER (ch2);
        }

        len1--;
      }
    } else {
      if (ch1 > ch2) {
        return 1;
      } else if (ch1 < ch2) {
        return -1;
      }

      str1++;
      str2++;
      ch1 = *str1;
      ch2 = *str2;

      if (caseInsensitive) {
        ch1 = TO_LOWER (ch1);
        ch2 = TO_LOWER (ch2);
      }
    }
  }

  if (ch1 == '\0') {
    if (ch2 == '\0') return 0;
    return 1;
  }
  if (ch2 == '\0') return -1;

  return 0;
}

NLM_EXTERN int LIBCALL Nlm_NaturalStringCmp (Nlm_CharPtr str1, Nlm_CharPtr str2)

{
  return Nlm_NaturalStringCmpEx (str1, str2, FALSE);
}

NLM_EXTERN int LIBCALL Nlm_NaturalStringICmp (Nlm_CharPtr str1, Nlm_CharPtr str2)

{
  return Nlm_NaturalStringCmpEx (str1, str2, TRUE);
}

/*  -------------------- MeshStringICmp() --------------------------------
 *  MeshStringICmp compares strings where / takes precedence to space.
 */

NLM_EXTERN Nlm_Int2 LIBCALL Nlm_MeshStringICmp (const char FAR *str1, const char FAR *str2)
{
	Nlm_Char  ch1, ch2;

	if (str1 == NULL)
	{
		if (str2 == NULL)
			return (Nlm_Int2)0;
		else
			return (Nlm_Int2)1;
	}
	else if (str2 == NULL)
		return (Nlm_Int2)-1;

	while ((*str1 >= ' ') && (*str2 >= ' ') && (TO_LOWER(*str1) == TO_LOWER(*str2)))
	{
		str1++; str2++;
	}

	ch1 = *str1;
	ch2 = *str2;
	if ((ch1 < ' ') && (ch2 < ' '))
		return (Nlm_Int2)0;
	else if (ch1 < ' ')
		return (Nlm_Int2)-1;
	else if (ch2 < ' ')
		return (Nlm_Int2)1;

	if (ch1 == '/')
	  ch1 = '\31';
	if (ch2 == '/')
	  ch2 = '\31';

	if (TO_LOWER (ch1) > TO_LOWER (ch2))
	  return (Nlm_Int2)1;
	else if (TO_LOWER (ch1) < TO_LOWER (ch2))
	  return (Nlm_Int2)(-1);
	else
	  return (Nlm_Int2)0;
}

/* StringSearch and StringISearch use the Boyer-Moore algorithm, as described
   in Niklaus Wirth, Algorithms and Data Structures, Prentice- Hall, Inc.,
   Englewood Cliffs, NJ., 1986, p. 69.  The original had an error, where
   UNTIL (j < 0) OR (p[j] # s[i]) should be UNTIL (j < 0) OR (p[j] # s[k]). */

/* New functions allow array data to be precomputed and kept for multiple uses */

NLM_EXTERN Nlm_Boolean Nlm_SetupSubString (
  const char FAR *sub,
  Nlm_Boolean caseCounts,
  Nlm_SubStringData PNTR data
)

{
  int     ch;
  int     j;
  size_t  subLen;

  if (data == NULL) return FALSE;
  MemSet ((Nlm_VoidPtr) data, 0, sizeof (Nlm_SubStringData));
  if (sub == NULL || sub [0] == '\0') return FALSE;

  subLen = Nlm_StringLen (sub);

  for (ch = 0; ch < 256; ch++) {
    data->d [ch] = (int)subLen;
  }
  for (j = 0; j < (int)(subLen - 1); j++) {
    ch = (int) (caseCounts ? sub [j] : TO_UPPER (sub [j]));
    if (ch >= 0 && ch <= 255) {
      data->d [ch] = (int)(subLen - j - 1);
    }
  }

  data->subLen = subLen;
  data->caseCounts = caseCounts;
  data->initialized = TRUE;
  data->sub = sub;

  return TRUE;
}

NLM_EXTERN Nlm_CharPtr Nlm_SearchSubString (
  const char FAR *str,
  Nlm_SubStringData PNTR data
)

{
  Nlm_Boolean     caseCounts;
  int             ch;
  int             i;
  int             j;
  int             k;
  size_t          strLen;
  size_t          subLen;
  const char FAR  *sub;

  if (str == NULL || str [0] == '\0') return NULL;
  if (data == NULL || ! data->initialized) return NULL;

  strLen = Nlm_StringLen (str);
  subLen = data->subLen;
  if (strLen < subLen) return NULL;

  caseCounts = data->caseCounts;
  sub = data->sub;
  if (sub == NULL || sub [0] == '\0') return NULL;

  i = subLen;
  do {
	j = subLen;
	k = i;
	do {
	  k--;
	  j--;
	} while (j >= 0 &&
			 (caseCounts ? sub [j] : TO_UPPER (sub [j])) ==
			 (caseCounts ? str [k] : TO_UPPER (str [k])));
	if (j >= 0) {
	  ch = (int) (caseCounts ? str [i - 1] : TO_UPPER (str [i - 1]));
	  if (ch >= 0 && ch <= 255) {
		i += data->d [ch];
	  } else {
		i++;
	  }
	}
  } while (j >= 0 && i <= (int) strLen);
  if (j < 0) {
	i -= subLen;
	return (Nlm_CharPtr) (str + i);
  }

  return NULL;
}

static Nlm_CharPtr Nlm_FindSubString (const char FAR *str, const char FAR *sub,
                                      Nlm_Boolean caseCounts)

{
  int      ch;
  size_t   d[256];
  int      i;
  int      j;
  int      k;
  size_t   strLen;
  size_t   subLen;

  if (sub != NULL && sub [0] != '\0' && str != NULL && str [0] != '\0') {
    strLen = Nlm_StringLen (str);
    subLen = Nlm_StringLen (sub);
    if (subLen <= strLen) {
      for (ch = 0; ch < 256; ch++) {
        d [ch] = (int)subLen;
      }
      for (j = 0; j < (int)(subLen - 1); j++) {
        ch = (int) (caseCounts ? sub [j] : TO_UPPER (sub [j]));
        if (ch >= 0 && ch <= 255) {
          d [ch] = subLen - j - 1;
        }
      }
      i = subLen;
      do {
        j = subLen;
        k = i;
        do {
          k--;
          j--;
        } while (j >= 0 &&
                 (caseCounts ? sub [j] : TO_UPPER (sub [j])) ==
                 (caseCounts ? str [k] : TO_UPPER (str [k])));
        if (j >= 0) {
          ch = (int) (caseCounts ? str [i - 1] : TO_UPPER (str [i - 1]));
          if (ch >= 0 && ch <= 255) {
            i += d [ch];
          } else {
            i++;
          }
        }
      } while (j >= 0 && i <= (int)strLen);
      if (j < 0) {
        i -= subLen;
        return (Nlm_CharPtr) (str + i);
      }
    }
  }
  return NULL;
}

NLM_EXTERN Nlm_CharPtr LIBCALL Nlm_StringSearch (const char FAR *str, const char FAR *sub)

{
  return Nlm_FindSubString (str, sub, TRUE);
}

NLM_EXTERN Nlm_CharPtr LIBCALL Nlm_StringISearch (const char FAR *str, const char FAR *sub)

{
  return Nlm_FindSubString (str, sub, FALSE);
}

NLM_EXTERN Nlm_Uint1Ptr LIBCALL Uint8ToBytes(Nlm_Uint8 value)
{
    Nlm_Uint1Ptr out_bytes;
    Nlm_Int4 i, mask = 0xFF;
    
    out_bytes = (Nlm_Uint1Ptr)MemNew(8);
    
    for(i = 0; i < 8; i++) {
        out_bytes[i] = (Nlm_Uint1)(value & mask);
        value >>= 8;
    }
    
    return out_bytes;
}

NLM_EXTERN Nlm_Uint8 LIBCALL BytesToUint8(Nlm_Uint1Ptr bytes)
{
    Nlm_Uint8 value;
    Nlm_Int4 i;

    value = 0;
    for(i = 7; i >= 0; i--) {
        value += bytes[i];
        if(i) value <<= 8;
    }
    
    return value;
}
static Nlm_Uint8 s_StringToUint8(const char *str, const char **endptr, int *sgn)
{
    int sign = 0;  /* actual sign */
    Nlm_Uint8 limdiv, limoff, result;
    const char *s, *save;
    unsigned char c;

    /* assume error */
    *endptr = 0;
    if (!str)
        return 0;

    s = str;
    while (IS_WHITESP(*s))
        s++;
    /* empty string - error */
    if (*s == '\0')
        return 0;

    if (*sgn == 1) {
        if (*s == '-') {
            sign = 1;
            s++;
        } else if (*s == '+') {
            s++;
        }        
    }
    save = s;

    limdiv = UINT8_MAX / 10;
    limoff = UINT8_MAX % 10;
    result = 0;
    
    for (c = *s; c; c = *++s) {
        if (!IS_DIGIT(c)) {
            break;
        }
        c -= '0';
        if (result > limdiv || (result == limdiv && c > limoff)) {
            /* overflow */
            return 0;
        }
        result *= 10;
        result += c;
    }

    /* there was no conversion - error */
    if (save == s)
        return 0;

    *sgn = sign;
    *endptr = s;
    return result;
}


NLM_EXTERN Nlm_Uint8 LIBCALL Nlm_StringToUint8(const char* str, const char** endptr)
{
    int sign = 0; /* no sign allowed */
    return s_StringToUint8(str, endptr, &sign);
}


NLM_EXTERN Nlm_Int8  LIBCALL Nlm_StringToInt8(const char* str, const char** endptr)
{
    int sign = 1; /* sign allowed */
    Nlm_Uint8 result = s_StringToUint8(str, endptr, &sign);
    if (*endptr) {
        /* Check for overflow */
        if (result > (sign
                      ? -((Nlm_Uint8)(INT8_MIN + 1)) + 1
                      : (Nlm_Uint8)(INT8_MAX)))
            *endptr = 0;            
    }
    if (!*endptr)
        return 0;
    return sign ? -(Nlm_Int8)result : result;
}


static char *s_Uint8ToString(Nlm_Uint8 value, char *str, size_t str_size)
{
    char buf[32];
    size_t i, j;

    if (!str || str_size < 2)
        return 0;

    for (i = sizeof(buf) - 1; i > 0; i--) {
        buf[i] = (char)(value % 10 + '0');
        value /= 10;
        if (!value)
            break;
    }
    if (!i || (j = sizeof(buf) - i) >= str_size)
        return 0;
    memcpy(str, buf + i, j);
    str[j] = '\0';
    return str;
}


NLM_EXTERN char* LIBCALL Nlm_Uint8ToString(Nlm_Uint8 value, char* str, size_t str_size)
{
    return s_Uint8ToString(value, str, str_size);
}


NLM_EXTERN char* LIBCALL Nlm_Int8ToString (Nlm_Int8  value, char* str, size_t str_size)
{
    Nlm_Uint8 val;
    char *save = str;

    if (value < 0) {
        if (!str || !str_size)
            return 0;
        *str++ = '-';
        str_size--;
        val = -(Nlm_Uint8)(value + 1) + 1;
    } else
        val = value;

    return s_Uint8ToString(val, str, str_size) ? save : 0;
}


NLM_EXTERN Nlm_Boolean Nlm_StringIsAllDigits (
  Nlm_CharPtr str
)

{
  Nlm_Char  ch;

  if (Nlm_StringHasNoText (str)) return FALSE;

  ch = *str;
  while (ch != '\0') {
    if (! IS_DIGIT (ch)) return FALSE;
    str++;
    ch = *str;
  }

  return TRUE;
}

NLM_EXTERN Nlm_Boolean Nlm_StringIsAllUpperCase (
  Nlm_CharPtr str
)

{
  Nlm_Char  ch;

  if (Nlm_StringHasNoText (str)) return FALSE;

  ch = *str;
  while (ch != '\0') {
    if (IS_ALPHA (ch) && IS_LOWER (ch)) return FALSE;
    str++;
    ch = *str;
  }

  return TRUE;
}

NLM_EXTERN Nlm_Boolean Nlm_StringIsAllLowerCase (
  Nlm_CharPtr str
)

{
  Nlm_Char  ch;

  if (Nlm_StringHasNoText (str)) return FALSE;

  ch = *str;
  while (ch != '\0') {
    if (IS_ALPHA (ch) && IS_UPPER (ch)) return FALSE;
    str++;
    ch = *str;
  }

  return TRUE;
}

NLM_EXTERN Nlm_Boolean Nlm_StringIsAllPunctuation (
  Nlm_CharPtr str
)

{
  Nlm_Char  ch;

  if (Nlm_StringHasNoText (str)) return FALSE;

  ch = *str;
  while (ch != '\0') {
    if (StringChr("!\"#%&'()*+,-./:;<=>?[\\]^_{|}~", ch) == NULL) return FALSE;
    str++;
    ch = *str;
  }

  return TRUE;
}


/*****************************************************************************
*
*   LabelCopy (to, from, buflen)
*   	Copies the string "from" into "to" for up to "buflen" chars
*   	if "from" is longer than buflen, makes the last character '>'
*   	always puts one '\0' to terminate the string in to
*       returns number of characters transferred to "to"
*
*****************************************************************************/
NLM_EXTERN Nlm_Uint4 LIBCALL Nlm_LabelCopy (
    Nlm_CharPtr to, 
    Nlm_CharPtr from, 
    Nlm_Uint4 buflen)
{
	Nlm_Uint4 len;


	if ((to == NULL) || (from == NULL) || (buflen == 0)) {
		return 0;
	}

    --buflen; /* reserve last byte for terminating \0 */

	len = buflen;
	while ((*from != '\0') && (buflen))
	{
		*to = *from;
		from++; to++; buflen--;
	}

	if (len && (*from != '\0'))
	{
		*(to - 1) = '>';
	}

	*to = '\0';
	return (Nlm_Uint4)(len - buflen);
}

NLM_EXTERN void LIBCALL Nlm_LabelCopyNext(Nlm_CharPtr PNTR to, Nlm_CharPtr from, Nlm_Uint4 PNTR buflen)
{
	Nlm_Uint4 diff;

	diff = Nlm_LabelCopy(*to, from, *buflen);
	*buflen -= diff; *to += diff;
	
}

/*****************************************************************************
*
*   LabelCopyExtra (to, from, buflen, prefix, suffix)
*   	Copies the string "from" into "to" for up to "buflen" chars
*   	if all together are longer than buflen, makes the last character '>'
*   	always puts one '\0' to terminate the string in to
*       returns number of characters transferred to "to"
*
*   	if not NULL, puts prefix before from and suffix after from
*   		both contained within buflen
*  
*
*****************************************************************************/
NLM_EXTERN Nlm_Uint4 LIBCALL Nlm_LabelCopyExtra (
    Nlm_CharPtr to, 
    Nlm_CharPtr from, 
    Nlm_Uint4 buflen, 
    Nlm_CharPtr prefix, 
    Nlm_CharPtr suffix)
{
	Nlm_Int4 len, diff;

    if ((to == NULL) || (from == NULL) || (buflen == 0)) {
        return 0;
    }

	len = buflen;
	diff = Nlm_LabelCopy(to, prefix, buflen);
	buflen -= diff; to += diff;

	diff = Nlm_LabelCopy(to, from, buflen);
	buflen -= diff; to += diff;

	diff = Nlm_LabelCopy(to, suffix, buflen);
	buflen -= diff;

	return (Nlm_Int4)(len-buflen);
}

#define NEWLINE '\n'
#define SPACE ' '

NLM_EXTERN Nlm_CharPtr LIBCALL StrCpyPtr (char FAR *Dest, char FAR *Start, char FAR *Stop)
/* copies the string from Start (inclusive) to Stop (exclusive) to 
string Dest. Start and Stop MUST point to the same string! 
*/
{
  Nlm_CharPtr To = Dest;
  while ( *Start && ( Start < Stop ) )
    *To++ = *Start++;
  *To = NULLB;
  
  return(Dest);
}

NLM_EXTERN Nlm_CharPtr LIBCALL StrDupPtr(char FAR *Start,char FAR *Stop)
/* copies the string from Start (inclusive) to Stop (exclusive) to 
   a new string and returns its pointer. 
   Start and Stop MUST point to the same string! 
*/
{
  Nlm_CharPtr Dest, DestPtr;
  
  DestPtr = Dest = (Nlm_CharPtr)Nlm_MemGet((Stop - Start + 1),MGET_ERRPOST);
  
  while ( *Start && ( Start < Stop ) )
    *DestPtr++ = *Start++;
  *DestPtr = NULLB;
  return(Dest);
}

NLM_EXTERN Nlm_CharPtr LIBCALL SkipSpaces(char FAR *Line)
/* returns the next non-whitespace position in the line. */
{
  while( (*Line != NULLB) && isspace(*Line) )
    Line++;
  return(Line);
}


NLM_EXTERN Nlm_CharPtr LIBCALL SkipToSpace(char FAR *theString)
/* returns a pointer to the leftmost whitespace character in theString, 
   or to the trailing NULL if no whitespace is found. */
{
  while (*theString && ( ! isspace(*theString) ))
    theString++;
  return(theString);
}


NLM_EXTERN Nlm_CharPtr LIBCALL SkipChar(char FAR *theString,char Char)
/* returns a pointer to the next non-Char character in theString. */
{
  while (*theString && ( *theString == Char ) )
    theString++;
  return(theString);
}


NLM_EXTERN Nlm_CharPtr LIBCALL SkipToChar(char FAR *theString,char Char)
/* returns a pointer to leftmost instance of Char in theString, or to
   the trailing NULL if none found. */
{
  while (*theString && ( *theString != Char ) )
    theString++;
  return(theString);
}


NLM_EXTERN Nlm_CharPtr LIBCALL SkipPastChar(char FAR *theString,char Char)
/* returns a pointer to the next character after the leftmost instance
   of Char in theString, or to the trailing NULL if none found. */
{
  while (*theString && ( *theString != Char ) )
    theString++;

  if (*theString)
    return(theString+1);
  else
    return(theString);
}

NLM_EXTERN Nlm_CharPtr LIBCALL SkipToString(char FAR *theString,char FAR *Find)
/* returns a pointer to leftmost instance of Find in theString, or to
   the trailing NULL if none found. */
{
  char *FindPtr,*theStringPtr;
  
  while (*theString)
    {
      FindPtr = Find;
      theStringPtr = theString;
      while (*FindPtr && ( *FindPtr == *theStringPtr))
	{
	  FindPtr++;
	  theStringPtr++;
	}
      
      if (*FindPtr == '\0')
	return(theString);
      else
	theString++;
    }
  
  return(theString);
}


NLM_EXTERN Nlm_CharPtr LIBCALL NoCaseSkipToString(char FAR *theString,char FAR *Find)
/* returns a pointer to leftmost instance of Find in theString, 
   ignoring case, or to the trailing NULL if none found. */
{
  char *FindPtr,*theStringPtr;
  
  while (*theString)
    {
      FindPtr = Find;
      theStringPtr = theString;
      while (*FindPtr && (toupper(*FindPtr) == toupper(*theStringPtr)))
	{
	  FindPtr++;
	  theStringPtr++;
	}
      
      if (*FindPtr == '\0')
	return(theString);
      else
	theString++;
    }
  
  return(theString);
}


NLM_EXTERN Nlm_CharPtr LIBCALL SkipPastString(char FAR *theString,char FAR *Find)
/* returns a pointer to the next character after the leftmost
   instance of Find in theString, or to the trailing NULL if none found. */
{
  Nlm_CharPtr Ptr = SkipToString(theString,Find);

  if (*Ptr == '\0')
    return (Ptr);

  return (Ptr + Nlm_StringLen(Find));
}

NLM_EXTERN Nlm_CharPtr LIBCALL NoCaseSkipPastString(char FAR *theString,char FAR *Find)
/* returns a pointer to the next character after the leftmost
   instance of Find in theString, ignoring case,
   or to the trailing NULL if none found. */
{
  Nlm_CharPtr Ptr = SkipToString(theString,Find);

  if (*Ptr == '\0')
    return (Ptr);

  return (Ptr + Nlm_StringLen(Find));
}

NLM_EXTERN Nlm_CharPtr LIBCALL SkipSet(char FAR *theString,char FAR *CharSet)
/* returns a pointer to the next character in theString that is
   not in CharSet. */
{
  Nlm_CharPtr CharSetPtr;
  while (*theString)
    {
      CharSetPtr = CharSet;
      while ( *CharSetPtr && *theString != *CharSetPtr )
        CharSetPtr++;
      if ( ! *CharSetPtr )
        return (theString);
      theString++;
    }
  return(theString);
}

NLM_EXTERN Nlm_CharPtr LIBCALL SkipToSet(char FAR *theString,char FAR *CharSet)
/* returns a pointer to leftmost instance of any char in string
   CharSet in theString, or to the trailing NULL if none found. */
{
  Nlm_CharPtr CharSetPtr;
  while (*theString)
    {
      CharSetPtr = CharSet;
      while ( *CharSetPtr && (*theString != *CharSetPtr) )
        CharSetPtr++;
      if ( *CharSetPtr )
        return (theString);
      theString++;
    }
  return(theString);
}


NLM_EXTERN Nlm_Boolean LIBCALL StringSub(char FAR *theString, char Find, char Replace)
/* replaces all instances of the character Find in the given theString with
   the character Replace. It returns TRUE if any characters were 
   replaced, else FALSE.   */
{
  Nlm_Boolean Replaced = FALSE;
  while ( *theString != NULLB )
    {
      if ( *theString == Find )
        {
          *theString = Replace;
          Replaced = TRUE;
        }
      theString++;
    }
  return(Replaced);
}

NLM_EXTERN Nlm_Boolean LIBCALL StringSubSet(char FAR *theString,char FAR *FindSet, char Replace)
/* replaces all instances of any character in string FindSet found in
   theString with the character Replace.  It returns TRUE if any
   characters were replaced, else FALSE. */
{
  Nlm_CharPtr FindPtr;
  Nlm_Boolean Replaced = FALSE;

  while ( *theString != NULLB )
    {
      FindPtr = FindSet;
      while ( *FindPtr != NULLB )
        {
          if (*theString == *FindPtr )
            {
              *theString = Replace;
              Replaced = TRUE;
            }
          FindPtr++;
        }
      theString++;
    }
  return(Replaced);
}

NLM_EXTERN Nlm_Boolean LIBCALL StringSubString(char FAR *theString, char FAR *Find, char FAR *Replace, Nlm_Int4 MaxLength)
/* replaces all non-overlapping instances of the string Find in the
   string theString with the string Replace. The strings do not have to be the 
   same size. The new string is truncated at MaxLength characters
   Including the final NULL). If MaxLength is zero, the string's current
   length is presumed to be the maximum.
   It returns TRUE if any strings were replaced, else FALSE.
   */
{
  Nlm_CharPtr FindPtr,ComparePtr,StringPtr,NewString, NewStringPtr;
  Nlm_Int4 SpaceNeeded,Len;
  Nlm_Boolean Replaced = FALSE;

  if (*Find == NULLB) {
      return(FALSE);
  }

  Len = (Nlm_Int4)Nlm_StringLen(theString);
  SpaceNeeded = MAX( (Nlm_Int4)((Len * Nlm_StringLen(Replace) 
				 * sizeof(Nlm_Char) )
				/ Nlm_StringLen(Find) + 1),Len) + 1;

  NewStringPtr = NewString = (Nlm_CharPtr)
    Nlm_MemGet((size_t)SpaceNeeded, MGET_ERRPOST);

  StringPtr = theString;
  while (*StringPtr != NULLB)
    {
      FindPtr = Find;
      ComparePtr = StringPtr;
      while ( (*FindPtr != NULLB) && (*FindPtr == *ComparePtr) )
	{
	  FindPtr++;
	  ComparePtr++;
	}
      
      /* if we found the entire string, replace it. */
      if (*FindPtr == NULLB)
	{
	  NewStringPtr = StringMove(NewStringPtr,Replace);
	  StringPtr = ComparePtr;
	  Replaced = TRUE;
	}
      else
	/* otherwise, move on to the next character. */
	*NewStringPtr++ = *StringPtr++;
    }
  *NewStringPtr = NULLB;

  if (MaxLength <= 0)
    MaxLength = (Nlm_Int4)strlen(theString) + 1;

  /* Truncate the string, if necessary.*/
  if ((Nlm_Int4)strlen(NewString) >= MaxLength - 1)
    {
      NewString[MaxLength-1] = NULLB;
    }
    
  Nlm_StringCpy(theString,NewString);
  Nlm_MemFree(NewString);

  return(Replaced);
}


NLM_EXTERN Nlm_CharPtr LIBCALL StringEnd(char FAR *theString)
/* This returns a pointer to the terminating null of the given theString.
 */
{
  while (*theString != NULLB)
    theString++;
  return(theString);
}



NLM_EXTERN Nlm_Int4 LIBCALL CountChar(char FAR *theString, char Char)
/* returns the number of times a given character appears in the given
   theString. */
{
  Nlm_Int4 CharCount = 0;

  while(*theString)
    if (*theString++ == Char)
      CharCount++;

  return(CharCount);
}

NLM_EXTERN Nlm_Int4 LIBCALL CountStrings(char FAR *theString, char FAR *Find)
/*  This returns the number of non-overlapping instances of Find
    in theString. */
{
  Nlm_Int4 count = 0;
  Nlm_Int4 Len = (Nlm_Int4)Nlm_StringLen(Find);
  Nlm_CharPtr Ptr = theString;

  for(;;)
    {
      Ptr = SkipToString(Ptr,Find);
      if (*Ptr == NULLB)
        break;

      count++;
      Ptr += Len;
    }

  return count;
}

NLM_EXTERN Nlm_Int4 LIBCALL CountSet(char FAR *theString, char FAR *Set)
/* returns the number of times any one of a given set of characters 
   appears in the given theString. */
{
  Nlm_Int4 CharCount = 0;
  Nlm_CharPtr SetPtr;

  while(*theString)
    {
      SetPtr = Set;

      while(*SetPtr)
	{
	  if (*theString == *SetPtr)
	    {
	      CharCount++;
	      break;
	    }
	  SetPtr++;
	}

      theString++;
    }

  return(CharCount);
}


NLM_EXTERN Nlm_CharPtr LIBCALL StripSpaces(char FAR *Line)
/* returns a pointer to the next nonwhitespace character in the string
and also removes trailing whitespaces. */
{
  Nlm_CharPtr Ptr;

  Line = SkipSpaces(Line);
  if (*Line != NULLB)
    {
      Ptr = StringEnd(Line) - 1;
      while ( (Ptr > Line) && isspace(*Ptr) )
        Ptr--;
      *(Ptr+1) = NULLB;
    }

  return(Line);
}

NLM_EXTERN void LIBCALL CleanSpaces(char FAR *Line)
/* This in-place deletes all leading and trailing whitespace and replaces
   all instances of one or more whitespace characters with one space,
   or one newline if the whitespace contained a newline.
   */
{
  Nlm_Boolean HasNewLine;
  Nlm_CharPtr LinePtr = SkipSpaces(Line);

  while (*LinePtr )
    {
      while ( *LinePtr &&  ! isspace (*LinePtr) )
        *Line++ = *LinePtr++;

      HasNewLine = FALSE;
      while ( isspace(*LinePtr) )
        {
          if (*LinePtr == NEWLINE)
            HasNewLine = TRUE;
          LinePtr++;
        }

      if (HasNewLine)
        *Line++ = NEWLINE;
      else
        if (*LinePtr)
          *Line++ = SPACE;
    }
  *Line = NULLB;
}


NLM_EXTERN Nlm_Int4 LIBCALL StringDiff(char FAR *This, char FAR *That)
/* This returns the character offset where the strings differ, or -1 if
   the strings are the same. */
{
  Nlm_Int4 Diff = 0;

  while (*This && (*This == *That) )
    {
      Diff++;
      This++;
      That++;
    }

  if (*This || *That)
    return(Diff);
  else
    return (-1);
}

NLM_EXTERN Nlm_Int4 LIBCALL StringDiffNum(char FAR *This, char FAR *That, Nlm_Int4 NumChars)
/* returns the character offset where the strings differ, examining only
   the first NumChars Characters. returns -1 if the two substrings 
   examined are equivalent. */
{
  Nlm_Int4 Diff = 0;

  while ((NumChars > 0) && *This &&  (*This == *That) )
    {
      This++;
      That++;
      NumChars--;
      Diff++;
    }

  if ( NumChars && (*This || *That) )
    return(Diff);
  else 
    return(-1);
}

NLM_EXTERN void LIBCALL TruncateString(char FAR *theString, Nlm_Int4 Length)
/* truncates a string to fit into an array of Length characters, 
   including the trailing NULL. */
{
  if((Nlm_Int4)strlen(theString) >= Length - 1)
    theString [Length-1] = NULLB;
}

NLM_EXTERN Nlm_CharPtr LIBCALL TruncateStringCopy(char FAR *theString, Nlm_Int4 Length)
/* Returns a new string consisting of at most the first length-1 
   characters of theString. */
{
  Nlm_CharPtr NewString = (Nlm_CharPtr)MemNew((size_t)Length);

  StrNCpy(NewString, theString, (size_t)(Length - 1));
  NewString[Length-1] = NULLB;
  return NewString;
}


NLM_EXTERN Nlm_Int4 LIBCALL BreakString(char FAR *theString, Nlm_CharPtr PNTR Words)
/* Breaks up a string at each occurrence of one or more spaces, placing
   each substring obtained into the array Words and returning the 
   number of substrings obtained. 
   */
{
  Nlm_CharPtr Start, Stop, *WordPtr;

  Start = SkipSpaces(theString);
  WordPtr = Words;

  while (*Start)
    {
      Stop = SkipToSpace(Start);
      StrCpyPtr(*WordPtr++,Start,Stop);
      Start = SkipSpaces(Stop);
    }

  return((Nlm_Int4) (WordPtr - Words) );
}

NLM_EXTERN void LIBCALL DeleteChar(char FAR *theString,char Delete)
/* removes all instances of the character Delete from the theString. */
{
  Nlm_CharPtr StringPtr = theString;

  while(*StringPtr)
    {
      if (*StringPtr !=  Delete)
	*theString++ = *StringPtr++;
      else
	StringPtr++;
    }

  *theString = NULLB;
}


NLM_EXTERN Nlm_CharPtr LIBCALL Nlm_StringPrintable(const Nlm_Char PNTR str,
                                                   Nlm_Boolean rn_eol)
{
  size_t str_len = 0;
  const Nlm_Char PNTR s;
  Nlm_CharPtr new_str, new_s;

  if ( !str )
    return NULL;

  if ( rn_eol )
    {
      for (s = str;  *s;  s++)
        if (*s == '\n')
          str_len += 2;
        else if (*s == '\t'  ||  IS_PRINT(*s))
          str_len++;
    }
  else
    {
      for (s = str;  *s;  s++)
        if (*s == '\n'  ||  *s == '\t'  ||  IS_PRINT(*s))
          str_len++;
    }

  new_str = (Nlm_CharPtr)Nlm_MemGet(str_len+1, MGET_ERRPOST);
  if ( !new_str )
    return NULL;

  if ( rn_eol )
    {
      for (s = str, new_s = new_str;  *s;  s++)
        if (*s == '\n')
          {
            *new_s++ = '\r';
            *new_s++ = '\n';
          }
        else if (*s == '\t'  ||  IS_PRINT(*s))
          *new_s++ = *s;
    }
  else
    {
      for (s = str, new_s = new_str;  *s;  s++)
        if (*s == '\n'  ||  *s == '\t'  ||  IS_PRINT(*s))
          *new_s++ = *s;
    }
  *new_s = '\0';

  return new_str;
}
        
  
/*****************************************************************************
 *  Text Formatting Functions
 ****************************************************************************/

#define MAX_NO_DASH 2
#define MIN_LEAD    3
#define MIN_TAIL    1

#define SPACE ' '

/* Act like a regular memcpy but replace all space symbols to #SPACE
 */
static void x_memcpy(Nlm_Char FAR PNTR targ, const Nlm_Char FAR PNTR src,
                     size_t n)
{
  for ( ;  n--;  src++)
    {
      _ASSERT ( *src );
      if ( IS_WHITESP(*src) )
        *targ++ = SPACE;
      else
        *targ++ = *src;
    }
}


/* Set of conditions when the decision on the line breaking can be
 * made having only 2 symbols("ch0" and "ch1" -- to the left and to the
 * right of the break, respectively)
 */
static int can_break(Nlm_Char ch0, Nlm_Char ch1)
{
  if (ch1 == '\0'  ||
      IS_WHITESP(ch1)  ||  IS_WHITESP(ch0))
    return 1;

  switch ( ch1 )
    {
    case '(':
    case '[':
    case '{':
      return 1;
    }

  switch ( ch0 )
    {
    case '-':
    case '+':
    case '=':
    case '&':
    case '|':
    case ')':
    case '}':
    case ']':
      if (ch1 != ch0)
        return 1;
      break;
  
    case '\\':
    case '/':
    case '*':
    case ';':
    case ':':
    case ',':
      return 1;

    case '?':
    case '!':
    case '.':
      if (ch1 != '.'  &&  ch1 != '?'  &&  ch1 != '!')
        return 1;
      break;
    }

  return 0;
}


NLM_EXTERN Nlm_CharPtr LIBCALL Nlm_text2stream(const Nlm_Char FAR PNTR str)
{
  int on_space = 0;
  Nlm_CharPtr line, s;

  if ( !str )
    return NULL;

  while (*str  &&  IS_WHITESP( *str ))
    str++;
  if ( !*str )
    return NULL;

  s = line = (Nlm_CharPtr) Nlm_MemNew(Nlm_StringLen(str) + 1);

  for (  ;  *str;  str++)
    {
      if ( IS_WHITESP(*str) )
        {
          if (*str == '\n')
            *s = '\n';
          on_space = 1;
        }
      else
        {
          if ( on_space ) {
            if (*s == '\n'  &&
                s - line > 1  &&  *(s-1) == '-'  &&  IS_ALPHA(*(s-2)))
              {
                *s = '\0';
                s--;  /* eat dash before end-of-line, merge the broken word */
              }
            else
              *s++ = SPACE;

            *s++ = *str;
            on_space = 0;
          }
          else
            *s++ = *str;
        }
    }
  *s = '\0';

  return (Nlm_CharPtr) realloc(line, Nlm_StringLen(line) + 1);
}


NLM_EXTERN size_t Nlm_stream2text(const Nlm_Char FAR PNTR str, size_t max_col,
                                  int PNTR dash)
{
  const Nlm_Char FAR PNTR s;
  const Nlm_Char FAR PNTR sb; /* the nearest breakable position */
  size_t n_lead = 0;
  size_t n_tail = 0;

  size_t len = Nlm_StringLen( str );
  len = max_col < len ? max_col : len;

  *dash = 0;
  if (len == 0  ||  can_break(str[len-1], str[len]))
    return len;

  /* go to the beginning of the last completely fit word */
  for (sb = &str[len-1];
       sb != str  &&  !IS_WHITESP(*sb)  &&  !can_break(*sb, *(sb+1));
       sb--) continue;
  while (sb != str  &&  IS_WHITESP(*sb))
    sb--;

  if (sb == str)
    { /* the first word is longer than "max_col" */
      if (len > MAX_NO_DASH  &&  IS_ALPHA(str[len-1])  &&  IS_ALPHA(str[len]))
        *dash = 1;  /* recommend use dash in the place of last symbol */

      return len;
    }

  /* decide of whether and how to break the last alphabet word */

  /*  count the lead and the tail of the last non-fit word */
  for (s = &str[len];  *s != '\0'  &&  IS_ALPHA(*s);  s++, n_tail++) continue;
  for (s = &str[len-1];  IS_ALPHA(*s);  s--, n_lead++) continue;
  _ASSERT ( s > str );

  /* try to "move" symbols from lead in the sake of tail */
  while (n_lead > MIN_LEAD  &&  n_tail < MIN_TAIL)  {
    n_lead--;
    n_tail++;
  }

  if (n_lead < MIN_LEAD  ||  n_tail < MIN_TAIL)
    { /* no luck this time -- move the whole non-fit word to the next line */
      return (sb - str + 1);
    }
  else
    {
      *dash = 1;
      return (s - str + n_lead + 1);
    }
}


NLM_EXTERN Nlm_CharPtr LIBCALL Nlm_rule_line(const Nlm_Char FAR PNTR str,
                                             size_t len,
                                             enumRuleLine method)
{
  size_t str_len;
  size_t n_space;

  /* allocate and initialize the resulting string */
  Nlm_CharPtr s = (Nlm_CharPtr) Nlm_MemNew(len + 1);
  Nlm_MemSet(s, SPACE, len);
  s[len] = '\0';

  /* skip leading and trailing spaces */
  while ( IS_WHITESP(*str) )
    str++;
  if ( !*str )
    return s;
  for (str_len = Nlm_StringLen( str );  IS_WHITESP(str[str_len-1]); str_len--) continue;


  /* truncate the original string if doesn't fit */
  if (len <= str_len) {
    x_memcpy(s, str, len);
    return s;
  }

  n_space = len - str_len;
  switch ( method )
    {
    case RL_Left:
      {
        x_memcpy(s, str, str_len);
        break;
      }
    case RL_Right:
      {
        x_memcpy(s + n_space, str, str_len);
        break;
      }
    case RL_Spread:
      {
        size_t n_gap = 0;

        int prev_space = 0;
        const Nlm_Char FAR PNTR _str = str;
        size_t i = str_len;
        for ( ;  i--;  _str++)
          {
            _ASSERT ( *_str );
            if ( IS_WHITESP(*_str) )
              {
                if ( !prev_space ) {
                  n_gap++;
                  prev_space = 1;
                }
                n_space++;
              }
            else
              prev_space = 0;
          }
        _ASSERT ( !prev_space );

        if ( n_gap )
          {
            size_t n_div = n_space / n_gap;
            size_t n_mod = n_space % n_gap;

            Nlm_CharPtr _s = s;
            for (_str = str;  *_str; )
              {
                if ( !IS_WHITESP( *_str ) )
                  *_s++ = *_str++;
                else if ( n_space )
                  {
                    size_t n_add = n_div;
                    if (n_mod > 0) {
                      n_add++;
                      n_mod--;
                    }
                    n_space -= n_add;
                    while ( n_add-- )
                      *_s++ = SPACE;

                    for (_str++;  IS_WHITESP(*_str);  _str++) continue;
                  }
                else
                  break;
              }
            _ASSERT ( _s == s + len );
            break;
          }  /* else -- use RL_Center */
      }

    case RL_Center:
      {
        x_memcpy(s + n_space/2, str, str_len);
        break;
      }

    default:
      _TROUBLE;
      Nlm_MemFree( s );
      return 0;
    }

  return s;
}


#ifdef TEST_TEXT_FMT
Nlm_Int2 Nlm_Main( void )
{
#define MAX_COL 24
  Nlm_Int4     argc = Nlm_GetArgc();
  Nlm_CharPtr *argv = Nlm_GetArgv();

  Nlm_Char x_str[MAX_COL * 1024];
  FILE *fp = NULL;
  int n_read;

  FILE *logfile = Nlm_FileOpen("stdout", "w");
  _ASSERT ( logfile );

  if (argc < 2)  {
    fprintf(logfile, "Usage: %s <file_name>\n", argv[0]);
    return 1;
  }

  fp = Nlm_FileOpen(argv[1], "rb");
  if ( !fp ) {
    fprintf(logfile, "Cannot open file: \"%s\"\n", argv[1]);
    return 2;
  }

  n_read = FileRead(x_str, 1, sizeof(x_str) - 1, fp);
  if (n_read < 2 * MAX_COL) {  
    fprintf(logfile, "Too few bytes read from \"%s\": %d\n", argv[1], n_read);
    return 3;
  }

  _ASSERT ( n_read < sizeof(x_str) );
  x_str[n_read] = '\0';

  {{
    size_t max_col = MAX_COL - 1;
    int    inc     = 1;
    enumRuleLine rule_method = RL_Center;

    Nlm_CharPtr str = text2stream( x_str );
    Nlm_CharPtr text_str = str;
    if ( !str ) {  
      fprintf(logfile, "No non-space symbols in \"%s\"\n", argv[1]);
      return 4;
    }
    
    while (*str != '\0')
      {
        Nlm_Char s[MAX_COL + 1];
        int dash = -12345;
        size_t n_print;

        while (*str  &&  IS_WHITESP(*str))
          str++;

        n_print = stream2text(str, max_col, &dash);
        _ASSERT ( (max_col > 0  &&  str  &&  *str)  ==  (n_print > 0) );
        _ASSERT ( n_print <= max_col );
        _ASSERT ( dash != -12345 );

        Nlm_MemCpy(s, str, n_print);
        s[n_print] = '\0';
        _ASSERT ( dash == 0  ||  n_print > 1 );
        if ( dash )
          s[--n_print] = '-';

        {{
          Nlm_CharPtr ruled_str = rule_line(s,
                                            (rule_method == RL_Right ||
                                             rule_method == RL_Center ) ?
                                            MAX_COL : max_col,
                                            rule_method);
          fprintf(logfile, "|%s|\n", ruled_str);
          Nlm_MemFree( ruled_str );
        }}
        
        str += n_print;

        if (max_col == 0  ||  max_col == MAX_COL)
          inc = -inc;
        max_col += inc;

        if (max_col == 0)
          if (rule_method == RL_Spread)
            rule_method = RL_Left;
          else
            rule_method++;
      }

    Nlm_MemFree( text_str );
  }}

  Nlm_FileClose( logfile );
  Nlm_FileClose( fp );
  return 0;
}
#endif  /* TEST_TEXT_FMT */


#ifdef TEST_INT8_CONVERSION
Nlm_Int2 Nlm_Main( void )
{
    char buffer[100];
    char *s;
    const char *p;
    Nlm_Int8 i;
    Nlm_Uint8 j;

    s = Nlm_Int8ToString(0, buffer, sizeof(buffer));
    assert(s != 0);
    printf("0 = %s\n", s);
    s = Nlm_Int8ToString(1, buffer, sizeof(buffer));
    assert(s != 0);
    printf("1 = %s\n", s);
    s = Nlm_Int8ToString(1222222, buffer, sizeof(buffer));
    assert(s != 0);
    printf("1222222 = %s\n", s);
    s = Nlm_Int8ToString(-15, buffer, sizeof(buffer));
    assert(s != 0);
    printf("-15 = %s\n", s);
    s = Nlm_Int8ToString(-15555555, buffer, sizeof(buffer));
    assert(s != 0);
    printf("-15555555 = %s\n", s);
    s = Nlm_Int8ToString(INT8_MAX, buffer, sizeof(buffer));
    assert(s != 0);
    printf("INT8_MAX = %s\n", s);
    s = Nlm_Int8ToString(INT8_MIN, buffer, sizeof(buffer));
    assert(s != 0);
    printf("INT8_MIN = %s\n", s);
    s = Nlm_Int8ToString(UINT8_MAX, buffer, sizeof(buffer));
    assert(s != 0);
    printf("UINT8_MAX = %s\n", s);
    s = Nlm_Uint8ToString(UINT8_MAX, buffer, sizeof(buffer));
    assert(s != 0);
    printf("UINT8_MAX = %s\n", s);

    strcpy(buffer, "9223372036854775807");
    i = Nlm_StringToInt8(buffer, &p);
    assert(p == buffer + strlen(buffer));
    s = Nlm_Int8ToString(i, buffer + strlen(buffer) + 1,
                         sizeof(buffer) - strlen(buffer) - 1);
    assert(s != 0);
    assert(strcmp(buffer, s) == 0);
    printf("INT8_MAX input Ok\n");
    i++;
    s = Nlm_Int8ToString(i, buffer, sizeof(buffer));
    assert(s != 0);
    printf("INT8_MAX+1 = %s\n", s);

    strcpy(buffer, "-9223372036854775808");
    i = Nlm_StringToInt8(buffer, &p);
    assert(p == buffer + strlen(buffer));
    s = Nlm_Int8ToString(i, buffer + strlen(buffer) + 1,
                         sizeof(buffer) - strlen(buffer) - 1);
    assert(s != 0);
    assert(strcmp(buffer, s) == 0);
    printf("INT8_MIN input Ok\n");
    i--;
    s = Nlm_Int8ToString(i, buffer, sizeof(buffer));
    assert(s != 0);
    printf("INT8_MIN-1 = %s\n", s);

    strcpy(buffer, "18446744073709551615");
    j = Nlm_StringToUint8(buffer, &p);
    assert(p == buffer + strlen(buffer));
    s = Nlm_Uint8ToString(j, buffer + strlen(buffer) + 1,
                          sizeof(buffer) - strlen(buffer) - 1);
    assert(s != 0);
    assert(strcmp(buffer, s) == 0);
    printf("UINT8_MAX input Ok\n");
    j++;
    s = Nlm_Uint8ToString(j, buffer, sizeof(buffer));
    assert(s != 0);
    printf("UINT8_MAX+1 = %s\n", s);

    strcpy(buffer, "1234567890abcdef0123546");
    i = Nlm_StringToInt8(buffer, &p);
    assert(p != 0);
    s = Nlm_Int8ToString(i, buffer + strlen(buffer) + 1,
                         sizeof(buffer) - strlen(buffer) - 1);
    assert(s != 0);
    printf("Out of %s only %.*s was accepted as input for Int8 %s\n",
           buffer, (int)(p - buffer), buffer, s);

    strcpy(buffer, "-987654321234567890abcdef0123546");
    i = Nlm_StringToInt8(buffer, &p);
    assert(p != 0);
    s = Nlm_Int8ToString(i, buffer + strlen(buffer) + 1,
                         sizeof(buffer) - strlen(buffer) - 1);
    assert(s != 0);
    printf("Out of %s only %.*s was accepted as input for Int8 %s\n",
           buffer, (int)(p - buffer), buffer, s);

    strcpy(buffer, "987654321234567890abcdef0123546");
    j = Nlm_StringToUint8(buffer, &p);
    assert(p != 0);
    s = Nlm_Uint8ToString(j, buffer + strlen(buffer) + 1,
                          sizeof(buffer) - strlen(buffer) - 1);
    assert(s != 0);
    printf("Out of %s only %.*s was accepted as input for Uint8 %s\n",
           buffer, (int)(p - buffer), buffer, s);

    strcpy(buffer, "-987654321234567890abcdef0123546");
    j = Nlm_StringToUint8(buffer, &p);
    assert(p == 0);
    printf("Conversion of %s (negative) to Uint8 caused error\n", buffer);

    strcpy(buffer, "9223372036854775808");
    i = Nlm_StringToInt8(buffer, &p);
    assert(p == 0);
    printf("Conversion of %s (INT8_MAX + 1) to Int8 caused error\n", buffer);

    strcpy(buffer, "-9223372036854775809");
    i = Nlm_StringToInt8(buffer, &p);
    assert(p == 0);
    printf("Conversion of %s (INT8_MIN - 1) to Int8 caused error\n", buffer);

    strcpy(buffer, "18446744073709551616");
    j = Nlm_StringToUint8(buffer, &p);
    assert(p == 0);
    printf("Conversion of %s (UINT8_MAX + 1) to Uint8 caused error\n", buffer);

    printf("All tests succeeded\n");

    return 0;
}

#endif /* TEST_INT8_CONVERSION */


END_CTRANSITION_SCOPE
