
// WinSaltyNESDlg.cpp : implementation file
//

#include "stdafx.h"
#include "WinSaltyNES.h"
#include "WinSaltyNESDlg.h"
#include "afxdialogex.h"

#include "NES/NES.h"
#include "NES/NESRom.h"
#include "NES/Ppu.h"
#include "NES/Cpu6502.h"
#include "Util/Stopwatch.h"
//#include "Util/ComPtr.h"

#include <Shlobj.h>
#include <stdexcept>

#include <xaudio2.h>
#include <Xinput.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define WM_RENDERFRAME (WM_APP + 1)

enum
{
	TIMER_REDRAW,
	TIMER_TESTRENDER
};

class CWin32ReadOnlyFile : public IReadableFile
{
public:
	CWin32ReadOnlyFile(LPCWSTR wzFileStr)
		: m_fileName(wzFileStr)
	{
		m_hFile = CreateFileW(wzFileStr, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	}

	virtual ~CWin32ReadOnlyFile() override
	{
		CloseHandle(m_hFile);
	}

	virtual void Read(uint32_t cbRead, _Out_writes_bytes_(cbRead) byte* pBuffer) override
	{
		DWORD cbBytesRead = 0;
		BOOL succeeded = ReadFile(m_hFile, pBuffer, cbRead, &cbBytesRead, nullptr);

		if (!succeeded || cbBytesRead != cbRead)
			throw std::runtime_error("File Read fatal error");
	}

	bool IsValid() const
	{
		return (m_hFile != INVALID_HANDLE_VALUE);
	}

private:
	std::wstring m_fileName;
	HANDLE m_hFile;
};



// CAboutDlg dialog used for App About

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// Dialog Data
	enum { IDD = IDD_ABOUTBOX };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CWinSaltyNESDlg dialog



CWinSaltyNESDlg::CWinSaltyNESDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(CWinSaltyNESDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CWinSaltyNESDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CWinSaltyNESDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_ERASEBKGND()
	ON_WM_TIMER()
	ON_MESSAGE(WM_RENDERFRAME, RenderNextFrame)
	ON_COMMAND(ID_FILE_OPENROM, &CWinSaltyNESDlg::OnFileOpenRom)
	ON_COMMAND(ID_NES_SOUND, &CWinSaltyNESDlg::OnNesSound)
	ON_COMMAND(ID_NES_PLAYPAUSE, &CWinSaltyNESDlg::OnNesPlayPause)
	ON_COMMAND(ID_DEBUG_DEBUGRENDERING, &CWinSaltyNESDlg::OnDebugDebugrendering)
	ON_COMMAND(ID_NES_RESET, &CWinSaltyNESDlg::OnNesReset)
	ON_COMMAND(ID_FILE_EXIT, &CWinSaltyNESDlg::OnFileExit)
END_MESSAGE_MAP()


// CWinSaltyNESDlg message handlers

BOOL CWinSaltyNESDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon
	//OpenRomFile(L"C:\\Users\\gelias\\OneDrive\\Documents\\NES_Rom_Backups\\LegendOfZelda.nes");
	//OpenRomFile(L"C:\\Users\\Galen\\OneDrive\\Documents\\NES_Rom_Backups\\LegendOfZelda.nes");
	SetupRenderBitmap();

	// Set controls initial states
	SetDlgItemTextW(IDC_EDIT_WAVE_HZ, L"200");

	m_d3dRenderer.Initialize(GetSafeHwnd());
	//SetTimer(TIMER_TESTRENDER, 0, nullptr);

	return TRUE;  // return TRUE  unless you set the focus to a control
}

std::wstring CWinSaltyNESDlg::PickRomFile()
{
	CComPtr<IFileDialog> spFileDialog;
	CComPtr<IShellItem> spItemResult;

	HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&spFileDialog));

	if (FAILED(hr))
		return std::wstring();

	if (FAILED(hr = spFileDialog->Show(NULL)))
		return std::wstring();

	if (FAILED(hr = spFileDialog->GetResult(&spItemResult)))
		return std::wstring();

	PWSTR pszFilePath = nullptr;
	spItemResult->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
	std::wstring result = pszFilePath;
	CoTaskMemFree(pszFilePath);

	return result;
}

