﻿
/*
Copyright (c) 2009-2014 Maximus5
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the authors may not be used to endorse or promote products
   derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ''AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#ifdef _DEBUG
//  Раскомментировать, чтобы сразу после загрузки плагина показать MessageBox, чтобы прицепиться дебаггером
//  #define SHOW_STARTED_MSGBOX
#endif

#define SHOWDEBUGSTR
//#define MCHKHEAP
#define DEBUGSTRMENU(s) //DEBUGSTR(s)
#define DEBUGSTRINPUT(s) //DEBUGSTR(s)
#define DEBUGSTRDLGEVT(s) //DEBUGSTR(s)
#define DEBUGSTRCMD(s) //DEBUGSTR(s)
#define DEBUGSTRACTIVATE(s) //DEBUGSTR(s)


#include "ConEmuPluginBase.h"
#include "PluginHeader.h"
#include "ConEmuPluginA.h"
#include "ConEmuPlugin995.h"
#include "ConEmuPlugin1900.h"
#include "ConEmuPlugin2800.h"
#include "PluginBackground.h"

extern MOUSE_EVENT_RECORD gLastMouseReadEvent;
extern LONG gnDummyMouseEventFromMacro;
extern BOOL gbUngetDummyMouseEvent;

CPluginBase* gpPlugin = NULL;

/* EXPORTS BEGIN */

#define MIN_FAR2_BUILD 1765 // svs 19.12.2010 22:52:53 +0300 - build 1765: Новая команда в FARMACROCOMMAND - MCMD_GETAREA
#define MAKEFARVERSION(major,minor,build) ( ((major)<<8) | (minor) | ((build)<<16))

int WINAPI GetMinFarVersion()
{
	// Однако, FAR2 до сборки 748 не понимал две версии плагина в одном файле
	bool bFar2 = false;

	if (!LoadFarVersion())
		bFar2 = true;
	else
		bFar2 = gFarVersion.dwVerMajor>=2;

	if (bFar2)
	{
		return MAKEFARVERSION(2,0,MIN_FAR2_BUILD);
	}

	return MAKEFARVERSION(1,71,2470);
}

int WINAPI GetMinFarVersionW()
{
	return MAKEFARVERSION(2,0,MIN_FAR2_BUILD);
}

int WINAPI ProcessSynchroEventW(int Event, void *Param)
{
	return Plugin()->ProcessSynchroEvent(Event, Param);
}

INT_PTR WINAPI ProcessSynchroEventW3(void* p)
{
	return Plugin()->ProcessSynchroEvent(p);
}

int WINAPI ProcessEditorEventW(int Event, void *Param)
{
	if (Event == 2/*EE_REDRAW*/)
		return 0;
	return Plugin()->ProcessEditorViewerEvent(Event, -1);
}

INT_PTR WINAPI ProcessEditorEventW3(void* p)
{
	return Plugin()->ProcessEditorEvent(p);
}

int WINAPI ProcessViewerEventW(int Event, void *Param)
{
	return Plugin()->ProcessEditorViewerEvent(-1, Event);
}

INT_PTR WINAPI ProcessViewerEventW3(void* p)
{
	return Plugin()->ProcessViewerEvent(p);
}

/* EXPORTS END */

CPluginBase* Plugin()
{
	if (!gpPlugin)
	{
		if (!gFarVersion.dwVerMajor)
			LoadFarVersion();

		if (gFarVersion.dwVerMajor==1)
			gpPlugin = new CPluginAnsi();
		else if (gFarVersion.dwBuild>=FAR_Y2_VER)
			gpPlugin = new CPluginW2800();
		else if (gFarVersion.dwBuild>=FAR_Y1_VER)
			gpPlugin = new CPluginW1900();
		else
			gpPlugin = new CPluginW995();
	}

	return gpPlugin;
}

CPluginBase::CPluginBase()
{
	mb_StartupInfoOk = false;

	ee_Read = ee_Save = ee_Redraw = ee_Close = ee_GotFocus = ee_KillFocus = ee_Change = -1;
	ve_Read = ve_Close = ve_GotFocus = ve_KillFocus = -1;
	se_CommonSynchro = -1;
	wt_Desktop = wt_Panels = wt_Viewer = wt_Editor = wt_Dialog = wt_VMenu = wt_Help = -1;
	ma_Other = ma_Shell = ma_Viewer = ma_Editor = ma_Dialog = ma_Search = ma_Disks = ma_MainMenu = ma_Menu = ma_Help = -1;
	ma_InfoPanel = ma_QViewPanel = ma_TreePanel = ma_FindFolder = ma_UserMenu = -1;
	ma_ShellAutoCompletion = ma_DialogAutoCompletion = -1;
	of_LeftDiskMenu = of_PluginsMenu = of_FindList = of_Shortcut = of_CommandLine = of_Editor = of_Viewer = of_FilePanel = of_Dialog = of_Analyse = of_RightDiskMenu = of_FromMacro = -1;

	ms_RootRegKey = NULL;

	InvalidPanelHandle = (gFarVersion.dwVerMajor >= 3) ? NULL : INVALID_HANDLE_VALUE;
}

CPluginBase::~CPluginBase()
{
}

int CPluginBase::ShowMessageGui(int aiMsg, int aiButtons)
{
	wchar_t wszBuf[MAX_PATH];
	LPCWSTR pwszMsg = GetMsg(aiMsg, wszBuf, countof(wszBuf));

	wchar_t szTitle[128];
	_wsprintf(szTitle, SKIPLEN(countof(szTitle)) L"ConEmu plugin (PID=%u)", GetCurrentProcessId());

	if (!pwszMsg || !*pwszMsg)
	{
		_wsprintf(wszBuf, SKIPLEN(countof(wszBuf)) L"<MsgID=%i>", aiMsg);
		pwszMsg = wszBuf;
	}

	int nRc = MessageBoxW(NULL, pwszMsg, szTitle, aiButtons);
	return nRc;
}

void CPluginBase::PostMacro(const wchar_t* asMacro, INPUT_RECORD* apRec)
{
	if (!asMacro || !*asMacro)
		return;

	_ASSERTE(GetCurrentThreadId()==gnMainThreadId);

	MOUSE_EVENT_RECORD mre;

	if (apRec && apRec->EventType == MOUSE_EVENT)
	{
		gLastMouseReadEvent = mre = apRec->Event.MouseEvent;
	}
	else
	{
		mre = gLastMouseReadEvent;
	}

	PostMacroApi(asMacro, apRec);

	//FAR BUGBUG: Макрос не запускается на исполнение, пока мышкой не дернем :(
	//  Это чаще всего проявляется при вызове меню по RClick
	//  Если курсор на другой панели, то RClick сразу по пассивной
	//  не вызывает отрисовку :(

#if 1
	//111002 - попробуем просто gbUngetDummyMouseEvent
	//InterlockedIncrement(&gnDummyMouseEventFromMacro);
	gnDummyMouseEventFromMacro = TRUE;
	gbUngetDummyMouseEvent = TRUE;
#endif
#if 0
	//if (!mcr.Param.PlainText.Flags) {
	INPUT_RECORD ir[2] = {{MOUSE_EVENT},{MOUSE_EVENT}};

	if (isPressed(VK_CAPITAL))
		ir[0].Event.MouseEvent.dwControlKeyState |= CAPSLOCK_ON;

	if (isPressed(VK_NUMLOCK))
		ir[0].Event.MouseEvent.dwControlKeyState |= NUMLOCK_ON;

	if (isPressed(VK_SCROLL))
		ir[0].Event.MouseEvent.dwControlKeyState |= SCROLLLOCK_ON;

	ir[0].Event.MouseEvent.dwEventFlags = MOUSE_MOVED;
	ir[0].Event.MouseEvent.dwMousePosition = mre.dwMousePosition;

	// Вроде одного хватало, правда когда {0,0} посылался
	ir[1].Event.MouseEvent.dwControlKeyState = ir[0].Event.MouseEvent.dwControlKeyState;
	ir[1].Event.MouseEvent.dwEventFlags = MOUSE_MOVED;
	//ir[1].Event.MouseEvent.dwMousePosition.X = 1;
	//ir[1].Event.MouseEvent.dwMousePosition.Y = 1;
	ir[0].Event.MouseEvent.dwMousePosition = mre.dwMousePosition;
	ir[0].Event.MouseEvent.dwMousePosition.X++;

	//2010-01-29 попробуем STD_OUTPUT
	//if (!ghConIn) {
	//	ghConIn  = CreateFile(L"CONIN$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_READ,
	//		0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	//	if (ghConIn == INVALID_HANDLE_VALUE) {
	//		#ifdef _DEBUG
	//		DWORD dwErr = GetLastError();
	//		_ASSERTE(ghConIn!=INVALID_HANDLE_VALUE);
	//		#endif
	//		ghConIn = NULL;
	//		return;
	//	}
	//}
	TODO("Необязательно выполнять реальную запись в консольный буфер. Можно обойтись подстановкой в наших функциях перехвата чтения буфера.");
	HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
	DWORD cbWritten = 0;

	// Вроде одного хватало, правда когда {0,0} посылался
	#ifdef _DEBUG
	BOOL fSuccess =
	#endif
	WriteConsoleInput(hIn/*ghConIn*/, ir, 1, &cbWritten);
	_ASSERTE(fSuccess && cbWritten==1);
	//}
	//InfoW995->AdvControl(InfoW995->ModuleNumber,ACTL_REDRAWALL,NULL);
#endif
}

bool CPluginBase::isMacroActive(int& iMacroActive)
{
	if (!FarHwnd)
	{
		return false;
	}

	if (!iMacroActive)
	{
		iMacroActive = IsMacroActive() ? 1 : 2;
	}

	return (iMacroActive == 1);
}

void CPluginBase::UpdatePanelDirs()
{
	bool bChanged = false;

	_ASSERTE(gPanelDirs.ActiveDir && gPanelDirs.PassiveDir);
	CmdArg* Pnls[] = {gPanelDirs.ActiveDir, gPanelDirs.PassiveDir};

	for (int i = 0; i <= 1; i++)
	{
		GetPanelDirFlags Flags = ((i == 0) ? gpdf_Active : gpdf_Passive) | gpdf_NoPlugin;

		wchar_t* pszDir = GetPanelDir(Flags);

		if (pszDir && (lstrcmp(pszDir, Pnls[i]->ms_Arg ? Pnls[i]->ms_Arg : L"") != 0))
		{
			Pnls[i]->Set(pszDir);
			bChanged = true;
		}
		SafeFree(pszDir);
	}

	if (bChanged)
	{
		// Send to GUI
		SendCurrentDirectory(FarHwnd, gPanelDirs.ActiveDir->ms_Arg, gPanelDirs.PassiveDir->ms_Arg);
	}
}

bool CPluginBase::RunExternalProgram(wchar_t* pszCommand)
{
	wchar_t *pszExpand = NULL;
	CmdArg szTemp, szExpand, szCurDir;

	if (!pszCommand || !*pszCommand)
	{
		if (!InputBox(L"ConEmu", L"Start console program", L"ConEmu.CreateProcess", L"cmd", szTemp.ms_Arg))
			return false;

		pszCommand = szTemp.ms_Arg;
	}

	if (wcschr(pszCommand, L'%'))
	{
		szExpand.ms_Arg = ExpandEnvStr(pszCommand);
		if (szExpand.ms_Arg)
			pszCommand = szExpand.ms_Arg;
	}

	szCurDir.ms_Arg = GetPanelDir(gpdf_Active|gpdf_NoPlugin);
	if (!szCurDir.ms_Arg || !*szCurDir.ms_Arg)
	{
		szCurDir.Set(L"C:\\");
	}

	bool bSilent = (wcsstr(pszCommand, L"-new_console") != NULL);

	if (!bSilent)
		ShowUserScreen(true);

	RunExternalProgramW(pszCommand, szCurDir.ms_Arg, bSilent);

	if (!bSilent)
		ShowUserScreen(false);
	RedrawAll();

	return TRUE;
}

bool CPluginBase::ProcessCommandLine(wchar_t* pszCommand)
{
	if (!pszCommand)
		return false;

	if (lstrcmpni(pszCommand, L"run:", 4) == 0)
	{
		RunExternalProgram(pszCommand+4); //-V112
		return true;
	}

	return false;
}

