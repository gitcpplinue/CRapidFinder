#include <iostream>
#include<process.h>

#include "CRapidFinder.h"

int main()
{
	CRapidFinder* pFinder = new CRapidFinder(64);
	CDirectoryNode* pNode = new CDirectoryNode;

	wchar_t szPath[] = L"E:\\";
	wchar_t szFile[] = L"qwindows.dll";

	wcscpy_s(pNode->szDir, szPath);
	pFinder->m_listDir.AddHead(pNode);

	wcscpy_s(pFinder->m_szMatchName, szFile);

	UINT uid;

	pFinder->m_nThreadCount = pFinder->m_nMaxThread;
	for (int i = 0; i < pFinder->m_nMaxThread; ++i)
	{
		_beginthreadex(NULL, 0, FinderEntry, (void*)pFinder, 0, &uid);
	}
	::WaitForSingleObject(pFinder->m_hExitEvent, INFINITE);

	printf("搜索到的文件个数为：%d \n", pFinder->m_nResultCount);
	delete pFinder;
}
