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




/* 
* 书中“能保证不会发生内存泄漏”，所以不需要new和delete的额外信息。
* 为保证所有线程都使用重写的new和delete，让线程局部存储使用的结构都从此类继承。
*/
class CNoTrackObject
{
public:
    // 调用GlobalAlloc申请堆内存
	void* operator new(size_t nSize);

    // 调用GlobalFree释放内存
	void operator delete(void*);

    // 析构函数设为虚函数
	virtual ~CNoTrackObject() {} 
};






/*
* 将pData所指空间分为多个槽，每个槽放1个线程私有指针，以实现“每个线程通过1个tls索引存放任意多个线程私有指针”
* CThreadData为tls指针所指向的数据类型
* CThreadData也作为链表的结点
*/
struct CThreadData :public CNoTrackObject
{
	CThreadData* pNext; // next指针
	int nCount; // 数组长度
	LPVOID* pData; // 数组首地址，数组的元素指向真正存储数据的堆内存
};

// 用于管理CThreadData::pData数组中的索引号分配
struct CSlotData
{
	DWORD dwFlags; // 标识槽是否被分配
	HINSTANCE hInst; // 占用槽的模块句柄
};
#define SLOT_USED 0X01



class CThreadSlotData
{
public:
	int m_nAlloc;	// m_pSlotData数组长度
	int m_nMax;		// CThreadData中pData数组长度
    int m_nRover;	// 当前可用槽的索引值。为了快速找到一个空闲槽而设定的值

	// 若需要获取单个线程的CThreadData对象，使用m_tlsIndex
	// 若需要获取所有线程的CThreadData对象以执行清理操作，使用m_list
	DWORD m_tlsIndex;// tls索引，供各线程获取CThreadData类型数据
	CTypeSimpleList<CThreadData*> m_list; // 用于管理CThreadData的链表
	CSlotData* m_pSlotData;	// 进程唯一的槽数组的首地址
	

// CThreadSlotData对象作为全局变量使用，需要用到临界区对象m_cs进行线程同步
	CRITICAL_SECTION m_cs;


	CThreadSlotData();
	~CThreadSlotData();

    // 分配槽号
	int AllocSlot();

    /*
    * 真正用户用于存储线程局部存储的空间是GetThreadValue函数返回的指针所指向的内存，
    * CThreadSlotData不负责创建这块空间，但它负责在释放索引的时候释放这块空间所使用的内存。
    * 对1个槽nSlot调用FreeSlot，就释放了所有线程中该槽对应的线程局部存储
    */
	void FreeSlot(int nSlot);

    // 获取线程局部存储指针
	void* GetThreadValue(int nSlot);

    // 给CThreadData::pData数组元素赋值
	// 只修改线程局部存储指针，不负责释放指针原来所指向的内存
	void SetValue(int nSlot, void* pValue);

    // 调用另一个版本的DeleteValues来完成任务
    // hInst为模块，为NULL表示匹配所有模块
    // bAll指示删除操作只针对当前线程还是所有线程
	void DeleteValues(HINSTANCE hInst, BOOL bAll = FALSE);

    // 重载DeleteValues，执行删除操作
	// pData通过tls获取或通过m_list获取，hInst同上
	void DeleteValues(CThreadData* pData, HINSTANCE hInst);

/* ？？？？？？
* 重载new运算符，不为CThreadSlotData对象分配空间，仅返回p指针作为对象的首地址
* 使用例子：
*   _afxThreadData = new(__afxThreadData)CthreadSlotData;
* new的语法：[::]new[placement] new-type-name [new-initializer]
* ？？？？？？
*/ 
	void* operator new(size_t, void* p) { return p; }
};






/*
* CThreadSlotData类没有实现为用户使用的数据分配存储空间的功能。这功能由CThreadLocal类实现。
* CThreadLocal类是最终提供给用户的类模板。
*
* 保存内存地址是一项独立的工作，另外封装CThreadLocalObject类来完成
*/
class CThreadLocalObject
{
private:
	DWORD m_nSlot; // 使用CThreadSlotData类分配的槽号

public:
	// 用于获取线程局部存储数据
    // 使用时传入一个构造函数，如果线程局部存储中没有数据，就用构造函数进行创建。肯定能返回有效值
	CNoTrackObject* GetData(CNoTrackObject* (*pfnCreateObject)());	

    // 和上面的函数功能一样，如果线程局部存储中没有数据，就直接返回NULL
	CNoTrackObject* GetDataNA();	

	~CThreadLocalObject();
};




// 通过模板简化CThreadLocalObject类的使用
template<class TYPE>
class CThreadLocal :public CThreadLocalObject
{
public:
    // 直接返回TYPE类型的默认构造函数
	static CNoTrackObject* CreateObject() { return new TYPE; }

	TYPE* GetData()
	{
		// 函数的返回类型为CNoTrackObject*，TYPE应为CNoTrackObject类的派生类
		TYPE* pData = (TYPE*)CThreadLocalObject::GetData(&CreateObject);

		// 返回一个指向TYPE对象的指针
		return pData;
	}
	TYPE* GetDataNA()
	{
		TYPE* pData = (TYPE*)CThreadLocalObject::GetDataNA();
		return pData;
	}

    // 重载*和->运算符
	operator TYPE* () { return GetData(); }
	TYPE* operator->() { return GetData(); }
};