void CPluginBase::ShowPluginMenu(PluginCallCommands nCallID /*= pcc_None*/)
{
	int nItem = -1;

	if (!FarHwnd)
	{
		ShowMessage(CEInvalidConHwnd,0); // "ConEmu plugin\nGetConsoleWindow()==FarHwnd is NULL"
		return;
	}

	if (IsTerminalMode())
	{
		ShowMessage(CEUnavailableInTerminal,0); // "ConEmu plugin\nConEmu is not available in terminal mode\nCheck TERM environment variable"
		return;
	}

	CheckConEmuDetached();

	if (nCallID != pcc_None)
	{
		// Команды CallPlugin
		for (size_t i = 0; i < countof(gpPluginMenu); i++)
		{
			if (gpPluginMenu[i].CallID == nCallID)
			{
				nItem = gpPluginMenu[i].MenuID;
				break;
			}
		}
		_ASSERTE(nItem!=-1);

		SHOWDBGINFO(L"*** ShowPluginMenu used default item\n");
	}
	else
	{
		ConEmuPluginMenuItem items[menu_Last] = {};
		int nCount = menu_Last; //sizeof(items)/sizeof(items[0]);
		_ASSERTE(nCount == countof(gpPluginMenu));
		for (int i = 0; i < nCount; i++)
		{
			if (!gpPluginMenu[i].LangID)
			{
				items[i].Separator = true;
				continue;
			}
			_ASSERTE(i == gpPluginMenu[i].MenuID);
			items[i].Selected = pcc_Selected((PluginMenuCommands)i);
			items[i].Disabled = pcc_Disabled((PluginMenuCommands)i);
			items[i].MsgID = gpPluginMenu[i].LangID;
		}

		SHOWDBGINFO(L"*** calling ShowPluginMenu\n");
		nItem = ShowPluginMenu(items, nCount);
	}

	if (nItem < 0)
	{
		SHOWDBGINFO(L"*** ShowPluginMenu cancelled, nItem < 0\n");
		return;
	}

	#ifdef _DEBUG
	wchar_t szInfo[128]; _wsprintf(szInfo, SKIPLEN(countof(szInfo)) L"*** ShowPluginMenu done, nItem == %i\n", nItem);
	SHOWDBGINFO(szInfo);
	#endif

	switch (nItem)
	{
		case menu_EditConsoleOutput:
		case menu_ViewConsoleOutput:
		{
			// Открыть в редакторе вывод последней консольной программы
			CESERVER_REQ* pIn = (CESERVER_REQ*)calloc(sizeof(CESERVER_REQ_HDR)+sizeof(DWORD),1);

			if (!pIn) return;

			CESERVER_REQ* pOut = NULL;
			ExecutePrepareCmd(&pIn->hdr, CECMD_GETOUTPUTFILE, sizeof(CESERVER_REQ_HDR)+sizeof(DWORD));
			pIn->OutputFile.bUnicode = (gFarVersion.dwVerMajor>=2);
			pOut = ExecuteGuiCmd(FarHwnd, pIn, FarHwnd);

			if (pOut)
			{
				if (pOut->OutputFile.szFilePathName[0])
				{
					bool lbRc = OpenEditor(pOut->OutputFile.szFilePathName, (nItem==1)/*abView*/, true);

					if (!lbRc)
					{
						DeleteFile(pOut->OutputFile.szFilePathName);
					}
				}

				ExecuteFreeResult(pOut);
			}

			free(pIn);
		} break;

		case menu_SwitchTabVisible: // Показать/спрятать табы
		case menu_SwitchTabNext:
		case menu_SwitchTabPrev:
		case menu_SwitchTabCommit:
		{
			CESERVER_REQ* pIn = ExecuteNewCmd(CECMD_TABSCMD, sizeof(CESERVER_REQ_HDR)+sizeof(pIn->Data));
			// Data[0] <== enum ConEmuTabCommand
			switch (nItem)
			{
			case menu_SwitchTabVisible: // Показать/спрятать табы
				pIn->Data[0] = ctc_ShowHide; break;
			case menu_SwitchTabNext:
				pIn->Data[0] = ctc_SwitchNext; break;
			case menu_SwitchTabPrev:
				pIn->Data[0] = ctc_SwitchPrev; break;
			case menu_SwitchTabCommit:
				pIn->Data[0] = ctc_SwitchCommit; break;
			default:
				_ASSERTE(nItem==menu_SwitchTabVisible); // неизвестная команда!
				pIn->Data[0] = ctc_ShowHide;
			}

			CESERVER_REQ* pOut = ExecuteGuiCmd(FarHwnd, pIn, FarHwnd);
			if (pOut) ExecuteFreeResult(pOut);
		} break;

		case menu_ShowTabsList:
		{
			CESERVER_REQ* pIn = ExecuteNewCmd(CECMD_GETALLTABS, sizeof(CESERVER_REQ_HDR));
			CESERVER_REQ* pOut = ExecuteGuiCmd(FarHwnd, pIn, FarHwnd);
			if (pOut && (pOut->GetAllTabs.Count > 0))
			{
				INT_PTR nMenuRc = -1;

				int Count = pOut->GetAllTabs.Count;
				int AllCount = Count + pOut->GetAllTabs.Tabs[Count-1].ConsoleIdx;
				ConEmuPluginMenuItem* pItems = (ConEmuPluginMenuItem*)calloc(AllCount,sizeof(*pItems));
				if (pItems)
				{
					int nLastConsole = 0;
					for (int i = 0, k = 0; i < Count; i++, k++)
					{
						if (nLastConsole != pOut->GetAllTabs.Tabs[i].ConsoleIdx)
						{
							pItems[k++].Separator = true;
							nLastConsole = pOut->GetAllTabs.Tabs[i].ConsoleIdx;
						}
						_ASSERTE(k < AllCount);
						pItems[k].Selected = (pOut->GetAllTabs.Tabs[i].ActiveConsole && pOut->GetAllTabs.Tabs[i].ActiveTab);
						pItems[k].Checked = pOut->GetAllTabs.Tabs[i].ActiveTab;
						pItems[k].Disabled = pOut->GetAllTabs.Tabs[i].Disabled;
						pItems[k].MsgText = pOut->GetAllTabs.Tabs[i].Title;
						pItems[k].UserData = i;
					}

					nMenuRc = ShowPluginMenu(pItems, AllCount);

					if ((nMenuRc >= 0) && (nMenuRc < AllCount))
					{
						nMenuRc = pItems[nMenuRc].UserData;

						if (pOut->GetAllTabs.Tabs[nMenuRc].ActiveConsole && !pOut->GetAllTabs.Tabs[nMenuRc].ActiveTab)
						{
							DWORD nTab = pOut->GetAllTabs.Tabs[nMenuRc].TabIdx;
							int nOpenFrom = -1;
							int nArea = Plugin()->GetMacroArea();
							if (nArea != -1)
							{
								if (nArea == ma_Shell || nArea == ma_Search || nArea == ma_InfoPanel || nArea == ma_QViewPanel || nArea == ma_TreePanel)
									gnPluginOpenFrom = of_FilePanel;
								else if (nArea == ma_Editor)
									gnPluginOpenFrom = of_Editor;
								else if (nArea == ma_Viewer)
									gnPluginOpenFrom = of_Viewer;
							}
							gnPluginOpenFrom = nOpenFrom;
							ProcessCommand(CMD_SETWINDOW, FALSE, &nTab, NULL, true/*bForceSendTabs*/);
						}
						else if (!pOut->GetAllTabs.Tabs[nMenuRc].ActiveConsole || !pOut->GetAllTabs.Tabs[nMenuRc].ActiveTab)
						{
							CESERVER_REQ* pActIn = ExecuteNewCmd(CECMD_ACTIVATETAB, sizeof(CESERVER_REQ_HDR)+2*sizeof(DWORD));
							pActIn->dwData[0] = pOut->GetAllTabs.Tabs[nMenuRc].ConsoleIdx;
							pActIn->dwData[1] = pOut->GetAllTabs.Tabs[nMenuRc].TabIdx;
							CESERVER_REQ* pActOut = ExecuteGuiCmd(FarHwnd, pActIn, FarHwnd);
							ExecuteFreeResult(pActOut);
							ExecuteFreeResult(pActIn);
						}
					}

					SafeFree(pItems);
				}
				ExecuteFreeResult(pOut);
			}
			else
			{
				ShowMessage(CEGetAllTabsFailed, 0);
			}
			ExecuteFreeResult(pIn);
		} break;

		case menu_ConEmuMacro: // Execute GUI macro (gialog)
		{
			if (gFarVersion.dwVerMajor==1)
				GuiMacroDlgA();
			else if (gFarVersion.dwBuild>=FAR_Y2_VER)
				FUNC_Y2(GuiMacroDlgW)();
			else if (gFarVersion.dwBuild>=FAR_Y1_VER)
				FUNC_Y1(GuiMacroDlgW)();
			else
				FUNC_X(GuiMacroDlgW)();
		} break;

		case menu_AttachToConEmu: // Attach to GUI (если FAR был CtrlAltTab)
		{
			if (TerminalMode) break;  // низзя

			if (ghConEmuWndDC && IsWindow(ghConEmuWndDC)) break;  // Мы и так подключены?

			Attach2Gui();
		} break;

		//#ifdef _DEBUG
		//case 11: // Start "ConEmuC.exe /DEBUGPID="
		//#else
		case menu_StartDebug: // Start "ConEmuC.exe /DEBUGPID="
			//#endif
		{
			if (TerminalMode) break;  // низзя

			StartDebugger();
		} break;

		case menu_ConsoleInfo:
		{
			ShowConsoleInfo();
		} break;
	}
}

int CPluginBase::ProcessSynchroEvent(int Event, void *Param)
{
	if (Event != se_CommonSynchro)
		return;

	if (gbInputSynchroPending)
		gbInputSynchroPending = false;

	// Некоторые плагины (NetBox) блокируют главный поток, и открывают
	// в своем потоке диалог. Это ThreadSafe. Некорректные открытия
	// отследить не удастся. Поэтому, считаем, если Far дернул наш
	// ProcessSynchroEventW, то это (временно) стала главная нить
	DWORD nPrevID = gnMainThreadId;
	gnMainThreadId = GetCurrentThreadId();

	#ifdef _DEBUG
	{
		static int nLastType = -1;
		int nCurType = GetActiveWindowType();

		if (nCurType != nLastType)
		{
			LPCWSTR pszCurType = GetWindowTypeName(nCurType);

			LPCWSTR pszLastType = GetWindowTypeName(nLastType);

			wchar_t szDbg[255];
			_wsprintf(szDbg, SKIPLEN(countof(szDbg)) L"FarWindow: %s activated (was %s)\n", pszCurType, pszLastType);
			DEBUGSTR(szDbg);
			nLastType = nCurType;
		}
	}
	#endif

	if (!gbSynchroProhibited)
	{
		OnMainThreadActivated();
	}

	if (gnSynchroCount > 0)
		gnSynchroCount--;

	if (gbSynchroProhibited && (gnSynchroCount == 0))
	{
		Plugin()->StopWaitEndSynchro();
	}

	gnMainThreadId = nPrevID;

	return 0;
}

int CPluginBase::ProcessEditorViewerEvent(int EditorEvent, int ViewerEvent)
{
	if (!gbRequestUpdateTabs)
	{
		if ((EditorEvent != -1)
			&& (EditorEvent == ee_Read || EditorEvent == ee_Close || EditorEvent == ee_GotFocus || EditorEvent == ee_KillFocus || EditorEvent == ee_Save))
		{
			gbRequestUpdateTabs = TRUE;
			//} else if (Event == EE_REDRAW && gbHandleOneRedraw) {
			//	gbHandleOneRedraw = false; gbRequestUpdateTabs = TRUE;
		}
		else if ((ViewerEvent != -1)
			&& (ViewerEvent == ve_Close || ViewerEvent == ve_GotFocus || ViewerEvent == ve_KillFocus || ViewerEvent == ve_Read))
		{
			gbRequestUpdateTabs = TRUE;
		}
	}

	if (isModalEditorViewer())
	{
		if ((EditorEvent == ee_Close) || (ViewerEvent == ve_Close))
		{
			gbClosingModalViewerEditor = TRUE;
		}
	}

	if (gpBgPlugin && (EditorEvent != ee_Redraw))
	{
		gpBgPlugin->OnMainThreadActivated(EditorEvent, ViewerEvent);
	}

	return 0;
}

bool CPluginBase::isModalEditorViewer()
{
	if (!gpTabs || !gpTabs->Tabs.nTabCount)
		return false;

	// Если последнее открытое окно - модальное
	if (gpTabs->Tabs.tabs[gpTabs->Tabs.nTabCount-1].Modal)
		return true;

	// Было раньше такое условие, по идее это не правильно
	// if (gpTabs->Tabs.tabs[0].Type != wt_Panels)
	// return true;

	return false;
}

// Плагин может быть вызван в первый раз из фоновой нити.
// Поэтому простой "gnMainThreadId = GetCurrentThreadId();" не прокатит. Нужно искать первую нить процесса!
DWORD CPluginBase::GetMainThreadId()
{
	DWORD nThreadID = 0;
	DWORD nProcID = GetCurrentProcessId();
	HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);

	if (h != INVALID_HANDLE_VALUE)
	{
		THREADENTRY32 ti = {sizeof(THREADENTRY32)};

		if (Thread32First(h, &ti))
		{
			do
			{
				// Нужно найти ПЕРВУЮ нить процесса
				if (ti.th32OwnerProcessID == nProcID)
				{
					nThreadID = ti.th32ThreadID;
					break;
				}
			}
			while(Thread32Next(h, &ti));
		}

		CloseHandle(h);
	}

	// Нехорошо. Должна быть найдена. Вернем хоть что-то (текущую нить)
	if (!nThreadID)
	{
		_ASSERTE(nThreadID!=0);
		nThreadID = GetCurrentThreadId();
	}

	return nThreadID;
}

