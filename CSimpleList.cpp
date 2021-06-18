#include "CSimpleList.h"
#include<process.h>


void CSimpleList::AddHead(void* p)
{
    // 普通的头插法：p->next = head; head = p;
	*GetNextPtr(p) = m_pHead;
	m_pHead = p;
}

BOOL CSimpleList::Remove(void* p)
{
	if (p == NULL)
		return NULL;

    // 函数返回结果
	BOOL bResult = FALSE;

	// 链表没有哑结点，头结点作特殊处理
	if (p == m_pHead)
	{
		m_pHead = *GetNextPtr(p);
		bResult = TRUE;
	}
	else
	{
		void* pTest = m_pHead;
		// 将pTest定位到p结点的前一个结点
		while (pTest != NULL && *GetNextPtr(pTest) != p)
			pTest = *GetNextPtr(pTest);
		// 如果链表中的确存在p结点，将其删除
		if (pTest != NULL)
		{
			*GetNextPtr(pTest) = *GetNextPtr(p);
			bResult = TRUE;
		}
	}
	return bResult;
}
