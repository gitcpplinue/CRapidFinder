#include "_AFXTLS_.h"
#include<process.h>


// --------------------- CSimpleList ---------------------
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
// --------------------- CSimpleList ---------------------


// --------------------- CNoTrackObject ---------------------
void* CNoTrackObject::operator new(size_t nSize)
{
	void* p = ::GlobalAlloc(GPTR, nSize);
	return p;
}

void CNoTrackObject::operator delete(void* p)
{
	if (!p)
		::GlobalFree(p);
}
// --------------------- CNoTrackObject ---------------------




// 在栈中申请能容纳1个CThreadSlotData对象的空间
BYTE __afxThreadData[sizeof(CThreadSlotData)];

// 作为全局变量使用
CThreadSlotData* _afxThreadData;





// --------------------- CThreadSlotData ---------------------
// 构造函数
CThreadSlotData::CThreadSlotData()
{
    m_tlsIndex = ::TlsAlloc();
	m_nAlloc = 0;
	m_nMax = 0;
	m_nRover = 1; // 将第1个槽留空，从第2个槽（下标1）开始分配
	m_pSlotData = NULL;

	// 设置偏移量
	m_list.SetNextOffset(offsetof(CThreadData, pNext));
    
	// 初始化临界区
	::InitializeCriticalSection(&m_cs);
}



int CThreadSlotData::AllocSlot()
{
	::EnterCriticalSection(&m_cs);	// 进入临界区

	int nAlloc = m_nAlloc;	// 获取槽的总数
	int nSlot = m_nRover;	// 获取当前的槽的索引值

	// 如果槽的索引值大于槽的总数 || 当前索引值定位到的槽不可用
	if (nSlot >= nAlloc || m_pSlotData[nSlot].dwFlags & SLOT_USED)
	{
		// 从头（略过0）遍历槽数组，将nSlot重定位到第1个找到的可用槽位置
		for (nSlot = 1; nSlot < nAlloc && m_pSlotData[nSlot].dwFlags & SLOT_USED; ++nSlot);

		// 循环结束了，没找到可用槽，意味着所有槽都已被使用（或是第一次使用，还未建立槽数组）
		if (nSlot >= nAlloc)
		{
			// 以32为单位扩容
			int nNewAlloc = nAlloc + 32;
			HGLOBAL hSlotData;	// HGLOBAL类型为内存句柄，是HANDLE的别名

			// 如果是第一次使用CThreadSlotData对象（此时槽数组还没建立）
			if (m_pSlotData == NULL)
				// 申请32个CSlotData对象大小的可移动内存
				hSlotData = ::GlobalAlloc(GMEM_MOVEABLE, nNewAlloc * sizeof(CSlotData));
			else // 如果不是第一次使用，没有空闲槽，原地扩容
			{
				// 获取已分配的内存首地址
				hSlotData = ::GlobalHandle(m_pSlotData);

				// 解锁内存
				::GlobalUnlock(hSlotData);

				// 重新分配内存，新分配的内存大小为扩容后的大小
				hSlotData = ::GlobalReAlloc(hSlotData, nNewAlloc * sizeof(CSlotData), GMEM_MOVEABLE);
			}
			// 无论是刚创建槽数组，还是重新分配了内存，都将内存锁定到物理内存
			CSlotData* pSlotData = (CSlotData*)::GlobalLock(hSlotData);

			// 将新申请的空间初始化为0。
			// pSlotData + m_nAlloc: 新申请部分的内存的首地址
			memset(pSlotData + m_nAlloc, 0, (nNewAlloc - nAlloc) * sizeof(CSlotData));
			
            // 更新槽数组大小和槽数组首地址
			m_nAlloc = nNewAlloc;
			m_pSlotData = pSlotData;
		}
	}

	// 更新线程的局部存储的数组大小m_nMax
	if (nSlot > m_nMax)
		m_nMax = nSlot;

	m_pSlotData[nSlot].dwFlags |= SLOT_USED; // 将nSlot定位的槽置为“已使用”
	m_nRover = nSlot + 1;	// 更新索引值，总是假设下一个槽未被使用

	::LeaveCriticalSection(&m_cs); // 退出临界区

	return nSlot;	// 返回的槽号供被其他成员函数使用
}



