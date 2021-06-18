#pragma once
#include "CSimpleList.h"

#define MAX_PATH 500

// 链表结点，用于记录目录名
struct CDirectoryNode //: public CNoTrackObject
{
    CDirectoryNode* pNext; // next指针
    wchar_t szDir[MAX_PATH]; // 要查找的目录
};


class CRapidFinder
{
public:
    int m_nResultCount;     // 结果数目
    int m_nThreadCount;     // 活动线程数目
    const int m_nMaxThread; // 最大线程数目

    wchar_t m_szMatchName[MAX_PATH];   // 要搜索的文件名

    CRITICAL_SECTION m_cs;  // 临界区对象

    CTypeSimpleList<CDirectoryNode*> m_listDir; // 目录列表，记录了当前找到的、还未开始搜索的目录
    
    HANDLE m_hDirEvent;     // 向m_listDir中添加新的目录后置位（受信）
    HANDLE m_hExitEvent;    // 判定查找结束后置位（受信）

public:
    CRapidFinder(int nMaxThread);
    virtual ~CRapidFinder();

    // 检查lpszFileName是否是要搜索的文件，不区分大小写
    BOOL CheckFile(wchar_t* lpszFileName);
};

// 线程函数
UINT __stdcall FinderEntry(LPVOID lpParam);