bool CWinSaltyNESDlg::OpenRomFile(LPCWSTR pwzRomFile)
{
	CWin32ReadOnlyFile romFile(pwzRomFile);

	m_isRomLoaded = false;
	if (!romFile.IsValid())
		return false;

	try
	{
		m_nes.LoadRomFile(&romFile);
		m_nes.Reset();
		m_isRomLoaded = true;
	}
	catch (NES::unsupported_mapper& mapperException)
	{
		char errorString[256];
		sprintf_s(errorString, "Unsupported mapper: %d", mapperException.GetMapperNumber());
		MessageBoxA(m_hWnd, errorString, "Error loading ROM", MB_OK);
		return false;
	}
	catch (std::exception& e)
	{
		char errorString[256];
		sprintf_s(errorString, "Unexpected error loading ROM: %s", e.what());
		MessageBoxA(m_hWnd, errorString, "Error loading ROM", MB_OK);
		return false;
	}

	if (m_loggingEnabled)
		StartLogging();

	return true;
}

void CWinSaltyNESDlg::StartLogging()
{
	PWSTR pwzLocalAppDataPath = nullptr;
	SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &pwzLocalAppDataPath);

	std::wstring logFileDirectory = pwzLocalAppDataPath;
	CoTaskMemFree(pwzLocalAppDataPath);
	logFileDirectory += L"\\SaltyNES";

	CreateDirectoryW(logFileDirectory.c_str(), nullptr);

	std::wstring logFilePath = logFileDirectory + L"\\Cpu.log";

	if (m_debugFileOutput)
		m_debugFileOutput.close();

	m_debugFileOutput.open(logFilePath.c_str());
}


void CWinSaltyNESDlg::SetupRenderBitmap()
{
	CClientDC clientDC(this);
	m_nesRenderBitmap.CreateCompatibleBitmap(&clientDC, PPU::c_displayWidth, PPU::c_displayHeight);

	// Setup bitmap render info
	memset(&m_nesRenderBitmapInfo.bmiHeader, 0, sizeof(m_nesRenderBitmapInfo.bmiHeader));
	m_nesRenderBitmapInfo.bmiHeader.biSize = sizeof(m_nesRenderBitmapInfo.bmiHeader);
	m_nesRenderBitmapInfo.bmiHeader.biWidth = PPU::c_displayWidth;
	m_nesRenderBitmapInfo.bmiHeader.biHeight = -PPU::c_displayHeight;;
	m_nesRenderBitmapInfo.bmiHeader.biPlanes = 1;
	m_nesRenderBitmapInfo.bmiHeader.biBitCount = 32;
	m_nesRenderBitmapInfo.bmiHeader.biCompression = BI_RGB;
	memset(&m_nesRenderBitmapInfo.bmiColors, 0, sizeof(RGBQUAD));

	DWORD bitmapData[PPU::c_displayWidth * PPU::c_displayHeight];
	memset(bitmapData, 0, sizeof(bitmapData));

	::SetDIBits(clientDC.GetSafeHdc(), (HBITMAP)m_nesRenderBitmap.GetSafeHandle(), 0, PPU::c_displayHeight, bitmapData, &m_nesRenderBitmapInfo, DIB_RGB_COLORS);
}

void CWinSaltyNESDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CWinSaltyNESDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CWinSaltyNESDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}


void CWinSaltyNESDlg::PaintNESFrame(CDC* pDC)
{
	::SetDIBits(pDC->GetSafeHdc(), (HBITMAP)m_nesRenderBitmap.GetSafeHandle(), 0, PPU::c_displayHeight, m_nes.GetPpu().GetDisplayBuffer(), &m_nesRenderBitmapInfo, DIB_RGB_COLORS);

	CDC memDC;
	memDC.CreateCompatibleDC(pDC);
	CBitmap* pOldBitmap = memDC.SelectObject(&m_nesRenderBitmap);
	pDC->StretchBlt(0, 0, 2 * PPU::c_displayWidth, 2 * PPU::c_displayHeight, &memDC, 0, 0, PPU::c_displayWidth, PPU::c_displayHeight, SRCCOPY);
	memDC.SelectObject(pOldBitmap);
}