void CThreadSlotData::FreeSlot(int nSlot)
{
	::EnterCriticalSection(&m_cs);

	// CTypeSimpleList类重载了operator TYPE()函数，下方代码直接得到链表头结点
	CThreadData* pThreadMember = m_list;

	while (pThreadMember != NULL)
	{	// 确认nSlot的值有效后，执行删除操作
		if (nSlot < pThreadMember->nCount)
		{
            // 直接释放内存，将指针置空
			delete (CNoTrackObject*)pThreadMember->pData[nSlot];
			pThreadMember->pData[nSlot] = NULL;
		}

        // 获取链表中的下一个结点，释放另一个线程的数据
		pThreadMember = pThreadMember->pNext;
	}

	// 将此槽号标识为未被使用
	m_pSlotData[nSlot].dwFlags &= ~SLOT_USED;

	::LeaveCriticalSection(&m_cs);
}



void* CThreadSlotData::GetThreadValue(int nSlot)
{
	// 从TLS获取CThreadData对象，取值
	CThreadData* pThreadMember = (CThreadData*)::TlsGetValue(m_tlsIndex);

    // CThreadData对象实例化 || 槽数值无效
	if (pThreadMember == NULL || nSlot >= pThreadMember->nCount)
		return NULL;

    // 直接返回指向数据的指针    
	return pThreadMember->pData[nSlot];
}



void CThreadSlotData::SetValue(int nSlot, void* pValue)
{
	// 通过TLS获取CThreadData对象
	CThreadData* pThreadMember = (CThreadData*)::TlsGetValue(m_tlsIndex);

	// (如果是第一次使用 || 槽索引值大于数组容量) && 传入了有效值
	if ((pThreadMember == NULL || nSlot >= pThreadMember->nCount) && pValue != NULL)
	{
		// 如果是第一次使用，实例化1个CThreadData对象并插入链表
		if (pThreadMember == NULL)
		{
			pThreadMember = new CThreadData; // 在DeleteValues函数中进行内存释放
			pThreadMember->nCount = 0;
			pThreadMember->pData = NULL;

			// 将刚创建的结点插入链表
			::EnterCriticalSection(&m_cs);
			m_list.AddHead(pThreadMember);
			::LeaveCriticalSection(&m_cs);
		}

		// 如果槽索引值大于数组容量
		// 情况一：第一次使用，结点CThreadData的数组还未创建，初始化数组
		if (pThreadMember->pData == NULL)
			pThreadMember->pData = (void**)::GlobalAlloc(LMEM_FIXED, m_nMax * sizeof(LPVOID));
		else	// 情况二：槽索引值大于数组容量，扩容。
			pThreadMember->pData = (void**)::GlobalReAlloc(pThreadMember->pData,
				m_nMax * sizeof(LPVOID), LMEM_MOVEABLE);
		// 将新申请部分的内存置0
		memset(pThreadMember->pData + pThreadMember->nCount, 0,
			(m_nMax - pThreadMember->nCount) * sizeof(LPVOID));

		// 更新nCount成员变量，更新TLS
		pThreadMember->nCount = m_nMax;
		::TlsSetValue(m_tlsIndex, pThreadMember);
	}

	// 给数组元素赋值
	pThreadMember->pData[nSlot] = pValue;
}



void CThreadSlotData::DeleteValues(HINSTANCE hInst, BOOL bAll)
{
	::EnterCriticalSection(&m_cs);

	if (!bAll)
	{   // 仅删除当前线程的线程局部存储占用的空间
		CThreadData* pThreadMember = (CThreadData*)::TlsGetValue(m_tlsIndex);
		if (pThreadMember != NULL)
			DeleteValues(pThreadMember, hInst);
	}
	else
	{   // 删除所有线程的局部存储
		CThreadData* pThreadMember = m_list;
		while (pThreadMember != NULL)
		{
			CThreadData* pNextData = pThreadMember->pNext;
			DeleteValues(pThreadMember, hInst);
			pThreadMember = pNextData;
		}
	}

	::LeaveCriticalSection(&m_cs);
}



