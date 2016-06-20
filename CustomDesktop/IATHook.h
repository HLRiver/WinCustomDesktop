﻿#pragma once
#include <Windows.h>


template<class FunctionType>
class CIATHook
{
protected:
	FunctionType* m_importAddress = NULL;
	FunctionType m_originalFunction = NULL;


	FunctionType* findImportAddress(HANDLE hookModule, LPCSTR moduleName, LPCSTR functionName)
	{
		// 被hook的模块基址
		uintptr_t hookModuleBase = (uintptr_t)hookModule;
		PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)hookModuleBase;
		PIMAGE_NT_HEADERS ntHeader = (PIMAGE_NT_HEADERS)(hookModuleBase + dosHeader->e_lfanew);
		// 导入表
		PIMAGE_IMPORT_DESCRIPTOR importTable = (PIMAGE_IMPORT_DESCRIPTOR)(hookModuleBase
			+ ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

		// 遍历导入的模块
		for (; importTable->Characteristics != 0; importTable++)
		{
			// 不是函数所在模块
			if (_stricmp((LPCSTR)(hookModuleBase + importTable->Name), moduleName) != 0)
				continue;

			PIMAGE_THUNK_DATA info = (PIMAGE_THUNK_DATA)(hookModuleBase + importTable->OriginalFirstThunk);
			FunctionType* iat = (FunctionType*)(hookModuleBase + importTable->FirstThunk);

			// 遍历导入的函数
			for (; info->u1.AddressOfData != 0; info++, iat++)
			{
				if ((info->u1.Ordinal & IMAGE_ORDINAL_FLAG) == 0) // 是用函数名导入的
				{
					PIMAGE_IMPORT_BY_NAME name = (PIMAGE_IMPORT_BY_NAME)(hookModuleBase + info->u1.AddressOfData);
					if (strcmp(name->Name, functionName) == 0)
						return iat;
				}
			}

			return NULL; // 没找到要hook的函数
		}

		return NULL; // 没找到要hook的模块
	}

public:
	CIATHook(HANDLE hookModule, LPCSTR moduleName, LPCSTR functionName)
	{
		m_importAddress = findImportAddress(hookModule, moduleName, functionName);
		// 保存原函数地址
		if (m_importAddress != NULL)
			m_originalFunction = *m_importAddress;
	}

	virtual ~CIATHook()
	{
		Unhook();
	}


	virtual BOOL Hook(FunctionType hookFunction)
	{
		if (m_importAddress == NULL)
			return FALSE;

		// 修改IAT中地址为hookFunction
		DWORD oldProtect, oldProtect2;
		VirtualProtect(m_importAddress, sizeof(FunctionType), PAGE_READWRITE, &oldProtect);
		*m_importAddress = hookFunction;
		VirtualProtect(m_importAddress, sizeof(FunctionType), oldProtect, &oldProtect2);

		return TRUE;
	}

	virtual BOOL Unhook()
	{
		// 修改回原函数地址
		return Hook(GetOriginalFunction());
	}


	virtual FunctionType GetOriginalFunction()
	{
		return m_originalFunction;
	}
};