void CPluginBase::ShowConsoleInfo()
{
	DWORD nConIn = 0, nConOut = 0;
	HANDLE hConIn = GetStdHandle(STD_INPUT_HANDLE);
	HANDLE hConOut = GetStdHandle(STD_OUTPUT_HANDLE);
	GetConsoleMode(hConIn, &nConIn);
	GetConsoleMode(hConOut, &nConOut);

	CONSOLE_SCREEN_BUFFER_INFO csbi = {};
	GetConsoleScreenBufferInfo(hConOut, &csbi);
	CONSOLE_CURSOR_INFO ci = {};
	GetConsoleCursorInfo(hConOut, &ci);

	wchar_t szInfo[1024];
	_wsprintf(szInfo, SKIPLEN(countof(szInfo))
		L"ConEmu Console information\n"
		L"TerminalMode=%s\n"
		L"Console HWND=0x%08X; "
		L"Virtual HWND=0x%08X\n"
		L"ServerPID=%u; CurrentPID=%u\n"
		L"ConInMode=0x%08X; ConOutMode=0x%08X\n"
		L"Buffer size=(%u,%u); Rect=(%u,%u)-(%u,%u)\n"
		L"CursorInfo=(%u,%u,%u%s); MaxWndSize=(%u,%u)\n"
		L"OutputAttr=0x%02X\n"
		,
		TerminalMode ? L"Yes" : L"No",
		(DWORD)FarHwnd, (DWORD)ghConEmuWndDC,
		gdwServerPID, GetCurrentProcessId(),
		nConIn, nConOut,
		csbi.dwSize.X, csbi.dwSize.Y,
		csbi.srWindow.Left, csbi.srWindow.Top, csbi.srWindow.Right, csbi.srWindow.Bottom,
		csbi.dwCursorPosition.X, csbi.dwCursorPosition.Y, ci.dwSize, ci.bVisible ? L"V" : L"H",
		csbi.dwMaximumWindowSize.X, csbi.dwMaximumWindowSize.Y,
		csbi.wAttributes,
		0
	);

	ShowMessage(szInfo, 0, false);
}

bool CPluginBase::RunExternalProgramW(wchar_t* pszCommand, wchar_t* pszCurDir, bool bSilent/*=false*/)
{
	bool lbRc = false;
	_ASSERTE(pszCommand && *pszCommand);

	if (bSilent)
	{
		DWORD nCmdLen = lstrlen(pszCommand);
		CESERVER_REQ* pIn = ExecuteNewCmd(CECMD_NEWCMD, sizeof(CESERVER_REQ_HDR)+sizeof(CESERVER_REQ_NEWCMD)+(nCmdLen*sizeof(wchar_t)));
		if (pIn)
		{
			pIn->NewCmd.hFromConWnd = FarHwnd;
			if (pszCurDir)
				lstrcpyn(pIn->NewCmd.szCurDir, pszCurDir, countof(pIn->NewCmd.szCurDir));

			lstrcpyn(pIn->NewCmd.szCommand, pszCommand, nCmdLen+1);

			HWND hGuiRoot = GetConEmuHWND(1);
			CESERVER_REQ* pOut = ExecuteGuiCmd(hGuiRoot, pIn, FarHwnd);
			if (pOut)
			{
				if (pOut->hdr.cbSize > sizeof(pOut->hdr) && pOut->Data[0])
				{
					lbRc = true;
				}
				ExecuteFreeResult(pOut);
			}
			else
			{
				_ASSERTE(pOut!=NULL);
			}
			ExecuteFreeResult(pIn);
		}
	}
	else
	{
		STARTUPINFO cif= {sizeof(STARTUPINFO)};
		PROCESS_INFORMATION pri= {0};
		HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
		DWORD oldConsoleMode;
		DWORD nErr = 0;
		DWORD nExitCode = 0;
		GetConsoleMode(hStdin, &oldConsoleMode);
		SetConsoleMode(hStdin, ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT); // подбиралось методом тыка

		#ifdef _DEBUG
		if (!bSilent)
		{
			WARNING("Посмотреть, как Update в консоль выводит.");
			wprintf(L"\nCmd: <%s>\nDir: <%s>\n\n", pszCommand, pszCurDir);
		}
		#endif

		MWow64Disable wow; wow.Disable();
		SetLastError(0);
		BOOL lb = CreateProcess(/*strCmd, strArgs,*/ NULL, pszCommand, NULL, NULL, TRUE,
		          NORMAL_PRIORITY_CLASS|CREATE_DEFAULT_ERROR_MODE, NULL, pszCurDir, &cif, &pri);
		nErr = GetLastError();
		wow.Restore();

		if (lb)
		{
			WaitForSingleObject(pri.hProcess, INFINITE);
			GetExitCodeProcess(pri.hProcess, &nExitCode);
			CloseHandle(pri.hProcess);
			CloseHandle(pri.hThread);

			#ifdef _DEBUG
			if (!bSilent)
				wprintf(L"\nConEmuC: Process was terminated, ExitCode=%i\n\n", nExitCode);
			#endif

			lbRc = true;
		}
		else
		{
			#ifdef _DEBUG
			if (!bSilent)
				wprintf(L"\nConEmuC: CreateProcess failed, ErrCode=0x%08X\n\n", nErr);
			#endif
		}

		//wprintf(L"Cmd: <%s>\nArg: <%s>\nDir: <%s>\n\n", strCmd, strArgs, pszCurDir);
		SetConsoleMode(hStdin, oldConsoleMode);
	}

	return lbRc;
}

bool CPluginBase::StartDebugger()
{
	if (IsDebuggerPresent())
	{
		ShowMessage(CEAlreadyDebuggerPresent,0); // "ConEmu plugin\nDebugger is already attached to current process"
		return false; // Уже
	}

	if (IsTerminalMode())
	{
		ShowMessage(CECantDebugInTerminal,0); // "ConEmu plugin\nDebugger is not available in terminal mode"
		return false; // Уже
	}

	//DWORD dwServerPID = 0;
	// Create process, with flag /Attach GetCurrentProcessId()
	// Sleep for sometimes, try InitHWND(hConWnd); several times
	wchar_t  szExe[MAX_PATH*3] = {0};
	wchar_t  szConEmuC[MAX_PATH];
	bool lbRc = false;
	DWORD nLen = 0;
	PROCESS_INFORMATION pi; memset(&pi, 0, sizeof(pi));
	STARTUPINFO si; memset(&si, 0, sizeof(si));
	si.cb = sizeof(si);
	DWORD dwSelfPID = GetCurrentProcessId();

	if ((nLen = GetEnvironmentVariableW(ENV_CONEMUBASEDIR_VAR_W, szConEmuC, MAX_PATH-16)) < 1)
	{
		ShowMessage(CECantDebugNotEnvVar,0); // "ConEmu plugin\nEnvironment variable 'ConEmuBaseDir' not defined\nDebugger is not available"
		return false; // Облом
	}

	lstrcatW(szConEmuC, L"\\ConEmuC.exe");

	if (!FileExists(szConEmuC))
	{
		wchar_t* pszSlash = NULL;

		if (((nLen=GetModuleFileName(0, szConEmuC, MAX_PATH-24)) < 1) || ((pszSlash = wcsrchr(szConEmuC, L'\\')) == NULL))
		{
			ShowMessage(CECantDebugNotEnvVar,0); // "ConEmu plugin\nEnvironment variable 'ConEmuBaseDir' not defined\nDebugger is not available"
			return false; // Облом
		}

		lstrcpyW(pszSlash, L"\\ConEmu\\ConEmuC.exe");

		if (!FileExists(szConEmuC))
		{
			lstrcpyW(pszSlash, L"\\ConEmuC.exe");

			if (!FileExists(szConEmuC))
			{
				ShowMessage(CECantDebugNotEnvVar,0); // "ConEmu plugin\nEnvironment variable 'ConEmuBaseDir' not defined\nDebugger is not available"
				return false; // Облом
			}
		}
	}

	int w = 80, h = 25;
	CONSOLE_SCREEN_BUFFER_INFO csbi;

	if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi))
	{
		w = csbi.srWindow.Right - csbi.srWindow.Left + 1;
		h = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
	}

	if (ghConEmuWndDC)
	{
		DWORD nGuiPID = 0; GetWindowThreadProcessId(ghConEmuWndDC, &nGuiPID);
		// Откроем дебаггер в новой вкладке ConEmu. При желании юзеру проще сделать Detach
		// "/DEBUGPID=" обязательно должен быть первым аргументом

		_wsprintf(szExe, SKIPLEN(countof(szExe)) L"\"%s\" /ATTACH /ROOT \"%s\" /DEBUGPID=%i /BW=%i /BH=%i /BZ=9999",
		          szConEmuC, szConEmuC, dwSelfPID, w, h);
		//_wsprintf(szExe, SKIPLEN(countof(szExe)) L"\"%s\" /ATTACH /GID=%u /GHWND=%08X /ROOT \"%s\" /DEBUGPID=%i /BW=%i /BH=%i /BZ=9999",
		//          szConEmuC, nGuiPID, (DWORD)(DWORD_PTR)ghConEmuWndDC, szConEmuC, dwSelfPID, w, h);
	}
	else
	{
		// Запустить дебаггер в новом видимом консольном окне
		_wsprintf(szExe, SKIPLEN(countof(szExe)) L"\"%s\" /DEBUGPID=%i /BW=%i /BH=%i /BZ=9999",
		          szConEmuC, dwSelfPID, w, h);
	}

	if (ghConEmuWndDC)
	{
		si.dwFlags |= STARTF_USESHOWWINDOW;
		si.wShowWindow = SW_HIDE;
	}

	if (!CreateProcess(NULL, szExe, NULL, NULL, FALSE, NORMAL_PRIORITY_CLASS|CREATE_NEW_CONSOLE, NULL,
	                  NULL, &si, &pi))
	{
		// Хорошо бы ошибку показать?
		#ifdef _DEBUG
		DWORD dwErr = GetLastError();
		#endif
		ShowMessage(CECantStartDebugger,0); // "ConEmu plugin\nНе удалось запустить процесс отладчика"
	}
	else
	{
		lbRc = true;
	}

	return lbRc;
}

bool CPluginBase::Attach2Gui()
{
	bool lbRc = false;
	DWORD dwServerPID = 0;
	BOOL lbFound = FALSE;
	WCHAR  szCmdLine[MAX_PATH+0x100] = {0};
	wchar_t szConEmuBase[MAX_PATH+1], szConEmuGui[MAX_PATH+1];
	//DWORD nLen = 0;
	PROCESS_INFORMATION pi; memset(&pi, 0, sizeof(pi));
	STARTUPINFO si = {sizeof(si)};
	DWORD dwSelfPID = GetCurrentProcessId();
	wchar_t* pszSlash = NULL;
	DWORD nStartWait = 255;

	if (!FindConEmuBaseDir(szConEmuBase, szConEmuGui, ghPluginModule))
	{
		ShowMessageGui(CECantStartServer2, MB_ICONSTOP|MB_SYSTEMMODAL);
		goto wrap;
	}

	// Нужно загрузить ConEmuHk.dll и выполнить инициализацию хуков. Учесть, что ConEmuHk.dll уже мог быть загружен
	if (!ghHooksModule)
	{
		wchar_t szHookLib[MAX_PATH+16];
		wcscpy_c(szHookLib, szConEmuBase);
		#ifdef _WIN64
			wcscat_c(szHookLib, L"\\ConEmuHk64.dll");
		#else
			wcscat_c(szHookLib, L"\\ConEmuHk.dll");
		#endif
		ghHooksModule = LoadLibrary(szHookLib);
		if (ghHooksModule)
		{
			gbHooksModuleLoaded = TRUE;
			// После подцепляния к GUI нужно выполнить StartupHooks!
			gbStartupHooksAfterMap = TRUE;
		}
	}


	if (FindServerCmd(CECMD_ATTACH2GUI, dwServerPID, true) && dwServerPID != 0)
	{
		// "Server was already started. PID=%i. Exiting...\n", dwServerPID
		gdwServerPID = dwServerPID;
		_ASSERTE(gdwServerPID!=0);
		gbTryOpenMapHeader = (gpConMapInfo==NULL);

		if (gpConMapInfo)  // 04.03.2010 Maks - Если мэппинг уже открыт - принудительно передернуть ресурсы и информацию
			CheckResources(TRUE);

		lbRc = true;
		goto wrap;
	}

	gdwServerPID = 0;
	//TODO("У сервера пока не получается менять шрифт в консоли, которую создал FAR");
	//SetConsoleFontSizeTo(GetConEmuHWND(2), 6, 4, L"Lucida Console");
	// Create process, with flag /Attach GetCurrentProcessId()
	// Sleep for sometimes, try InitHWND(hConWnd); several times

	szCmdLine[0] = L'"';
	wcscat_c(szCmdLine, szConEmuBase);
	wcscat_c(szCmdLine, L"\\");
	//if ((nLen = GetEnvironmentVariableW(ENV_CONEMUBASEDIR_VAR_W, szCmdLine+1, MAX_PATH)) > 0)
	//{
	//	if (szCmdLine[nLen] != L'\\') { szCmdLine[nLen+1] = L'\\'; szCmdLine[nLen+2] = 0; }
	//}
	//else
	//{
	//	if (!GetModuleFileName(0, szCmdLine+1, MAX_PATH) || !(pszSlash = wcsrchr(szCmdLine, L'\\')))
	//	{
	//		ShowMessageGui(CECantStartServer2, MB_ICONSTOP|MB_SYSTEMMODAL);
	//		goto wrap;
	//	}
	//	pszSlash[1] = 0;
	//}

	pszSlash = szCmdLine + lstrlenW(szCmdLine);
	//BOOL lbFound = FALSE;
	// Для фанатов 64-битных версий
#ifdef WIN64

	//if (!lbFound) -- точная папка уже найдена
	//{
	//	lstrcpyW(pszSlash, L"ConEmu\\ConEmuC64.exe");
	//	lbFound = FileExists(szCmdLine+1);
	//}

	if (!lbFound)
	{
		lstrcpyW(pszSlash, L"ConEmuC64.exe");
		lbFound = FileExists(szCmdLine+1);
	}

#endif

	//if (!lbFound) -- точная папка уже найдена
	//{
	//	lstrcpyW(pszSlash, L"ConEmu\\ConEmuC.exe");
	//	lbFound = FileExists(szCmdLine+1);
	//}

	if (!lbFound)
	{
		lstrcpyW(pszSlash, L"ConEmuC.exe");
		lbFound = FileExists(szCmdLine+1);
	}

	if (!lbFound)
	{
		ShowMessageGui(CECantStartServer3, MB_ICONSTOP|MB_SYSTEMMODAL);
		goto wrap;
	}

	//if (IsWindows64())
	//	wsprintf(szCmdLine+lstrlenW(szCmdLine), L"ConEmuC64.exe\" /ATTACH /PID=%i", dwSelfPID);
	//else
	wsprintf(szCmdLine+lstrlenW(szCmdLine), L"\" /ATTACH /FARPID=%i", dwSelfPID);
	if (gdwPreDetachGuiPID)
		wsprintf(szCmdLine+lstrlenW(szCmdLine), L" /GID=%i", gdwPreDetachGuiPID);

	if (!CreateProcess(NULL, szCmdLine, NULL, NULL, FALSE, NORMAL_PRIORITY_CLASS, NULL,
	                  NULL, &si, &pi))
	{
		// Хорошо бы ошибку показать?
		ShowMessageGui(CECantStartServer, MB_ICONSTOP|MB_SYSTEMMODAL); // "ConEmu plugin\nCan't start console server process (ConEmuC.exe)\nOK"
	}
	else
	{
		wchar_t szName[64];
		_wsprintf(szName, SKIPLEN(countof(szName)) CESRVSTARTEDEVENT, pi.dwProcessId/*gnSelfPID*/);
		// Event мог быть создан и ранее (в Far-плагине, например)
		HANDLE hServerStartedEvent = CreateEvent(LocalSecurity(), TRUE, FALSE, szName);
		_ASSERTE(hServerStartedEvent!=NULL);
		HANDLE hWait[] = {pi.hProcess, hServerStartedEvent};
		nStartWait = WaitForMultipleObjects(countof(hWait), hWait, FALSE, ATTACH_START_SERVER_TIMEOUT);

		if (nStartWait == 0)
		{
			// Server was terminated!
			ShowMessageGui(CECantStartServer, MB_ICONSTOP|MB_SYSTEMMODAL); // "ConEmu plugin\nCan't start console server process (ConEmuC.exe)\nOK"
		}
		else
		{
			// Server must be initialized ATM
			_ASSERTE(nStartWait == 1);

			// Recall initialization of ConEmuHk.dll
			if (ghHooksModule)
			{
				RequestLocalServer_t fRequestLocalServer = (RequestLocalServer_t)GetProcAddress(ghHooksModule, "RequestLocalServer");
				// Refresh ConEmu HWND's
				if (fRequestLocalServer)
				{
					RequestLocalServerParm Parm = {sizeof(Parm), slsf_ReinitWindows};
					//if (gFarVersion.dwVerMajor >= 3)
					//	Parm.Flags |=
					fRequestLocalServer(&Parm);
				}
			}

			gdwServerPID = pi.dwProcessId;
			_ASSERTE(gdwServerPID!=0);
			SafeCloseHandle(pi.hProcess);
			SafeCloseHandle(pi.hThread);
			lbRc = true;
			// Чтобы MonitorThread пытался открыть Mapping
			gbTryOpenMapHeader = (gpConMapInfo==NULL);
		}
	}

wrap:
	return lbRc;
}

