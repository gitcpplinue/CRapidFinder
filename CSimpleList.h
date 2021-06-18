#pragma once
#include<windows.h>


/* 
* 链表类，只提供链表操作，不负责结点内存的申请和释放。
* 因为不知道结点具体成员变量的名称，所以使用偏移量的方法获取"p->next"指针。
*/
class CSimpleList
{
public:
	void* m_pHead; // 链表头结点
	size_t m_nNextOffset; // 链表结点的pNext指针相对于结点地址的偏移量
	

	CSimpleList(int nNextOffset = 0)
    {
        m_pHead = NULL;
        m_nNextOffset = nNextOffset;
    }

    // 头插法插入新结点p
	void AddHead(void* p);

    // 从链表中删除结点p
	BOOL Remove(void* p);

    // 为m_nNextOffset赋值
	void SetNextOffset(int nNextOffset) { m_nNextOffset = nNextOffset; }

    // 检查链表是否为空
	BOOL IsEmpty() const { return m_pHead == NULL; }

    // 清空链表
	void RemoveAll() { m_pHead = NULL; }

    // 获取链表头结点
	void* GetHead() const { return m_pHead; }

	// 间接调用GetNextPtr，得到"preElement所指结点"的"下一个结点的指针"
	void* GetNext(void* preElement) const { return *GetNextPtr(preElement); }

	// 传入指针p，返回指向指针"p->next"的指针
	void** GetNextPtr(void* p) const { return (void**)((BYTE*)p + m_nNextOffset); }
};




// 用模板实现CSimpleList，省略使用时的(void*)转换
template<class TYPE>
class CTypeSimpleList : public CSimpleList
{
public:
	CTypeSimpleList(int nNextOffset = 0)
		:CSimpleList(nNextOffset)
	{ }

	void AddHead(TYPE p) { CSimpleList::AddHead((void*)p); }

	TYPE GetHead() { return (TYPE)CSimpleList::GetHead(); }

	TYPE GetNext(TYPE p) { return (TYPE)CSimpleList::GetNext(p); }

	BOOL Remove(TYPE p) { return CSimpleList::Remove(p); }

    // 类型转换函数。可以直接使用CTypeSimpleList对象为TYPE类型数据赋值
	operator TYPE() { return (TYPE)CSimpleList::GetHead(); }
};


