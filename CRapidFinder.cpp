#include<stdio.h>
#include "CRapidFinder.h"

// --------------------成员函数--------------------
// 构造函数，初始化各个对象
CRapidFinder::CRapidFinder(int nMaxThread)
    : m_nMaxThread(nMaxThread)
{// m_nMaxThread为const变量，得用列表初始化
    m_nResultCount = 0;
    m_nThreadCount = 0;
    
    m_szMatchName[0] = '\0';

    ::InitializeCriticalSection(&m_cs);

    m_listDir.SetNextOffset(offsetof(CDirectoryNode, pNext));

    m_hDirEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
    m_hExitEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
}

// 析构函数，释放各个对象
CRapidFinder::~CRapidFinder()
{
    ::DeleteCriticalSection(&m_cs);
    ::CloseHandle(m_hDirEvent);
    ::CloseHandle(m_hExitEvent);
}


BOOL CRapidFinder::CheckFile(wchar_t* lpszFileName)
{
    wchar_t strCurrent[MAX_PATH]; // 当前文件的完整路径名
    wchar_t strSearch[MAX_PATH]; // 要匹配的文件名

    // wcscpy_s: strcpy的wchar_t版本
    wcscpy_s(strCurrent, lpszFileName);
    wcscpy_s(strSearch, m_szMatchName);

    // _wcsupr_s: _strupr的wchar_t版本
    _wcsupr_s(strCurrent);
    _wcsupr_s(strSearch);

    // wcsstr: strstr的wchar_t版本
    if (wcsstr(strCurrent, strSearch) != NULL)
        return TRUE;
    return FALSE;
}


// --------------------全局函数--------------------
UINT __stdcall FinderEntry(LPVOID lpParam)
{
    wchar_t findFileName[MAX_PATH];
    CRapidFinder* pFinder = (CRapidFinder*)lpParam;
    CDirectoryNode* pNode = NULL; // 从m_listDir链表中取出的结点
    BOOL bActive = TRUE;	// 当前线程的活跃状态

    // 循环处理m_listDir链表中的目录
    while (1)
    {
        // 互斥使用m_listDir链表，从中获取一个目录结点
        ::EnterCriticalSection(&pFinder->m_cs);
        if (pFinder->m_listDir.IsEmpty())
            bActive = FALSE;
        else
        {	// 从链表中取出第一个结点，并将其从链表中移除
            pNode = pFinder->m_listDir.GetHead();
            pFinder->m_listDir.Remove(pNode);
        }
        ::LeaveCriticalSection(&pFinder->m_cs);

        // 如果m_listDir为空，在m_hDirEvent事件上等待
        if (!bActive)
        {
            ::EnterCriticalSection(&pFinder->m_cs);

            // 递减活跃线程数
            pFinder->m_nThreadCount--;
            // 如果已经没有任何线程处于活跃状态，则不可能再向链表中添加新的目录，文件搜索结束
            if (pFinder->m_nThreadCount == 0)
            {
                ::LeaveCriticalSection(&pFinder->m_cs);
                break;
            }
            ::LeaveCriticalSection(&pFinder->m_cs);

            // 将m_hDirEvent事件设置为非受信状态
            ::ResetEvent(pFinder->m_hDirEvent);
            // 线程阻塞，等待m_hDirEvent事件受信
            ::WaitForSingleObject(pFinder->m_hDirEvent, INFINITE);

            // 结束等待，变成活跃线程，递增活跃线程数
            ::EnterCriticalSection(&pFinder->m_cs);
            pFinder->m_nThreadCount++;
            ::LeaveCriticalSection(&pFinder->m_cs);
            bActive = TRUE;

            // 线程被激活后进入下一次循环，重新尝试获取目录结点
            continue; 
        } // if (!bActive)


        WIN32_FIND_DATA fileData;
        HANDLE hFindFile;

        // 为目录添加 \*.* 后缀，将搜索路径修改为 X:\XXX\*.* 的格式
        if (pNode->szDir[wcslen(pNode->szDir) - 1] != '\\')
            wcscat_s(pNode->szDir, L"\\");
        wcscat_s(pNode->szDir, L"*.*");

        // 搜索当前目录pNode下的所有文件
        hFindFile = ::FindFirstFile(pNode->szDir, &fileData);
        if (hFindFile != INVALID_HANDLE_VALUE)
        {
            do
            {
                // 排除本目录和上级目录
                if (fileData.cFileName[0] == '.')
                    continue;
                // 若检索到一个子目录，添加进目录列表中
                if (fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                {
                    // 为保存新的子目录创建结点
                    CDirectoryNode* p = new CDirectoryNode;
                    // 将子目录的完整路径名记入p->szDir
                    wcsncpy_s(p->szDir, pNode->szDir, wcslen(pNode->szDir) - 3);
                    wcscat_s(p->szDir, fileData.cFileName);

                    ::EnterCriticalSection(&pFinder->m_cs);
                    pFinder->m_listDir.AddHead(p);
                    ::LeaveCriticalSection(&pFinder->m_cs);
                    
                    // 唤醒一个等待的线程，该线程会尝试从链表中获取目录结点，执行搜索操作
                    ::SetEvent(pFinder->m_hDirEvent);
                }
                else // 若检索到一个文件
                {
                    // 检查文件名，如果是要找的文件，增加结果计数、打印文件名。
                    if (pFinder->CheckFile(fileData.cFileName))
                    {
                        ::EnterCriticalSection(&pFinder->m_cs);
                        pFinder->m_nResultCount++;
                        ::LeaveCriticalSection(&pFinder->m_cs);

                        // 打印文件的完整路径名
                        wcsncpy_s(findFileName, pNode->szDir, wcslen(pNode->szDir) - 3);
                        wcscat_s(findFileName, fileData.cFileName);
                        wprintf(L"%ws\n", findFileName);
                    }
                }
            } while (::FindNextFile(hFindFile, &fileData));
        } // if (hFindFile != INVALID_HANDLE_VALUE)

        // 此结点记录的目录已经搜索完毕，释放内存空间，进入下一次循环
        delete pNode;
        pNode = NULL;
    } // while(1)

    // 只有当(pFinder->m_nThreadCount == 0)时才会退出循环
    // 所有目录都搜索完毕，结束搜索操作

    // 唤醒1个线程，使其退出循环；该线程又会唤醒1个新的线程，并让其退出循环……
    ::SetEvent(pFinder->m_hDirEvent); 

    // 阻塞等待时间为0，立即返回
    // 只有最后一个结束循环的线程会执行if语句通知主线程
    if (::WaitForSingleObject(pFinder->m_hDirEvent, 0) != WAIT_TIMEOUT)
        // 通知主线程最后一个线程即将退出，文件搜索完毕
        ::SetEvent(pFinder->m_hExitEvent);

/* ？？？？？？
* 所以说，除了最后一个线程，其余线程在调用line 164的SetEvent时，
受信信号都是被其它线程的WaitForSingleObject接收，
而不是线程本身的line 168的WaitForSingleObject？
* ？？？？？？
*/ 
    return 0;
}