bool CPluginBase::FindServerCmd(DWORD nServerCmd, DWORD &dwServerPID, bool bFromAttach /*= false*/)
{
	if (!FarHwnd)
	{
		_ASSERTE(FarHwnd!=NULL);
		return false;
	}

	bool lbRc = false;

	//111209 - пробуем через мэппинг, там ИД сервера уже должен быть
	CESERVER_CONSOLE_MAPPING_HDR SrvMapping = {};
	if (LoadSrvMapping(FarHwnd, SrvMapping))
	{
		CESERVER_REQ* pIn = ExecuteNewCmd(nServerCmd, sizeof(CESERVER_REQ_HDR)+sizeof(DWORD));
		pIn->dwData[0] = GetCurrentProcessId();
		CESERVER_REQ* pOut = ExecuteSrvCmd(SrvMapping.nServerPID, pIn, FarHwnd);

		if (pOut)
		{
			_ASSERTE(SrvMapping.nServerPID == pOut->dwData[0]);
			dwServerPID = SrvMapping.nServerPID;
			ExecuteFreeResult(pOut);
			lbRc = true;
		}
		else
		{
			_ASSERTE(pOut!=NULL);
		}

		ExecuteFreeResult(pIn);

		// Если команда успешно выполнена - выходим
		if (lbRc)
			return true;
	}
	else
	{
		_ASSERTE(bFromAttach && "LoadSrvMapping(FarHwnd, SrvMapping) failed");
		return false;
	}
	return false;

#if 0
	BOOL lbRc = FALSE;
	DWORD nProcessCount = 0, nProcesses[100] = {0};
	dwServerPID = 0;
	typedef DWORD (WINAPI* FGetConsoleProcessList)(LPDWORD lpdwProcessList, DWORD dwProcessCount);
	FGetConsoleProcessList pfnGetConsoleProcessList = NULL;
	HMODULE hKernel = GetModuleHandleW(L"kernel32.dll");

	if (hKernel)
	{
		pfnGetConsoleProcessList = (FGetConsoleProcessList)GetProcAddress(hKernel, "GetConsoleProcessList");
	}

	BOOL lbWin2kMode = (pfnGetConsoleProcessList == NULL);

	if (!lbWin2kMode)
	{
		if (pfnGetConsoleProcessList)
		{
			nProcessCount = pfnGetConsoleProcessList(nProcesses, countof(nProcesses));

			if (nProcessCount && nProcessCount > countof(nProcesses))
			{
				_ASSERTE(nProcessCount <= countof(nProcesses));
				nProcessCount = 0;
			}
		}
	}

	if (lbWin2kMode)
	{
		DWORD nSelfPID = GetCurrentProcessId();
		HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);

		if (lbWin2kMode)
		{
			if (hSnap != INVALID_HANDLE_VALUE)
			{
				PROCESSENTRY32 prc = {sizeof(PROCESSENTRY32)};

				if (Process32First(hSnap, &prc))
				{
					do
					{
						if (prc.th32ProcessID == nSelfPID)
						{
							nProcesses[0] = prc.th32ParentProcessID;
							nProcesses[1] = nSelfPID;
							nProcessCount = 2;
							break;
						}
					}
					while(!dwServerPID && Process32Next(hSnap, &prc));
				}

				CloseHandle(hSnap);
			}
		}
	}

	if (nProcessCount >= 2)
	{
		//DWORD nParentPID = 0;
		DWORD nSelfPID = GetCurrentProcessId();
		HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);

		if (hSnap != INVALID_HANDLE_VALUE)
		{
			PROCESSENTRY32 prc = {sizeof(PROCESSENTRY32)};

			if (Process32First(hSnap, &prc))
			{
				do
				{
					for(UINT i = 0; i < nProcessCount; i++)
					{
						if (prc.th32ProcessID != nSelfPID
						        && prc.th32ProcessID == nProcesses[i])
						{
							if (lstrcmpiW(prc.szExeFile, L"conemuc.exe")==0
							        /*|| lstrcmpiW(prc.szExeFile, L"conemuc64.exe")==0*/)
							{
								CESERVER_REQ* pIn = ExecuteNewCmd(nServerCmd, sizeof(CESERVER_REQ_HDR)+sizeof(DWORD));
								pIn->dwData[0] = GetCurrentProcessId();
								CESERVER_REQ* pOut = ExecuteSrvCmd(prc.th32ProcessID, pIn, FarHwnd);

								if (pOut) dwServerPID = prc.th32ProcessID;

								ExecuteFreeResult(pIn); ExecuteFreeResult(pOut);

								// Если команда успешно выполнена - выходим
								if (dwServerPID)
								{
									lbRc = TRUE;
									break;
								}
							}
						}
					}
				}
				while(!dwServerPID && Process32Next(hSnap, &prc));
			}

			CloseHandle(hSnap);
		}
	}

	return lbRc;
#endif
}

int CPluginBase::ShowMessage(int aiMsg, int aiButtons)
{
	wchar_t szMsgText[512] = L"";
	GetMsg(aiMsg, szMsgText, countof(szMsgText));

	return ShowMessage(szMsg, aiButtons, true);
}

// Если не вызывать - буфер увеличивается автоматически. Размер в БАЙТАХ
// Возвращает FALSE при ошибках выделения памяти
bool CPluginBase::OutDataAlloc(DWORD anSize)
{
	_ASSERTE(gpCmdRet==NULL);
	// + размер заголовка gpCmdRet
	gpCmdRet = (CESERVER_REQ*)Alloc(sizeof(CESERVER_REQ_HDR)+anSize,1);

	if (!gpCmdRet)
		return false;

	// Код команды пока не известен - установит вызывающая функция
	ExecutePrepareCmd(&gpCmdRet->hdr, 0, anSize+sizeof(CESERVER_REQ_HDR));
	gpData = gpCmdRet->Data;
	gnDataSize = anSize;
	gpCursor = gpData;
	return true;
}

// Размер в БАЙТАХ. вызывается автоматически из OutDataWrite
// Возвращает FALSE при ошибках выделения памяти
bool CPluginBase::OutDataRealloc(DWORD anNewSize)
{
	if (!gpCmdRet)
		return OutDataAlloc(anNewSize);

	if (anNewSize < gnDataSize)
		return false; // нельзя выделять меньше памяти, чем уже есть

	// realloc иногда не работает, так что даже и не пытаемся
	CESERVER_REQ* lpNewCmdRet = (CESERVER_REQ*)Alloc(sizeof(CESERVER_REQ_HDR)+anNewSize,1);

	if (!lpNewCmdRet)
		return false;

	ExecutePrepareCmd(&lpNewCmdRet->hdr, gpCmdRet->hdr.nCmd, anNewSize+sizeof(CESERVER_REQ_HDR));
	LPBYTE lpNewData = lpNewCmdRet->Data;

	if (!lpNewData)
		return false;

	// скопировать существующие данные
	memcpy(lpNewData, gpData, gnDataSize);
	// запомнить новую позицию курсора
	gpCursor = lpNewData + (gpCursor - gpData);
	// И новый буфер с размером
	Free(gpCmdRet);
	gpCmdRet = lpNewCmdRet;
	gpData = lpNewData;
	gnDataSize = anNewSize;
	return true;
}

// Размер в БАЙТАХ
// Возвращает FALSE при ошибках выделения памяти
bool CPluginBase::OutDataWrite(LPVOID apData, DWORD anSize)
{
	if (!gpData)
	{
		if (!OutDataAlloc(max(1024, (anSize+128))))
			return false;
	}
	else if (((gpCursor-gpData)+anSize)>gnDataSize)
	{
		if (!OutDataRealloc(gnDataSize+max(1024, (anSize+128))))
			return false;
	}

	// Скопировать данные
	memcpy(gpCursor, apData, anSize);
	gpCursor += anSize;
	return true;
}

bool CPluginBase::CreateTabs(int windowCount)
{
	if (gpTabs && maxTabCount > (windowCount + 1))
	{
		// пересоздавать не нужно, секцию не трогаем. только запомним последнее кол-во окон
		lastWindowCount = windowCount;
		return true;
	}

	//Enter CriticalSection(csTabs);

	if ((gpTabs==NULL) || (maxTabCount <= (windowCount + 1)))
	{
		MSectionLock SC; SC.Lock(csTabs, TRUE);
		maxTabCount = windowCount + 20; // с запасом

		if (gpTabs)
		{
			Free(gpTabs); gpTabs = NULL;
		}

		gpTabs = (CESERVER_REQ*) Alloc(sizeof(CESERVER_REQ_HDR) + maxTabCount*sizeof(ConEmuTab), 1);
	}

	lastWindowCount = windowCount;

	return (gpTabs != NULL);
}

