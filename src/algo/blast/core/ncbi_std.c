#include <ncbi_std.h>

void * MemDup (const void *orig, size_t size)
{
	VoidPtr	copy;

	if (orig == NULL || size == 0)
		return NULL;

	if ((copy = malloc (size)) == NULL)
		abort();

	memcpy(copy, orig, size);
		return copy;
}

#if 0
static CharPtr Nlm_FileBuildPath (CharPtr root, CharPtr sub_path, CharPtr filename)

{
    CharPtr tmp;
    Boolean dir_start = FALSE;

    if (root == NULL)              /* no place to put it */
        return NULL;

    tmp = root;
    if (*tmp != '\0')                /* if not empty */
    {
        had_root = TRUE;
        while (*tmp != '\0')
        {
            tmp++;
        }

        if ((*(tmp - 1) != DIRDELIMCHR) && (dir_start))
        {
            *tmp = DIRDELIMCHR;
            tmp++; *tmp = '\0';
        }
    }

    if (sub_path != NULL)
    {
        if ((dir_start) && (*sub_path == DIRDELIMCHR))
            sub_path++;
        tmp = StringMove(tmp, sub_path);
        if (*(tmp-1) != DIRDELIMCHR)
        {
            *tmp = DIRDELIMCHR;
            tmp++; *tmp = '\0';
        }
    }

    if (filename != NULL)
        StringMove(tmp, filename);

    return root;
}
#endif
Boolean FindPath(const Char* file, const Char* section, const Char* type, Char* buf, Int2 buflen)   /* length of path buffer */
{
  Boolean rsult = FALSE;

  if (file == NULL  ||  section == 0  ||  type == NULL  ||
      buf == NULL  ||  buflen <= 0)
    return FALSE;

#if 0
  /*NlmMutexLockEx( &corelibMutex );*/

  *buf = '\0';
  if (*file != '\0'  &&  *section != '\0'  &&  *type != '\0'  &&
      Nlm_GetAppParam(file, section, type, "", buf, (Nlm_Int2)(buflen - 1))  &&
      *buf != '\0')
    {
      Nlm_FileBuildPath(buf, NULL, NULL);
      rsult = TRUE;
    }

  /*NlmMutexUnlock( corelibMutex );*/
  return rsult;
#endif
  return TRUE;
}