void CThreadSlotData::DeleteValues(CThreadData* pThreadMember, HINSTANCE hInst)
{
	BOOL bDelete = TRUE; // 清空标志位

	// 检查pThreadMember所属线程的所有被占用的槽，匹配模块句柄。
	// hInst的值为NULL表示匹配所有模块
	for (int i = 1; i < pThreadMember->nCount; ++i)
	{
		if (hInst == NULL || m_pSlotData[i].hInst == hInst)
		{
			// 如果占用槽的模块句柄与参数hInst匹配，直接释放空间
			delete (CNoTrackObject*)pThreadMember->pData[i];
			pThreadMember->pData[i] = NULL;
		}
		else
		{
			// 只要遇到有槽被占用且与参数hInst不匹配，标志位bDelete就为FALSE
			// 意思是这个CThreadData对象还存有数据，不能清空！
			if (pThreadMember->pData[i] != NULL)
				bDelete = FALSE;
		}
	}

	if (bDelete)
	{	// 如果bDelete = TRUE，意味着这个CThreadData对象没有保存任何线程局部存储

		// 执行清空操作，将其从链表中移除
		::EnterCriticalSection(&m_cs);
		m_list.Remove(pThreadMember);
		::LeaveCriticalSection(&m_cs);

		// 释放CThreadData对象的pData数组
		::LocalFree(pThreadMember->pData);
		delete pThreadMember;

		// 删除这个线程的TLS索引
		::TlsSetValue(m_tlsIndex, NULL);
	}
}



/*
* 虚构函数要释放掉所有使用的内存，并释放TLS索引m_tlsIndex，移除临界区对象m_cs
*/
CThreadSlotData::~CThreadSlotData()
{
    // 删除所有线程的局部存储
    DeleteValues(NULL, true);

	// 释放m_pSlotData数组
	if (m_pSlotData != NULL)
	{
		// 为什么不直接“::GlobalUnlock(m_pSlotData);”？
		HGLOBAL hSlotData = ::GlobalHandle(m_pSlotData);
		::GlobalUnlock(hSlotData);
		::GlobalFree(m_pSlotData);
	}

	// 释放TLS索引
	if (m_tlsIndex != (DWORD)-1)
		::TlsFree(m_tlsIndex);

	// 移除临界区对象m_cs
	::DeleteCriticalSection(&m_cs);
}
// --------------------- CThreadSlotData ---------------------






// --------------------- CThreadLocalObject ---------------------
CNoTrackObject* CThreadLocalObject::GetData(CNoTrackObject* (*pfnCreateObject)())
{
	if (m_nSlot == 0) // 还未获得槽索引值
	{
		// 返回__afxThreadData作为CThreadSlotData对象的首地址
		if (_afxThreadData == NULL)
			_afxThreadData = new(__afxThreadData)CThreadSlotData;
		
        // 获取一个槽索引值
		m_nSlot = _afxThreadData->AllocSlot();
	}

	// 用槽索引获取线程局部存储
	// 线程局部存储的数据类型为CNoTrackObject类的的派生类对象
	CNoTrackObject* pValue = (CNoTrackObject*)_afxThreadData->GetThreadValue(m_nSlot);

	// 如果线程局部存储还未存有数据，调用pfnCreateObject所指向的构造函数生成一个对象
	if (pValue == NULL)
	{
		pValue = (*pfnCreateObject)();

		// 将新创建的对象的指针存入线程局部存储
		_afxThreadData->SetValue(m_nSlot, pValue);
	}
	// 返回类对象
	return pValue;
}



CNoTrackObject* CThreadLocalObject::GetDataNA()
{
	// 还未获得槽索引值 || 还未生成全局CThreadSlotData对象，返回空
	if (m_nSlot == 0 || _afxThreadData == 0)
		return NULL;
        
	// 获取线程局部存储
	return (CNoTrackObject*)_afxThreadData->GetThreadValue(m_nSlot);
}

CThreadLocalObject::~CThreadLocalObject()
{
	if (m_nSlot != 0 && _afxThreadData != NULL)
		_afxThreadData->FreeSlot(m_nSlot);
	m_nSlot = 0;
}
// --------------------- CThreadLocalObject ---------------------