void CWinSaltyNESDlg::UpdateRuntimeStats()
{
	if (m_loggingEnabled)
		m_debugFileOutput.flush();

	//auto cyclesString = std::to_wstring(m_nes.GetCyclesRanSoFar());
	//SetDlgItemTextW(IDC_CYCLES_DISPLAY, cyclesString.c_str());

	wchar_t wzProgramCounter[32];
	swprintf_s(wzProgramCounter, _countof(wzProgramCounter),L"%04hX", m_nes.GetCpu().GetProgramCounter());
	SetDlgItemTextW(IDC_PROGRAM_COUNTER, wzProgramCounter);

}


void CALLBACK FrameTimerProc(PVOID lpParameter, BOOLEAN /*timerOrWaitFired*/)
{
	CWinSaltyNESDlg* pDlg = (CWinSaltyNESDlg*)lpParameter;
	pDlg->DoFrameTimerProc();
}

void CWinSaltyNESDlg::DoFrameTimerProc()
{
	RenderFrame();
}

void CWinSaltyNESDlg::StopTimer()
{
	UpdateRuntimeStats();
	m_runMode = NESRunMode::Paused;
	KillTimer(TIMER_REDRAW);
	//DeleteTimerQueueTimer(NULL, m_frameTimer, NULL);
}


void CWinSaltyNESDlg::RenderFrame()
{
	if (!m_xinputController1.Poll())
		SetDlgItemTextW(IDC_CONTROLLER_OUTPUT, L"No controller detected");

	PushAggregateControllerState({m_xinputController1, m_keyboardController}, m_nes.UseController1());


	try
	{
		for (;;)
		{
			if (m_loggingEnabled)
			{
				const char* pszDebugString = m_nes.GetCpu().GetDebugState();
				m_debugFileOutput.write(pszDebugString, strlen(pszDebugString));
			}

			m_nes.RunCycle();

			if (m_nes.GetPpu().ShouldRender())
			{
				m_nes.GetApu().PushAudio();

				if (m_eRenderMode == ERenderMode::DirectX)
				{
					m_d3dRenderer.Render(m_nes.GetPpu().GetDisplayBuffer());
				}
				else
				{
					CClientDC clientDC(this);
					PaintNESFrame(&clientDC);
				}

				break;
			}
		}
	}
	catch (std::exception& e)
	{
		char errorString[256];
		sprintf_s(errorString, "Exception encountered: %s", e.what());

		if (m_loggingEnabled)
		{
			m_debugFileOutput.write(errorString, strlen(errorString));
		}

		StopTimer();

		MessageBoxA(m_hWnd, errorString, "Error!", MB_OK);

	}
	
	IncrementFrameCount(true /*shouldUpdateFpsCounter*/);

}

void CWinSaltyNESDlg::IncrementFrameCount(bool shouldUpdateFpsCounter)
{
	Duration frameTime = m_frameStopwatch.Lap();
	int averageFrameTime = (int)m_fpsAverage.AddValue(frameTime.GetMilliseconds());

	static int s_counter = 0;

	if (shouldUpdateFpsCounter && s_counter++ % 4 == 0)
	{
		if (averageFrameTime == 0)
			averageFrameTime = 1;

		WCHAR wzAverageFps[128];
		swprintf_s(wzAverageFps, _countof(wzAverageFps), L"%d", 1000 / averageFrameTime);
		SetDlgItemTextW(IDC_STATUSEDIT, wzAverageFps);
	}
}



BOOL CWinSaltyNESDlg::OnEraseBkgnd(CDC* pDC)
{
	return CDialogEx::OnEraseBkgnd(pDC);
}