bool CPluginBase::AddTab(int &tabCount, int WindowPos, bool losingFocus, bool editorSave,
			int Type, LPCWSTR Name, LPCWSTR FileName,
			int Current, int Modified, int Modal,
			int EditViewId)
{
	bool lbCh = false;
	DEBUGSTR(L"--AddTab\n");

	if (Type == wt_Panels)
	{
		lbCh = (gpTabs->Tabs.tabs[0].Current != (Current/*losingFocus*/ ? 1 : 0)) ||
		       (gpTabs->Tabs.tabs[0].Type != wt_Panels);
		gpTabs->Tabs.tabs[0].Current = Current/*losingFocus*/ ? 1 : 0;
		//lstrcpyn(gpTabs->Tabs.tabs[0].Name, FUNC_Y(GetMsgW)(0), CONEMUTABMAX-1);
		gpTabs->Tabs.tabs[0].Name[0] = 0;
		gpTabs->Tabs.tabs[0].Pos = (WindowPos >= 0) ? WindowPos : 0;
		gpTabs->Tabs.tabs[0].Type = wt_Panels;
		gpTabs->Tabs.tabs[0].Modified = 0; // Иначе GUI может ошибочно считать, что есть несохраненные редакторы
		gpTabs->Tabs.tabs[0].EditViewId = 0;
		gpTabs->Tabs.tabs[0].Modal = 0;

		if (!tabCount)
			tabCount++;

		if (Current)
		{
			gpTabs->Tabs.CurrentType = gnCurrentWindowType = Type;
			gpTabs->Tabs.CurrentIndex = 0;
		}
	}
	else if (Type == wt_Editor || Type == wt_Viewer)
	{
		// Первое окно - должно быть панели. Если нет - значит фар открыт в режиме редактора
		if (tabCount == 1)
		{
			// 04.06.2009 Maks - Не, чего-то не то... при открытии редактора из панелей - он заменяет панели
			//gpTabs->Tabs.tabs[0].Type = Type;
		}

		// when receiving saving event receiver is still reported as modified
		if (editorSave && lstrcmpi(FileName, Name) == 0)
			Modified = 0;


		// Облагородить заголовок таба с Ctrl-O
		wchar_t szConOut[MAX_PATH];
		LPCWSTR pszName = PointToName(Name);
		if (pszName && (wmemcmp(pszName, L"CEM", 3) == 0))
		{
			LPCWSTR pszExt = PointToExt(pszName);
			if (lstrcmpi(pszExt, L".tmp") == 0)
			{
				if (gFarVersion.dwVerMajor==1)
				{
					GetMsgA(CEConsoleOutput, szConOut);
				}
				else
					lstrcpyn(szConOut, GetMsgW(CEConsoleOutput), countof(szConOut));

				Name = szConOut;
			}
		}


		lbCh = (gpTabs->Tabs.tabs[tabCount].Current != (Current/*losingFocus*/ ? 1 : 0)/*(losingFocus ? 0 : Current)*/)
		    || (gpTabs->Tabs.tabs[tabCount].Type != Type)
		    || (gpTabs->Tabs.tabs[tabCount].Modified != Modified)
			|| (gpTabs->Tabs.tabs[tabCount].Modal != Modal)
		    || (lstrcmp(gpTabs->Tabs.tabs[tabCount].Name, Name) != 0);
		// when receiving losing focus event receiver is still reported as current
		gpTabs->Tabs.tabs[tabCount].Type = Type;
		gpTabs->Tabs.tabs[tabCount].Current = (Current/*losingFocus*/ ? 1 : 0)/*losingFocus ? 0 : Current*/;
		gpTabs->Tabs.tabs[tabCount].Modified = Modified;
		gpTabs->Tabs.tabs[tabCount].Modal = Modal;
		gpTabs->Tabs.tabs[tabCount].EditViewId = EditViewId;

		if (gpTabs->Tabs.tabs[tabCount].Current != 0)
		{
			lastModifiedStateW = Modified != 0 ? 1 : 0;
			gpTabs->Tabs.CurrentType = gnCurrentWindowType = Type;
			gpTabs->Tabs.CurrentIndex = tabCount;
		}

		//else
		//{
		//	lastModifiedStateW = -1; //2009-08-17 при наличии более одного редактора - сносит крышу
		//}
		int nLen = min(lstrlen(Name),(CONEMUTABMAX-1));
		lstrcpyn(gpTabs->Tabs.tabs[tabCount].Name, Name, nLen+1);
		gpTabs->Tabs.tabs[tabCount].Name[nLen]=0;
		gpTabs->Tabs.tabs[tabCount].Pos = (WindowPos >= 0) ? WindowPos : tabCount;
		tabCount++;
	}

	return lbCh;
}

void CPluginBase::SendTabs(int tabCount, bool abForceSend/*=false*/)
{
	MSectionLock SC; SC.Lock(csTabs);

	if (!gpTabs)
	{
		_ASSERTE(gpTabs!=NULL);
		return;
	}

	gnCurTabCount = tabCount; // сразу запомним!, А то при ретриве табов количество еще старым будет...
	gpTabs->Tabs.nTabCount = tabCount;
	gpTabs->hdr.cbSize = sizeof(CESERVER_REQ_HDR) + sizeof(CESERVER_REQ_CONEMUTAB)
	                     + sizeof(ConEmuTab) * ((tabCount > 1) ? (tabCount - 1) : 0);
	// Обновляем структуру сразу, чтобы она была готова к отправке в любой момент
	ExecutePrepareCmd(&gpTabs->hdr, CECMD_TABSCHANGED, gpTabs->hdr.cbSize);

	// Это нужно делать только если инициировано ФАРОМ. Если запрос прислал ConEmu - не посылать...
	if (tabCount && ghConEmuWndDC && IsWindow(ghConEmuWndDC) && abForceSend)
	{
		gpTabs->Tabs.bMacroActive = Plugin()->IsMacroActive();
		gpTabs->Tabs.bMainThread = (GetCurrentThreadId() == gnMainThreadId);

		// Если выполняется макрос и отложенная отсылка (по окончанию) уже запрошена
		if (gpTabs->Tabs.bMacroActive && gbNeedPostTabSend)
		{
			gnNeedPostTabSendTick = GetTickCount(); // Обновить тик
			return;
		}

		gbNeedPostTabSend = FALSE;
		CESERVER_REQ* pOut =
		    ExecuteGuiCmd(FarHwnd, gpTabs, FarHwnd);

		if (pOut)
		{
			if (pOut->hdr.cbSize >= (sizeof(CESERVER_REQ_HDR) + sizeof(CESERVER_REQ_CONEMUTAB_RET)))
			{
				if (gpTabs->Tabs.bMacroActive && pOut->TabsRet.bNeedPostTabSend)
				{
					// Отослать после того, как макрос завершится
					gbNeedPostTabSend = TRUE;
					gnNeedPostTabSendTick = GetTickCount();
				}
				else if (pOut->TabsRet.bNeedResize)
				{
					// Если это отложенная отсылка табов после выполнения макросов
					if (GetCurrentThreadId() == gnMainThreadId)
					{
						FarSetConsoleSize(pOut->TabsRet.crNewSize.X, pOut->TabsRet.crNewSize.Y);
					}
				}
			}

			ExecuteFreeResult(pOut);
		}
	}

	SC.Unlock();
}

void CPluginBase::CloseTabs()
{
	if (ghConEmuWndDC && IsWindow(ghConEmuWndDC) && FarHwnd)
	{
		CESERVER_REQ in; // Пустая команда - значит FAR закрывается
		ExecutePrepareCmd(&in, CECMD_TABSCHANGED, sizeof(CESERVER_REQ_HDR));
		CESERVER_REQ* pOut = ExecuteGuiCmd(FarHwnd, &in, FarHwnd);

		if (pOut) ExecuteFreeResult(pOut);
	}
}

bool CPluginBase::UpdateConEmuTabs(bool abSendChanges)
{
	bool lbCh = false;
	// Блокируем сразу, т.к. ниже по коду gpTabs тоже используется
	MSectionLock SC; SC.Lock(csTabs);
	// На случай, если текущее окно заблокировано диалогом - не получится точно узнать
	// какое окно фара активно. Поэтому вернем последнее известное.
	int nLastCurrentTab = -1, nLastCurrentType = -1;

	if (gpTabs && gpTabs->Tabs.nTabCount > 0)
	{
		nLastCurrentTab = gpTabs->Tabs.CurrentIndex;
		nLastCurrentType = gpTabs->Tabs.CurrentType;
	}

	if (gpTabs)
	{
		gpTabs->Tabs.CurrentIndex = -1; // для строгости
	}

	if (!gbIgnoreUpdateTabs)
	{
		if (gbRequestUpdateTabs)
			gbRequestUpdateTabs = FALSE;

		if (ghConEmuWndDC && FarHwnd)
			CheckResources(FALSE);

		bool lbDummy = false;
		int windowCount = GetWindowCount();

		if ((windowCount == 0) && !gpFarInfo->bFarPanelAllowed)
		{
			windowCount = 1; lbDummy = true;
		}

		// lastWindowCount обновляется в CreateTabs
		lbCh = (lastWindowCount != windowCount);

		if (CreateTabs(windowCount))
		{
			if (lbDummy)
			{
				int tabCount = 0;
				lbCh |= AddTab(tabCount, 0, false, false, WTYPE_PANELS, NULL, NULL, 1, 0, 0, 0);
				gpTabs->Tabs.nTabCount = tabCount;
			}
			else
			{
				lbCh |= UpdateConEmuTabsApi(windowCount);
			}
		}
	}

	if (gpTabs)
	{
		if (gpTabs->Tabs.CurrentIndex == -1 && nLastCurrentTab != -1 && gpTabs->Tabs.nTabCount > 0)
		{
			// Активное окно определить не удалось
			if ((UINT)nLastCurrentTab >= gpTabs->Tabs.nTabCount)
				nLastCurrentTab = (gpTabs->Tabs.nTabCount - 1);

			gpTabs->Tabs.CurrentIndex = nLastCurrentTab;
			gpTabs->Tabs.tabs[nLastCurrentTab].Current = TRUE;
			gpTabs->Tabs.CurrentType = gpTabs->Tabs.tabs[nLastCurrentTab].Type;
		}

		if (gpTabs->Tabs.CurrentType == 0)
		{
			if (gpTabs->Tabs.CurrentIndex >= 0 && gpTabs->Tabs.CurrentIndex < (int)gpTabs->Tabs.nTabCount)
				gpTabs->Tabs.CurrentType = gpTabs->Tabs.tabs[nLastCurrentTab].Type;
			else
				gpTabs->Tabs.CurrentType = WTYPE_PANELS;
		}

		gnCurrentWindowType = gpTabs->Tabs.CurrentType;

		if (abSendChanges || gbForceSendTabs)
		{
			_ASSERTE((gbForceSendTabs==FALSE || IsDebuggerPresent()) && "Async SetWindow was timeouted?");
			gbForceSendTabs = FALSE;
			SendTabs(gpTabs->Tabs.nTabCount, lbCh && (gnReqCommand==(DWORD)-1));
		}
	}

	if (lbCh && gpBgPlugin)
	{
		gpBgPlugin->SetForceUpdate();
		gpBgPlugin->OnMainThreadActivated();
		gbNeedBgActivate = FALSE;
	}

	return lbCh;
}

// Вызывается при инициализации из SetStartupInfo[W] и при обновлении табов UpdateConEmuTabs[???]
// То есть по идее, это происходит только когда фар явно вызывает плагин (legal api calls)
void CPluginBase::CheckResources(bool abFromStartup)
{
	if (GetCurrentThreadId() != gnMainThreadId)
	{
		_ASSERTE(GetCurrentThreadId() == gnMainThreadId);
		return;
	}

	if (gsFarLang[0] && !abFromStartup)
	{
		static DWORD dwLastTickCount = GetTickCount();
		DWORD dwCurTick = GetTickCount();

		if ((dwCurTick - dwLastTickCount) < CHECK_RESOURCES_INTERVAL)
			return;

		dwLastTickCount = dwCurTick;
	}

	//if (abFromStartup) {
	//	_ASSERTE(gpConMapInfo!=NULL);
	//	if (!gpFarInfo)
	//		gpFarInfo = (CEFAR_INFO_MAPPING*)Alloc(sizeof(CEFAR_INFO_MAPPING),1);
	//}
	//if (gpConMapInfo)
	// Теперь он отвязан от gpConMapInfo
	ReloadFarInfo(TRUE);

	wchar_t szLang[64];
	if (gpConMapInfo)  //2010-12-13 Имеет смысл только при запуске из-под ConEmu
	{
		GetEnvironmentVariable(L"FARLANG", szLang, 63);

		if (abFromStartup || lstrcmpW(szLang, gsFarLang) || !gdwServerPID)
		{
			wchar_t szTitle[1024] = {0};
			GetConsoleTitleW(szTitle, 1024);
			SetConsoleTitleW(L"ConEmuC: CheckResources started");
			InitResources();
			DWORD dwServerPID = 0;
			FindServerCmd(CECMD_FARLOADED, dwServerPID);
			_ASSERTE(dwServerPID!=0);
			gdwServerPID = dwServerPID;
			SetConsoleTitleW(szTitle);
		}
		_ASSERTE(gdwServerPID!=0);
	}
}

// Передать в ConEmu строки с ресурсами
void CPluginBase::InitResources()
{
	// В ConEmu нужно передать следущие ресурсы
	struct {
		int MsgId; wchar_t* pszRc; size_t cchMax; LPCWSTR pszDef;
	} OurStr[] = {
		{CELngEdit, gpFarInfo->sLngEdit, countof(gpFarInfo->sLngEdit), L"edit"},
		{CELngView, gpFarInfo->sLngView, countof(gpFarInfo->sLngView), L"view"},
		{CELngTemp, gpFarInfo->sLngTemp, countof(gpFarInfo->sLngTemp), L"{Temporary panel"},
	};

	if (GetCurrentThreadId() == gnMainThreadId)
	{
		for (size_t i = 0; i < countof(OurStr); i++)
		{
			GetMsgA(OurStr[i].MsgId, OurStr[i].pszRc, OurStr[i].cchMax);
		}
	}

	if (!ghConEmuWndDC || !FarHwnd)
		return;

	int iAllLen = 0;
	for (size_t i = 0; i < countof(OurStr); i++)
	{
		if (!*OurStr[i].pszRc)
			lstrcpyn(OurStr[i].pszRc, OurStr[i].pszDef, OurStr[i].cchMax);
		iAllLen += lstrlen(OurStr[i].pszRc)+1;
	}

	int nSize = sizeof(CESERVER_REQ) + sizeof(DWORD) + iAllLen*sizeof(OurStr[0].pszRc[0]) + 2;
	CESERVER_REQ *pIn = (CESERVER_REQ*)Alloc(nSize,1);

	if (pIn)
	{
		ExecutePrepareCmd(&pIn->hdr, CECMD_RESOURCES, nSize);
		pIn->dwData[0] = GetCurrentProcessId();
		wchar_t* pszRes = (wchar_t*)&(pIn->dwData[1]);

		for (size_t i = 0; i < countof(OurStr); i++)
		{
			lstrcpyW(pszRes, OurStr[i].pszRc); pszRes += lstrlenW(pszRes)+1;
		}

		// Поправить nSize (он должен быть меньше)
		_ASSERTE(pIn->hdr.cbSize >= (DWORD)(((LPBYTE)pszRes) - ((LPBYTE)pIn)));
		pIn->hdr.cbSize = (DWORD)(((LPBYTE)pszRes) - ((LPBYTE)pIn));
		CESERVER_REQ* pOut = ExecuteGuiCmd(FarHwnd, pIn, FarHwnd);

		if (pOut)
		{
			if (pOut->DataSize() >= sizeof(FAR_REQ_FARSETCHANGED))
			{
				cmd_FarSetChanged(&pOut->FarSetChanged);
			}
			else
			{
				_ASSERTE(FALSE && "CECMD_RESOURCES failed (DataSize)");
			}
			ExecuteFreeResult(pOut);
		}
		else
		{
			_ASSERTE(pOut!=NULL && "CECMD_RESOURCES failed");
		}

		Free(pIn);
		GetEnvironmentVariable(L"FARLANG", gsFarLang, 63);
	}
}

