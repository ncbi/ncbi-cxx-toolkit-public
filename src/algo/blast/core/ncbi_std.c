#include <algo/blast/core/blast_def.h> /* for sfree() macro */
#include <algo/blast/core/ncbi_std.h>

void * MemDup (const void *orig, size_t size)
{
	void*	copy;

	if (orig == NULL || size == 0)
		return NULL;

	if ((copy = malloc (size)) == NULL)
		return NULL;

	memcpy(copy, orig, size);
		return copy;
}

/*****************************************************************************
*
*   ListNodeNew(vnp)
*      adds after last node in list if vnp not NULL
*
*****************************************************************************/
ListNode* ListNodeNew (ListNode* vnp)
{
	ListNode* newnode;

	newnode = (ListNode*) calloc(1, sizeof(ListNode));
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
ListNode* ListNodeAdd (ListNode** head)
{
	ListNode* newnode;

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
ListNode* ListNodeAddPointer (ListNode** head, Uint1 choice, 
                                void *value)
{
	ListNode* newnode;

	newnode = ListNodeAdd(head);
	if (newnode != NULL)
	{
		newnode->choice = choice;
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
ListNode* ListNodeCopyStr (ListNode** head, Uint1 choice, char* str)
{
	ListNode* newnode;

	if (str == NULL) return NULL;

	newnode = ListNodeAdd(head);
	if (newnode != NULL)
	{
		newnode->choice = choice;
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
ListNode* ListNodeFree (ListNode* vnp)
{
	ListNode* next;

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
*   ListNodeFreeData(vnp)
*   	frees whole chain of ListNodes
*       frees associated data pointers - BEWARE of this if these are not
*           allocated single memory block structures.
*
*****************************************************************************/
ListNode* ListNodeFreeData (ListNode* vnp)
{
	ListNode* next;

	while (vnp != NULL)
	{
		sfree(vnp->ptr);
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
Int4 ListNodeLen (ListNode* vnp)
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
ListNode* ListNodeSort (ListNode* list, 
               int (*compar )(const void *, const void *))
{
	ListNode* tmp,** head;
	Int4 count, i;

	if (list == NULL) return NULL;
	
	count = ListNodeLen (list);
	head = (ListNode* *) calloc (((size_t) count + 1), sizeof (ListNode*));
	for (tmp = list, i = 0; tmp != NULL && i < count; i++) {
		head [i] = tmp;
		tmp = tmp->next;
	}

	qsort (head, (size_t) count, sizeof (ListNode*), compar);
	for (i = 0; i < count; i++) {
		tmp = head [i];
		tmp->next = head [i + 1];
	}
	list = head [0];
	sfree (head);

	return list;
}