void CWinSaltyNESDlg::OnTimer(UINT_PTR nIDEvent)
{
	switch (nIDEvent)
	{
		case TIMER_REDRAW:
			RenderFrame();
			break;
		case TIMER_TESTRENDER:
			TestRender();
			break;
	}

	CDialogEx::OnTimer(nIDEvent);
}


BOOL CWinSaltyNESDlg::PreTranslateMessage(MSG* pMsg)
{
	return m_keyboardController.TranlateWindowsMessage(pMsg->message, pMsg->wParam);
}


// https://social.msdn.microsoft.com/Forums/en-US/2a2a4106-5b00-4763-b08c-99f7a45ba552/forcing-message-processing?forum=vcmfcatl
void GiveTime()
{
	// Idle until the screen redraws itself, et. al.
	MSG msg;
	while (::PeekMessage( &msg, NULL, 0, 0, PM_NOREMOVE ) ) { 
		if (!AfxGetThread()->PumpMessage( )) { 
			::PostQuitMessage(0); 
			break; 
		} 
	} 
	// let MFC do its idle processing
	LONG lIdle = 0;
	while (AfxGetApp()->OnIdle(lIdle++ ))
		;
}


afx_msg LRESULT CWinSaltyNESDlg::RenderNextFrame(WPARAM /*wParam*/, LPARAM /*lParam*/)
{
	RenderFrame();
	GiveTime();	
	PostMessage(WM_RENDERFRAME);

	return 0;
}


void CWinSaltyNESDlg::OnFileOpenRom()
{
	std::wstring romFileName = PickRomFile();

	if (!romFileName.empty())
	{
		if (OpenRomFile(romFileName.c_str()))
			PlayRom();
	}
}


void CWinSaltyNESDlg::OnNesSound()
{
	m_isSoundEnabled = !m_isSoundEnabled;
	m_nes.GetApu().EnableSound(m_isSoundEnabled);

	CMenu *pMenu = GetMenu();
	if (pMenu != nullptr)
	{
		pMenu->CheckMenuItem(ID_NES_SOUND, MF_BYCOMMAND | (m_isSoundEnabled ? MF_CHECKED : MF_UNCHECKED));
	}
}


void CWinSaltyNESDlg::PlayRom()
{
	m_runMode = NESRunMode::Continuous;
	m_frameStopwatch.Start();
	//CreateTimerQueueTimer(&m_frameTimer, NULL /*default timer queue*/, FrameTimerProc, this, 1000/60, 1000/60, WT_EXECUTEINTIMERTHREAD /*flags*/);

	if (m_eRunMode == ERunMode::Timer)
		SetTimer(TIMER_REDRAW, 0, nullptr);
	else // if (m_eRunMode == ERunMode::FullThrottle)
		RenderNextFrame(0,0); // Run as fast as humanly possible for performance measurement reasons
}

void CWinSaltyNESDlg::PauseRom()
{
	StopTimer();
}


void CWinSaltyNESDlg::OnNesPlayPause()
{
	if (m_runMode != NESRunMode::Continuous && m_isRomLoaded)
	{
		PlayRom();
	}
	else
	{
		PauseRom();
	}
}


void CWinSaltyNESDlg::OnDebugDebugrendering()
{
	m_isDebugRenderingEnabled = !m_isDebugRenderingEnabled;

	CMenu *pMenu = GetMenu();
	if (pMenu != nullptr)
	{
		pMenu->CheckMenuItem(ID_DEBUG_DEBUGRENDERING, MF_BYCOMMAND | (m_isDebugRenderingEnabled ? MF_CHECKED : MF_UNCHECKED));
	}

	m_renderOptions.fDrawBackgroundGrid = m_isDebugRenderingEnabled;
	m_renderOptions.fDrawSpriteOutline = m_isDebugRenderingEnabled;
	m_nes.GetPpu().SetRenderOptions(m_renderOptions);
}


void CWinSaltyNESDlg::OnNesReset()
{
	m_nes.Reset();
}


void CWinSaltyNESDlg::OnFileExit()
{
	this->OnOK();
}


void CWinSaltyNESDlg::TestRender()
{
	m_d3dRenderer.Render(m_nes.GetPpu().GetDisplayBuffer());
}