void CPluginBase::CloseMapHeader()
{
	if (gpConMap)
		gpConMap->CloseMap();

	// delete для gpConMap здесь не делаем, может использоваться в других нитях!
	gpConMapInfo = NULL;
}

int CPluginBase::OpenMapHeader()
{
	int iRc = -1;

	CloseMapHeader();

	if (FarHwnd)
	{
		if (!gpConMap)
			gpConMap = new MFileMapping<CESERVER_CONSOLE_MAPPING_HDR>;

		gpConMap->InitName(CECONMAPNAME, (DWORD)FarHwnd); //-V205

		if (gpConMap->Open())
		{
			gpConMapInfo = gpConMap->Ptr();

			if (gpConMapInfo)
			{
				if (gpConMapInfo->hConEmuWndDc)
				{
					SetConEmuEnvVar(gpConMapInfo->hConEmuRoot);
					SetConEmuEnvVarChild(gpConMapInfo->hConEmuWndDc, gpConMapInfo->hConEmuWndBack);
				}
				//if (gpConMapInfo->nLogLevel)
				//	InstallTrapHandler();
				iRc = 0;
			}
		}
		else
		{
			gpConMapInfo = NULL;
		}

		//_wsprintf(szMapName, SKIPLEN(countof(szMapName)) CECONMAPNAME, (DWORD)FarHwnd);
		//ghFileMapping = OpenFileMapping(FILE_MAP_READ, FALSE, szMapName);
		//if (ghFileMapping)
		//{
		//	gpConMapInfo = (const CESERVER_CONSOLE_MAPPING_HDR*)MapViewOfFile(ghFileMapping, FILE_MAP_READ,0,0,0);
		//	if (gpConMapInfo)
		//	{
		//		//ReloadFarInfo(); -- смысла нет. SetStartupInfo еще не вызывался
		//		iRc = 0;
		//	}
		//	else
		//	{
		//		#ifdef _DEBUG
		//		dwErr = GetLastError();
		//		#endif
		//		CloseHandle(ghFileMapping);
		//		ghFileMapping = NULL;
		//	}
		//}
		//else
		//{
		//	#ifdef _DEBUG
		//	dwErr = GetLastError();
		//	#endif
		//}
	}

	return iRc;
}

void CPluginBase::InitRootRegKey()
{
	_ASSERTE(gFarVersion.dwVerMajor==1 || gFarVersion.dwVerMajor==2);
	// начальная инициализация. в SetStartupInfo поправим
	LPCWSTR pszFarName = (gFarVersion.dwVerMajor==3) ? L"Far Manager" :
		(gFarVersion.dwVerMajor==2) ? L"FAR2"
		: L"FAR";

	// Нужно учесть, что FAR мог запуститься с ключом /u (выбор конфигурации)
	wchar_t szFarUser[MAX_PATH];
	if (GetEnvironmentVariable(L"FARUSER", szFarUser, countof(szFarUser)) == 0)
		szFarUser[0] = 0;

	SafeFree(ms_RootRegKey);
	if (szFarUser[0])
		ms_RootRegKey = lstrmerge(L"Software\\", pszFarName, L"\\Users\\", szFarUser);
	else
		ms_RootRegKey = lstrmerge(L"Software\\", pszFarName);
}

void CPluginBase::SetRootRegKey(wchar_t* asKeyPtr)
{
	SafeFree(ms_RootRegKey);
	ms_RootRegKey = asKeyPtr;

	int nLen = ms_RootRegKey ? lstrlen(ms_RootRegKey) : 0;
	// Тут к нам приходит путь к настройкам НАШЕГО плагина
	// А на нужно получить "общий" ключ (для последующего считывания LoadPanelTabsFromRegistry
	if (nLen > 0)
	{
		if (ms_RootRegKey[nLen-1] == L'\\')
			ms_RootRegKey[nLen-1] = 0;
		wchar_t* pszName = wcsrchr(ms_RootRegKey, L'\\');
		if (pszName)
			*pszName = 0;
		else
			SafeFree(ms_RootRegKey);
	}
}

void CPluginBase::LoadPanelTabsFromRegistry()
{
	if (!ms_RootRegKey || !*ms_RootRegKey)
		return;

	wchar_t* pszTabsKey = lstrmerge(ms_RootRegKey, L"\\Plugins\\PanelTabs");
	if (!pszTabsKey)
		return;

	HKEY hk;
	if (0 == RegOpenKeyExW(HKEY_CURRENT_USER, szTabsKey, 0, KEY_READ, &hk))
	{
		DWORD dwVal, dwSize;

		if (!RegQueryValueExW(hk, L"SeparateTabs", NULL, NULL, (LPBYTE)&dwVal, &(dwSize = sizeof(dwVal))))
			gpFarInfo->PanelTabs.SeparateTabs = dwVal ? 1 : 0;

		if (!RegQueryValueExW(hk, L"ButtonColor", NULL, NULL, (LPBYTE)&dwVal, &(dwSize = sizeof(dwVal))))
			gpFarInfo->PanelTabs.ButtonColor = dwVal & 0xFF;

		RegCloseKey(hk);
	}

	free(pszTabsKey);
}

void CPluginBase::InitHWND()
{
	gsFarLang[0] = 0;

	bool lbExportsChanged = false;
	if (!gFarVersion.dwVerMajor)
	{
		LoadFarVersion();  // пригодится уже здесь!

		if (gFarVersion.dwVerMajor == 3)
		{
			lbExportsChanged = ChangeExports( Far3Func, ghPluginModule );
			if (!lbExportsChanged)
			{
				_ASSERTE(lbExportsChanged);
			}
		}
	}


	// Returns HWND of ...
	//  aiType==0: Gui console DC window
	//        ==1: Gui Main window
	//        ==2: Console window
	FarHwnd = GetConEmuHWND(2/*Console window*/);
	ghConEmuWndDC = GetConEmuHWND(0/*Gui console DC window*/);


	{
		// TrueColor buffer check
		wchar_t szMapName[64];
		_wsprintf(szMapName, SKIPLEN(countof(szMapName)) L"Console2_consoleBuffer_%d", (DWORD)GetCurrentProcessId());
		HANDLE hConsole2 = OpenFileMapping(FILE_MAP_READ, FALSE, szMapName);
		gbStartedUnderConsole2 = (hConsole2 != NULL);

		if (hConsole2)
			CloseHandle(hConsole2);
	}

	// CtrlShiftF3 - для MMView & PicView
	if (!ghConEmuCtrlPressed)
	{
		wchar_t szName[64];
		_wsprintf(szName, SKIPLEN(countof(szName)) CEKEYEVENT_CTRL, gnSelfPID);
		ghConEmuCtrlPressed = CreateEvent(NULL, TRUE, FALSE, szName);
		if (ghConEmuCtrlPressed) ResetEvent(ghConEmuCtrlPressed); else { _ASSERTE(ghConEmuCtrlPressed); }

		_wsprintf(szName, SKIPLEN(countof(szName)) CEKEYEVENT_SHIFT, gnSelfPID);
		ghConEmuShiftPressed = CreateEvent(NULL, TRUE, FALSE, szName);
		if (ghConEmuShiftPressed) ResetEvent(ghConEmuShiftPressed); else { _ASSERTE(ghConEmuShiftPressed); }
	}

	OpenMapHeader();
	// Проверить, созданы ли буферы для True-Colorer
	// Это для того, чтобы пересоздать их при детаче
	//CheckColorerHeader();
	//memset(hEventCmd, 0, sizeof(HANDLE)*MAXCMDCOUNT);
	//int nChk = 0;
	//ghConEmuWndDC = GetConEmuHWND(FALSE/*abRoot*/  /*, &nChk*/);
	gnMsgTabChanged = RegisterWindowMessage(CONEMUTABCHANGED);

	if (!ghSetWndSendTabsEvent) ghSetWndSendTabsEvent = CreateEvent(0,0,0,0);

	// Даже если мы не в ConEmu - все равно запустить нить, т.к. в ConEmu теперь есть возможность /Attach!
	//WCHAR szEventName[128];
	DWORD dwCurProcId = GetCurrentProcessId();

	if (!ghReqCommandEvent)
	{
		ghReqCommandEvent = CreateEvent(NULL,FALSE,FALSE,NULL);
		_ASSERTE(ghReqCommandEvent!=NULL);
	}

	if (!ghPluginSemaphore)
	{
		ghPluginSemaphore = CreateSemaphore(NULL, 1, 1, NULL);
		_ASSERTE(ghPluginSemaphore!=NULL);
	}

	// Запустить сервер команд
	if (!PlugServerStart())
	{
		TODO("Показать ошибку");
	}

	ghConsoleWrite = CreateEvent(NULL,FALSE,FALSE,NULL);
	_ASSERTE(ghConsoleWrite!=NULL);
	ghConsoleInputEmpty = CreateEvent(NULL,FALSE,FALSE,NULL);
	_ASSERTE(ghConsoleInputEmpty!=NULL);
	ghMonitorThread = CreateThread(NULL, 0, MonitorThreadProcW, 0, 0, &gnMonitorThreadId);

	//ghInputThread = CreateThread(NULL, 0, InputThreadProcW, 0, 0, &gnInputThreadId);

	// Если мы не под эмулятором - больше ничего делать не нужно
	if (ghConEmuWndDC)
	{
		//
		DWORD dwPID, dwThread;
		dwThread = GetWindowThreadProcessId(ghConEmuWndDC, &dwPID);
		typedef BOOL (WINAPI* AllowSetForegroundWindowT)(DWORD);
		HMODULE hUser32 = GetModuleHandle(L"user32.dll");

		if (hUser32)
		{
			AllowSetForegroundWindowT AllowSetForegroundWindowF = (AllowSetForegroundWindowT)GetProcAddress(hUser32, "AllowSetForegroundWindow");

			if (AllowSetForegroundWindowF) AllowSetForegroundWindowF(dwPID);
		}

		// дернуть табы, если они нужны
		int tabCount = 0;
		MSectionLock SC; SC.Lock(csTabs);
		CreateTabs(1);
		AddTab(tabCount, 0, false, false, WTYPE_PANELS, NULL, NULL, 1, 0, 0, 0);
		// Сейчас отсылать не будем - выполним, когда вызовется SetStartupInfo -> CommonStartup
		//SendTabs(tabCount=1, TRUE);
		SC.Unlock();
	}
}

void CPluginBase::CheckConEmuDetached()
{
	if (ghConEmuWndDC)
	{
		// ConEmu могло подцепиться
		MFileMapping<CESERVER_CONSOLE_MAPPING_HDR> ConMap;
		ConMap.InitName(CECONMAPNAME, (DWORD)FarHwnd); //-V205

		if (ConMap.Open())
		{
			if (ConMap.Ptr()->hConEmuWndDc == NULL)
			{
				ghConEmuWndDC = NULL;
			}

			ConMap.CloseMap();
		}
		else
		{
			ghConEmuWndDC = NULL;
		}
	}
}

