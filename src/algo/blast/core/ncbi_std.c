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

/*****************************************************************************
*
*   ListNodeNew(vnp)
*      adds after last node in list if vnp not NULL
*
*****************************************************************************/
ListNodePtr ListNodeNew (ListNodePtr vnp)
{
	ListNodePtr newnode;

	newnode = (ListNodePtr) calloc(1, sizeof(ListNode));
	if (vnp != NULL)
    {
        while (vnp->next != NULL)
            vnp = vnp->next;
		vnp->next = newnode;
    }
	return newnode;
}

/*****************************************************************************
*
*   ListNodeAdd(head)
*      adds after last node in list if *head not NULL
*      If *head is NULL, sets it to the new ListNode
*      returns pointer to the NEW node added
*
*****************************************************************************/
ListNodePtr ListNodeAdd (ListNodePtr PNTR head)
{
	ListNodePtr newnode;

	if (head != NULL)
	{
		newnode = ListNodeNew(*head);
		if (*head == NULL)
			*head = newnode;
	}
	else
		newnode = ListNodeNew(NULL);

	return newnode;
}

/*   ListNodeAddPointer (head, choice, value)
*      adds like ListNodeAdd()
*      sets newnode->choice = choice (if choice does not matter, use 0)
*      sets newnode->ptr = value
*
*****************************************************************************/
ListNodePtr ListNodeAddPointer (ListNodePtr PNTR head, Int2 choice, 
                                void *value)
{
	ListNodePtr newnode;

	newnode = ListNodeAdd(head);
	if (newnode != NULL)
	{
		newnode->choice = (Uint1)choice;
		newnode->ptr = value;
	}

	return newnode;
}

/*****************************************************************************
*
*   ListNodeCopyStr (head, choice, str)
*      adds like ListNodeAdd()
*      sets newnode->choice = choice (if choice does not matter, use 0)
*      sets newnode->ptr = str
*         makes a COPY of str
*      if str == NULL, does not add a ListNode
*
*****************************************************************************/
ListNodePtr ListNodeCopyStr (ListNodePtr PNTR head, Int2 choice, CharPtr str)
{
	ListNodePtr newnode;

	if (str == NULL) return NULL;

	newnode = ListNodeAdd(head);
	if (newnode != NULL)
	{
		newnode->choice = (Uint1)choice;
		newnode->ptr = strdup(str);
	}

	return newnode;
}

/*****************************************************************************
*
*   ListNodeFree(vnp)
*   	frees whole chain of ListNodes
*       Does NOT free associated data pointers
*           see ListNodeFreeData()
*
*****************************************************************************/
ListNodePtr ListNodeFree (ListNodePtr vnp)
{
	ListNodePtr next;

	while (vnp != NULL)
	{
		next = vnp->next;
		sfree(vnp);
		vnp = next;
	}
	return NULL;
}

/*****************************************************************************
*
*   ListNodeLen(vnp)
*      returns the number of nodes in the linked list
*
*****************************************************************************/
Int4 ListNodeLen (ListNodePtr vnp)
{
	Int4 len;

	len = 0;
	while (vnp != NULL) {
		len++;
		vnp = vnp->next;
	}
	return len;
}

/*****************************************************************************
*
*   ListNodeSort(list, compar)
*   	Copied from SortListNode in jzcoll, renamed, for more general access
*   	Makes array from ListNode list, calls HeapSort, reconnects ListNode list
*
*****************************************************************************/
ListNodePtr ListNodeSort (ListNodePtr list, 
               int (*compar )(const void *, const void *))
{
	ListNodePtr tmp, PNTR head;
	Int4 count, i;

	if (list == NULL) return NULL;
	
	count = ListNodeLen (list);
	head = (ListNodePtr *) calloc (((size_t) count + 1), sizeof (ListNodePtr));
	for (tmp = list, i = 0; tmp != NULL && i < count; i++) {
		head [i] = tmp;
		tmp = tmp->next;
	}

	qsort (head, (size_t) count, sizeof (ListNodePtr), compar);
	for (i = 0; i < count; i++) {
		tmp = head [i];
		tmp->next = head [i + 1];
	}
	list = head [0];
	sfree (head);

	return list;
}