// Эту нить нужно оставить, чтобы была возможность отобразить консоль при падении ConEmu
// static, WINAPI
DWORD CPluginBase::MonitorThreadProcW(LPVOID lpParameter)
{
	//DWORD dwProcId = GetCurrentProcessId();
	DWORD dwStartTick = GetTickCount();
	//DWORD dwMonitorTick = dwStartTick;
	BOOL lbStartedNoConEmu = (ghConEmuWndDC == NULL) && !gbStartedUnderConsole2;
	//BOOL lbTryOpenMapHeader = FALSE;
	//_ASSERTE(ghConEmuWndDC!=NULL); -- ConEmu может подцепиться позднее!

	WARNING("В MonitorThread нужно также отслеживать и 'живость' сервера. Иначе приложение останется невидимым (");

	while(true)
	{
		DWORD dwWait = 0;
		DWORD dwTimeout = 500;
		/*#ifdef _DEBUG
		dwTimeout = INFINITE;
		#endif*/
		//dwWait = WaitForMultipleObjects(MAXCMDCOUNT, hEventCmd, FALSE, dwTimeout);
		dwWait = WaitForSingleObject(ghServerTerminateEvent, dwTimeout);

		if (dwWait == WAIT_OBJECT_0)
			break; // завершение плагина

		// Если FAR запущен в "невидимом" режиме и по истечении таймаута
		// так и не подцепились к ConEmu - всплыть окошко консоли
		if (lbStartedNoConEmu && ghConEmuWndDC == NULL && FarHwnd != NULL)
		{
			DWORD dwCurTick = GetTickCount();
			DWORD dwDelta = dwCurTick - dwStartTick;

			if (dwDelta > GUI_ATTACH_TIMEOUT)
			{
				lbStartedNoConEmu = FALSE;

				if (!TerminalMode && !IsWindowVisible(FarHwnd))
				{
					EmergencyShow(FarHwnd);
				}
			}
		}

		// Теоретически, нить обработки может запуститься и без ghConEmuWndDC (под телнетом)
		if (ghConEmuWndDC && FarHwnd && (dwWait == WAIT_TIMEOUT))
		{
			// Может быть ConEmu свалилось
			if (!IsWindow(ghConEmuWndDC) && ghConEmuWndDC)
			{
				HWND hConWnd = GetConEmuHWND(2);

				if ((hConWnd && !IsWindow(hConWnd))
					|| (!gbWasDetached && FarHwnd && !IsWindow(FarHwnd)))
				{
					// hConWnd не валидно
					wchar_t szWarning[255];
					_wsprintf(szWarning, SKIPLEN(countof(szWarning)) L"Console was abnormally termintated!\r\nExiting from FAR (PID=%u)", GetCurrentProcessId());
					MessageBox(0, szWarning, L"ConEmu plugin", MB_OK|MB_ICONSTOP|MB_SETFOREGROUND);
					TerminateProcess(GetCurrentProcess(), 100);
					return 0;
				}

				if (!TerminalMode && !IsWindowVisible(FarHwnd))
				{
					EmergencyShow(FarHwnd);
				}
				else if (!gbWasDetached)
				{
					gbWasDetached = TRUE;
					ghConEmuWndDC = NULL;
				}
			}
		}

		if (gbWasDetached && !ghConEmuWndDC)
		{
			// ConEmu могло подцепиться
			if (gpConMapInfo && gpConMapInfo->hConEmuWndDc && IsWindow(gpConMapInfo->hConEmuWndDc))
			{
				gbWasDetached = FALSE;
				ghConEmuWndDC = (HWND)gpConMapInfo->hConEmuWndDc;

				// Update our in-process env vars
				SetConEmuEnvVar(gpConMapInfo->hConEmuRoot);
				SetConEmuEnvVarChild(gpConMapInfo->hConEmuWndDc, gpConMapInfo->hConEmuWndBack);

				// Передернуть отрисовку, чтобы обновить TrueColor
				Plugin()->RedrawAll();

				// Inform GUI about our Far/Plugin
				InitResources();

				// Обновить ТАБЫ после реаттача
				if (gnCurTabCount && gpTabs)
				{
					SendTabs(gnCurTabCount, TRUE);
				}
			}
		}

		//if (ghConEmuWndDC && gbMonitorEnvVar && gsMonitorEnvVar[0]
		//        && (GetTickCount() - dwMonitorTick) > MONITORENVVARDELTA)
		//{
		//	UpdateEnvVar(gsMonitorEnvVar);
		//	dwMonitorTick = GetTickCount();
		//}

		if (gbNeedPostTabSend)
		{
			DWORD nDelta = GetTickCount() - gnNeedPostTabSendTick;

			if (nDelta > NEEDPOSTTABSENDDELTA)
			{
				if (Plugin()->IsMacroActive())
				{
					gnNeedPostTabSendTick = GetTickCount();
				}
				else
				{
					// Force Send tabs to ConEmu
					MSectionLock SC; SC.Lock(csTabs, TRUE); // блокируем exclusively, чтобы во время пересылки данные не поменялись из другого потока
					SendTabs(gnCurTabCount, TRUE);
					SC.Unlock();
				}
			}
		}

		if (/*ghConEmuWndDC &&*/ gbTryOpenMapHeader)
		{
			if (gpConMapInfo)
			{
				_ASSERTE(gpConMapInfo == NULL);
				gbTryOpenMapHeader = FALSE;
			}
			else if (OpenMapHeader() == 0)
			{
				// OK, переподцепились
				gbTryOpenMapHeader = FALSE;
			}

			if (gpConMapInfo)
			{
				// 04.03.2010 Maks - Если мэппинг открыли - принудительно передернуть ресурсы и информацию
				//CheckResources(true); -- должен выполняться в основной нити, поэтому - через Activate
				// 22.09.2010 Maks - вызывать ActivatePlugin - некорректно!
				//ActivatePlugin(CMD_CHKRESOURCES, NULL);
				ProcessCommand(CMD_CHKRESOURCES, TRUE/*bReqMainThread*/, NULL);
			}
		}

		if (gbStartupHooksAfterMap && gpConMapInfo && ghConEmuWndDC && IsWindow(ghConEmuWndDC))
		{
			gbStartupHooksAfterMap = FALSE;
			StartupHooks(ghPluginModule);
		}

		if (gpBgPlugin)
		{
			gpBgPlugin->MonitorBackground();
		}

		//if (gpConMapInfo) {
		//	if (gpConMapInfo->nFarPID == 0)
		//		gbNeedReloadFarInfo = TRUE;
		//}
	}

	return 0;
}

HANDLE CPluginBase::OpenPluginCommon(int OpenFrom, INT_PTR Item, bool FromMacro)
{
	if (!mb_StartupInfoOk)
		return InvalidPanelHandle;

	HANDLE hResult = InvalidPanelHandle;
	INT_PTR nID = pcc_None; // выбор из меню

	#ifdef _DEBUG
	if (gFarVersion.dwVerMajor==1)
	{
		wchar_t szInfo[128]; _wsprintf(szInfo, SKIPLEN(countof(szInfo)) L"OpenPlugin[Ansi] (%i%s, Item=0x%X, gnReqCmd=%i%s)\n",
		                               OpenFrom, (OpenFrom==OPEN_COMMANDLINE) ? L"[OPEN_COMMANDLINE]" :
		                               (OpenFrom==OPEN_PLUGINSMENU) ? L"[OPEN_PLUGINSMENU]" : L"",
		                               (DWORD)Item,
		                               (int)gnReqCommand,
		                               (gnReqCommand == (DWORD)-1) ? L"" :
		                               (gnReqCommand == CMD_REDRAWFAR) ? L"[CMD_REDRAWFAR]" :
		                               (gnReqCommand == CMD_EMENU) ? L"[CMD_EMENU]" :
		                               (gnReqCommand == CMD_SETWINDOW) ? L"[CMD_SETWINDOW]" :
		                               (gnReqCommand == CMD_POSTMACRO) ? L"[CMD_POSTMACRO]" :
		                               L"");
		OutputDebugStringW(szInfo);
	}
	#endif

	if (OpenFrom == OPEN_COMMANDLINE && Item)
	{
		if (gFarVersion.dwVerMajor==1)
		{
			wchar_t* pszUnicode = ToUnicode((char*)Item);
			ProcessCommandLine(pszUnicode);
			SafeFree(pszUnicode);
		}
		else
		{
			ProcessCommandLine((wchar_t*)Item);
		}
		goto wrap;
	}

	if (gnReqCommand != (DWORD)-1)
	{
		gnPluginOpenFrom = (OpenFrom & 0xFFFF);
		ProcessCommand(gnReqCommand, FALSE/*bReqMainThread*/, gpReqCommandData);
		goto wrap;
	}

	if (FromMacro)
	{
		if (Item >= 0x4000)
		{
			// Хорошо бы, конечно точнее определять, строка это, или нет...
			LPCWSTR pszCallCmd = (LPCWSTR)Item;

			if (!IsBadStringPtrW(pszCallCmd, 255) && *pszCallCmd)
			{
				if (!ghConEmuWndDC)
				{
					SetEnvironmentVariable(CEGUIMACRORETENVVAR, NULL);
				}
				else
				{
					int nLen = lstrlenW(pszCallCmd);
					CESERVER_REQ *pIn = NULL, *pOut = NULL;
					pIn = ExecuteNewCmd(CECMD_GUIMACRO, sizeof(CESERVER_REQ_HDR)+sizeof(CESERVER_REQ_GUIMACRO)+nLen*sizeof(wchar_t));
					lstrcpyW(pIn->GuiMacro.sMacro, pszCallCmd);
					pOut = ExecuteGuiCmd(FarHwnd, pIn, FarHwnd);

					if (pOut)
					{
						SetEnvironmentVariable(CEGUIMACRORETENVVAR,
						                       pOut->GuiMacro.nSucceeded ? pOut->GuiMacro.sMacro : NULL);
						ExecuteFreeResult(pOut);
						// 130708 -- Otherwise Far Macro "Plugin.Call" returns "0" always...
						hResult = (HANDLE)TRUE;
					}
					else
					{
						SetEnvironmentVariable(CEGUIMACRORETENVVAR, NULL);
					}

					ExecuteFreeResult(pIn);
				}
			}

			goto wrap;
		}

		if (Item >= pcc_First && Item <= pcc_Last)
		{
			nID = Item; // Будет сразу выполнена команда
		}
		else if (Item >= SETWND_CALLPLUGIN_BASE)
		{
			// Переключение табов выполняется макросом, чтобы "убрать" QSearch и выполнить проверки
			// (посылается из OnMainThreadActivated: gnReqCommand == CMD_SETWINDOW)
			DEBUGSTRCMD(L"Plugin: SETWND_CALLPLUGIN_BASE\n");
			gnPluginOpenFrom = OPEN_PLUGINSMENU;
			DWORD nTab = (DWORD)(Item - SETWND_CALLPLUGIN_BASE);
			ProcessCommand(CMD_SETWINDOW, FALSE, &nTab);
			SetEvent(ghSetWndSendTabsEvent);
			goto wrap;
		}
		else if (Item == CE_CALLPLUGIN_SENDTABS)
		{
			DEBUGSTRCMD(L"Plugin: CE_CALLPLUGIN_SENDTABS\n");
			// Force Send tabs to ConEmu
			//MSectionLock SC; SC.Lock(csTabs, TRUE);
			//SendTabs(gnCurTabCount, TRUE);
			//SC.Unlock();
			UpdateConEmuTabs(true);
			SetEvent(ghSetWndSendTabsEvent);
			goto wrap;
		}
		else if (Item == CE_CALLPLUGIN_UPDATEBG)
		{
			if (gpBgPlugin)
			{
				gpBgPlugin->SetForceUpdate(true);
				gpBgPlugin->OnMainThreadActivated();
			}
			goto wrap;
		}
	}

	ShowPluginMenu((PluginCallCommands)nID);

wrap:
	#ifdef _DEBUG
	if ((gFarVersion.dwVerMajor==1) && (gnReqCommand != (DWORD)-1))
	{
		wchar_t szInfo[128]; _wsprintf(szInfo, SKIPLEN(countof(szInfo)) L"*** OpenPlugin[Ansi] post gnReqCmd=%i%s\n",
		                               (int)gnReqCommand,
		                               (gnReqCommand == (DWORD)-1) ? L"" :
		                               (gnReqCommand == CMD_REDRAWFAR) ? L"CMD_REDRAWFAR" :
		                               (gnReqCommand == CMD_EMENU) ? L"CMD_EMENU" :
		                               (gnReqCommand == CMD_SETWINDOW) ? L"CMD_SETWINDOW" :
		                               (gnReqCommand == CMD_POSTMACRO) ? L"CMD_POSTMACRO" :
		                               L"");
		OutputDebugStringW(szInfo);
	}
	#endif
	return hResult;
}

void CPluginBase::ExitFarCommon()
{
	ShutdownPluginStep(L"ExitFarCmn");

	gbExitFarCalled = true;

	// Плагин выгружается, Вызывать Syncho больше нельзя
	gbSynchroProhibited = true;
	ShutdownHooks();
	StopThread();

	ShutdownPluginStep(L"ExitFarCmn - done");
}

// Вызывается из ACTL_SYNCHRO для FAR2
// или при ConsoleReadInput(1) в FAR1
void CPluginBase::OnMainThreadActivated()
{
	// Теоретически, в FAR2 мы сюда можем попасть и не из основной нити,
	// если таки будет переделана "thread-safe" активация.
	if (gbNeedPostEditCheck)
	{
		DWORD currentModifiedState = Plugin()->GetEditorModifiedState();

		if (lastModifiedStateW != (int)currentModifiedState)
		{
			lastModifiedStateW = (int)currentModifiedState;
			gbRequestUpdateTabs = TRUE;
		}

		// 100909 - не было
		gbNeedPostEditCheck = FALSE;
	}

	// To avoid spare API calls
	int iMacroActive = 0;

	if (!gbRequestUpdateTabs && gbNeedPostTabSend)
	{
		if (!Plugin()->isMacroActive(iMacroActive))
		{
			gbRequestUpdateTabs = TRUE; gbNeedPostTabSend = FALSE;
		}
	}

	if (gbRequestUpdateTabs && !Plugin()->isMacroActive(iMacroActive))
	{
		gbRequestUpdateTabs = gbNeedPostTabSend = FALSE;
		Plugin()->UpdateConEmuTabs(true);

		if (gbClosingModalViewerEditor)
		{
			gbClosingModalViewerEditor = FALSE;
			gbRequestUpdateTabs = TRUE;
		}
	}

	// Retrieve current panel CD's
	// Remove (gnCurrentWindowType == WTYPE_PANELS) restriction,
	// panel paths may be changed even from editor
	if (!Plugin()->isMacroActive(iMacroActive))
	{
		Plugin()->UpdatePanelDirs();
	}

	// !!! Это только чисто в OnConsolePeekReadInput, т.к. FAR Api тут не используется
	//if (gpConMapInfo && gpFarInfo && gpFarInfoMapping)
	//	TouchReadPeekConsoleInputs(abPeek ? 1 : 0);

	if (gbNeedPostReloadFarInfo)
	{
		gbNeedPostReloadFarInfo = FALSE;
		ReloadFarInfo(FALSE);
	}

	// !!! Это только чисто в OnConsolePeekReadInput, т.к. FAR Api тут не используется
	//// В некоторых случаях (CMD_LEFTCLKSYNC,CMD_CLOSEQSEARCH,...) нужно дождаться, пока очередь опустеет
	//if (gbWaitConsoleInputEmpty)
	//{
	//	DWORD nTestEvents = 0;
	//	HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
	//	if (GetNumberOfConsoleInputEvents(h, &nTestEvents))
	//	{
	//		if (nTestEvents == 0)
	//		{
	//			gbWaitConsoleInputEmpty = FALSE;
	//			SetEvent(ghConsoleInputEmpty);
	//		}
	//	}
	//}

	// Если был запрос на обновление Background
	if (gbNeedBgActivate)  // выставляется в gpBgPlugin->SetForceCheck() или SetForceUpdate()
	{
		gbNeedBgActivate = FALSE;

		if (gpBgPlugin)
			gpBgPlugin->OnMainThreadActivated();
	}

	// Проверяем, надо ли "активировать" плагин?
	if (!gbReqCommandWaiting || gnReqCommand == (DWORD)-1)
	{
		return; // активация в данный момент не требуется
	}

	gbReqCommandWaiting = FALSE; // чтобы ожидающая нить случайно не удалила параметры, когда мы работаем
	TODO("Определить текущую область... (panel/editor/viewer/menu/...");
	gnPluginOpenFrom = 0;

	// Обработка CtrlTab из ConEmu
	if (gnReqCommand == CMD_SETWINDOW)
	{
		ProcessSetWindowCommand();
	}
	else
	{
		// Результата ожидает вызывающая нить, поэтому передаем параметр
		ProcessCommand(gnReqCommand, FALSE/*bReqMainThread*/, gpReqCommandData, &gpCmdRet);
		// Но не освобождаем его (pCmdRet) - это сделает ожидающая нить
	}

	// Мы закончили
	SetEvent(ghReqCommandEvent);
}

void CPluginBase::ProcessSetWindowCommand()
{
	// Обработка CtrlTab из ConEmu
	_ASSERTE(gnReqCommand == CMD_SETWINDOW);
	DEBUGSTRCMD(L"Plugin: OnMainThreadActivated: CMD_SETWINDOW\n");

	if (gFarVersion.dwVerMajor==1)
	{
		gnPluginOpenFrom = OPEN_PLUGINSMENU;
		// Результата ожидает вызывающая нить, поэтому передаем параметр
		ProcessCommand(gnReqCommand, FALSE/*bReqMainThread*/, gpReqCommandData, &gpCmdRet);
	}
	else
	{
		// Необходимо быть в panel/editor/viewer
		wchar_t szMacro[255];
		DWORD nTabShift = SETWND_CALLPLUGIN_BASE + *((DWORD*)gpReqCommandData);
		// Если панели-редактор-вьювер - сменить окно. Иначе - отослать в GUI табы
		if (gFarVersion.dwVerMajor == 2)
		{
			_wsprintf(szMacro, SKIPLEN(countof(szMacro)) L"$if (Search) Esc $end $if (Shell||Viewer||Editor) callplugin(0x%08X,%i) $else callplugin(0x%08X,%i) $end",
				  ConEmu_SysID, nTabShift, ConEmu_SysID, CE_CALLPLUGIN_SENDTABS);
		}
		else if (!gFarVersion.IsFarLua())
		{
			_wsprintf(szMacro, SKIPLEN(countof(szMacro)) L"$if (Search) Esc $end $if (Shell||Viewer||Editor) callplugin(\"%s\",%i) $else callplugin(\"%s\",%i) $end",
				  ConEmu_GuidS, nTabShift, ConEmu_GuidS, CE_CALLPLUGIN_SENDTABS);
		}
		else
		{
			_wsprintf(szMacro, SKIPLEN(countof(szMacro)) L"if Area.Search then Keys(\"Esc\") end if Area.Shell or Area.Viewer or Area.Editor then Plugin.Call(\"%s\",%i) else Plugin.Call(\"%s\",%i) end",
				  ConEmu_GuidS, nTabShift, ConEmu_GuidS, CE_CALLPLUGIN_SENDTABS);
		}
		gnReqCommand = -1;
		gpReqCommandData = NULL;
		PostMacro(szMacro, NULL);
	}
	// Done
}

void CPluginBase::CommonPluginStartup()
{
	gbBgPluginsAllowed = TRUE;

	//111209 - CheckResources зовем перед UpdateConEmuTabs, т.к. иначе CheckResources вызывается дважды
	//2010-12-13 информацию (начальную) о фаре грузим всегда, а отсылаем в GUI только если в ConEmu
	// здесь же и ReloadFarInfo() позовется
	CheckResources(true);

	// Надо табы загрузить
	Plugin()->UpdateConEmuTabs(true);


	// Пробежаться по всем загруженным в данный момент плагинам и дернуть в них "OnConEmuLoaded"
	// А все из за того, что при запуске "Far.exe /co" - порядок загрузки плагинов МЕНЯЕТСЯ
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, 0);
	if (snapshot != INVALID_HANDLE_VALUE)
	{
		MODULEENTRY32 module = {sizeof(MODULEENTRY32)};

		for (BOOL res = Module32First(snapshot, &module); res; res = Module32Next(snapshot, &module))
		{
			OnConEmuLoaded_t fnOnConEmuLoaded;

			if (((fnOnConEmuLoaded = (OnConEmuLoaded_t)GetProcAddress(module.hModule, "OnConEmuLoaded")) != NULL)
				&& /* Наверное, только для плагинов фара */
				((GetProcAddress(module.hModule, "SetStartupInfoW") || GetProcAddress(module.hModule, "SetStartupInfo"))))
			{
				OnLibraryLoaded(module.hModule);
			}
		}

		CloseHandle(snapshot);
	}


	//if (gpConMapInfo)  //2010-03-04 Имеет смысл только при запуске из-под ConEmu
	//{
	//	//CheckResources(true);
	//	LogCreateProcessCheck((LPCWSTR)-1);
	//}

	TODO("перенести инициализацию фаровских callback'ов в SetStartupInfo, т.к. будет грузиться как Inject!");

	if (!StartupHooks(ghPluginModule))
	{
		if (ghConEmuWndDC)
		{
			_ASSERTE(FALSE);
			DEBUGSTR(L"!!! Can't install injects!!!\n");
		}
		else
		{
			DEBUGSTR(L"No GUI, injects was not installed!\n");
		}
	}
}

void CPluginBase::StopThread()
{
	ShutdownPluginStep(L"StopThread");
	#ifdef _DEBUG
	LPCVOID lpPtrConInfo = gpConMapInfo;
	#endif
	gpConMapInfo = NULL;
	//LPVOID lpPtrColorInfo = gpColorerInfo; gpColorerInfo = NULL;
	gbBgPluginsAllowed = FALSE;
	NotifyConEmuUnloaded();

	ShutdownPluginStep(L"...ClosingTabs");
	CloseTabs();

	//if (hEventCmd[CMD_EXIT])
	//	SetEvent(hEventCmd[CMD_EXIT]); // Завершить нить

	if (ghServerTerminateEvent)
	{
		SetEvent(ghServerTerminateEvent);
	}

	//if (gnInputThreadId) {
	//	PostThreadMessage(gnInputThreadId, WM_QUIT, 0, 0);
	//}

	ShutdownPluginStep(L"...Stopping server");
	PlugServerStop();

	ShutdownPluginStep(L"...Finalizing");

	SafeCloseHandle(ghPluginSemaphore);

	if (ghMonitorThread)  // подождем чуть-чуть, или принудительно прибъем нить ожидания
	{
		if (WaitForSingleObject(ghMonitorThread,1000))
		{
#if !defined(__GNUC__)
#pragma warning (disable : 6258)
#endif
			TerminateThread(ghMonitorThread, 100);
		}

		SafeCloseHandle(ghMonitorThread);
	}

	//if (ghInputThread) { // подождем чуть-чуть, или принудительно прибъем нить ожидания
	//	if (WaitForSingleObject(ghInputThread,1000)) {
	//		#if !defined(__GNUC__)
	//		#pragma warning (disable : 6258)
	//		#endif
	//		TerminateThread(ghInputThread, 100);
	//	}
	//	SafeCloseHandle(ghInputThread);
	//}

	if (gpTabs)
	{
		Free(gpTabs);
		gpTabs = NULL;
	}

	if (ghReqCommandEvent)
	{
		CloseHandle(ghReqCommandEvent); ghReqCommandEvent = NULL;
	}

	if (gpFarInfo)
	{
		LPVOID ptr = gpFarInfo; gpFarInfo = NULL;
		Free(ptr);
	}

	if (gpFarInfoMapping)
	{
		UnmapViewOfFile(gpFarInfoMapping);
		CloseHandle(ghFarInfoMapping);
		ghFarInfoMapping = NULL;
	}

	if (ghFarAliveEvent)
	{
		CloseHandle(ghFarAliveEvent);
		ghFarAliveEvent = NULL;
	}

	if (ghRegMonitorKey) { RegCloseKey(ghRegMonitorKey); ghRegMonitorKey = NULL; }

	SafeCloseHandle(ghRegMonitorEvt);
	SafeCloseHandle(ghServerTerminateEvent);
	//WARNING("Убрать, заменить ghConIn на GetStdHandle()"); // Иначе в Win7 будет буфер разрушаться
	//SafeCloseHandle(ghConIn);
	//SafeCloseHandle(ghInputSynchroExecuted);
	SafeCloseHandle(ghSetWndSendTabsEvent);
	SafeCloseHandle(ghConsoleInputEmpty);
	SafeCloseHandle(ghConsoleWrite);

	if (gpConMap)
	{
		gpConMap->CloseMap();
		delete gpConMap;
		gpConMap = NULL;
	}

	//if (lpPtrConInfo)
	//{
	//	UnmapViewOfFile(lpPtrConInfo);
	//}
	//if (ghFileMapping)
	//{
	//	CloseHandle(ghFileMapping);
	//	ghFileMapping = NULL;
	//}
	// -- теперь мэппинги создает GUI
	//CloseColorerHeader();

	CommonShutdown();
	ShutdownPluginStep(L"StopThread - done");
}

void CPluginBase::ShutdownPluginStep(LPCWSTR asInfo, int nParm1 /*= 0*/, int nParm2 /*= 0*/, int nParm3 /*= 0*/, int nParm4 /*= 0*/)
{
#ifdef _DEBUG
	static int nDbg = 0;
	if (!nDbg)
		nDbg = IsDebuggerPresent() ? 1 : 2;
	if (nDbg != 1)
		return;
	wchar_t szFull[512];
	msprintf(szFull, countof(szFull), L"%u:ConEmuP:PID=%u:TID=%u: ",
		GetTickCount(), GetCurrentProcessId(), GetCurrentThreadId());
	if (asInfo)
	{
		int nLen = lstrlen(szFull);
		msprintf(szFull+nLen, countof(szFull)-nLen, asInfo, nParm1, nParm2, nParm3, nParm4);
	}
	lstrcat(szFull, L"\n");
	OutputDebugString(szFull);
#endif
}

// Теоретически, из этой функции Far2+ может сразу вызвать ProcessSynchroEventW.
// Но в текущей версии Far 2/3 она работает асинхронно и сразу выходит, а сама
// ProcessSynchroEventW зовется потом в главной нити (где-то при чтении буфера консоли)
void CPluginBase::ExecuteSynchro()
{
	WARNING("Нет способа определить, будет ли фар вызывать наш ProcessSynchroEventW и в какой момент");
	// Например, если в фаре выставлен ProcessException - то никакие плагины больше не зовутся

	if (IS_SYNCHRO_ALLOWED)
	{
		if (gbSynchroProhibited)
		{
			_ASSERTE(gbSynchroProhibited==false);
			return;
		}

		//Чтобы не было зависаний при попытке активации плагина во время прокрутки
		//редактора, в плагине мониторить нажатие мыши. Если последнее МЫШИНОЕ событие
		//было с нажатой кнопкой - сначала пульнуть в консоль команду "отпускания" кнопки,
		//и только после этого - пытаться активироваться.
		if ((gnAllowDummyMouseEvent > 0) && (gLastMouseReadEvent.dwButtonState & (RIGHTMOST_BUTTON_PRESSED|FROM_LEFT_1ST_BUTTON_PRESSED)))
		{
			//_ASSERTE(!(gLastMouseReadEvent.dwButtonState & (RIGHTMOST_BUTTON_PRESSED|FROM_LEFT_1ST_BUTTON_PRESSED)));
			int nWindowType = Plugin()->GetActiveWindowType();
			// "Зависания" возможны (вроде) только при прокрутке зажатой кнопкой мышки
			// редактора или вьювера. Так что в других областях - не дергаться.
			if (nWindowType == WTYPE_EDITOR || nWindowType == WTYPE_VIEWER)
			{
				gbUngetDummyMouseEvent = TRUE;
			}
		}

		Plugin()->ExecuteSynchroApi();
	}
}
