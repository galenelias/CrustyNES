
// WinSaltyNESDlg.cpp : implementation file
//

#include "stdafx.h"
#include "WinSaltyNES.h"
#include "WinSaltyNESDlg.h"
#include "afxdialogex.h"

#include "NESRom.h"
#include "Ppu.h"
#include "Cpu6502.h"

#include <Shlobj.h>
#include <stdexcept>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


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

	virtual void Read(size_t cbRead, _Out_writes_bytes_(cbRead) byte* pBuffer) override
	{
		DWORD cbBytesRead = 0;
		BOOL succeeded = ReadFile(m_hFile, pBuffer, cbRead, &cbBytesRead, nullptr);

		if (!succeeded || cbBytesRead != cbRead)
			throw std::runtime_error("File Read fatal error");
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
	ON_BN_CLICKED(IDC_BUTTON1, &CWinSaltyNESDlg::OnBnClickedButton1)
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

	//m_logBoxFont.CreateFont(
	//	16,                        // nHeight
	//	0,                         // nWidth
	//	0,                         // nEscapement
	//	0,                         // nOrientation
	//	FW_NORMAL,                 // nWeight
	//	FALSE,                     // bItalic
	//	FALSE,                     // bUnderline
	//	0,                         // cStrikeOut
	//	ANSI_CHARSET,              // nCharSet
	//	OUT_DEFAULT_PRECIS,        // nOutPrecision
	//	CLIP_DEFAULT_PRECIS,       // nClipPrecision
	//	DEFAULT_QUALITY,           // nQuality
	//	DEFAULT_PITCH | FF_SWISS,  // nPitchAndFamily
	//	L"Courier New");                 // lpszFacename

	// TODO: Add extra initialization here
	//CWin32ReadOnlyFile romFile(_T("C:\\Games\\Emulation\\NES_Roms\\LegendOfZelda.nes"));

	//rom.LoadRomFromFile(&CWin32ReadOnlyFile(_T("C:\\Games\\Emulation\\NES_Roms\\Balloon Fight (USA).nes")));

	//CWin32ReadOnlyFile romFile(_T("C:\\Games\\Emulation\\NES_Roms\\Balloon Fight (USA).nes"));
	CWin32ReadOnlyFile romFile(_T("C:\\Games\\Emulation\\NES_Roms\\Joust (Japan).nes"));
	//CWin32ReadOnlyFile romFile(_T("C:\\Games\\Emulation\\NES_Roms\\nestest.nes"));

	m_nes.LoadRomFile(&romFile);
	m_nes.Reset();

	PWSTR pwzLocalAppDataPath = nullptr;
	SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &pwzLocalAppDataPath);

	pwzLocalAppDataPath;
	std::wstring logFileDirectory = pwzLocalAppDataPath;
	logFileDirectory += L"\\SaltyNES";

	CreateDirectoryW(logFileDirectory.c_str(), nullptr);

	std::wstring logFilePath = logFileDirectory + L"\\Cpu.log";

	m_debugFileOutput.open(logFilePath.c_str());

	//rom.LoadRomFromFile(&CWin32ReadOnlyFile(_T("C:\\Games\\Emulation\\NES_Roms\\Mike Tyson's Punch-Out!!.nes")));
	//rom.LoadRomFromFile(&CWin32ReadOnlyFile(_T("C:\\Games\\Emulation\\NES_Roms\\Dr. Mario.nes")));
	//rom.LoadRomFromFile(&CWin32ReadOnlyFile(_T("C:\\Games\\Emulation\\NES_Roms\\MegaMan.nes")));
	//rom.LoadRomFromFile(&CWin32ReadOnlyFile(_T("C:\\Games\\Emulation\\NES_Roms\\LegendOfZelda.nes")));
	//rom.LoadRomFromFile(&CWin32ReadOnlyFile(_T("C:\\Games\\Emulation\\NES_Roms\\BubbleBobble.nes")));
	//rom.LoadRomFromFile(&CWin32ReadOnlyFile(_T("C:\\Games\\Emulation\\NES_Roms\\Contra.nes")));

	return TRUE;  // return TRUE  unless you set the focus to a control
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



void CWinSaltyNESDlg::OnBnClickedButton1()
{
	for (int instructionsRun = 0;instructionsRun < 20000; instructionsRun++)
	{
		std::string str = m_nes.GetCpu().GetDebugState() + "\n";

		m_debugFileOutput.write(str.c_str(), str.size());

		m_nes.RunCycle();

		if (m_nes.GetPpu().ShouldRender())
		{
			RECT clientRect;
			GetClientRect(&clientRect);
			CClientDC dc(this);

			PPU::ppuDisplayBuffer_t screenPixels;

			m_nes.GetPpu().RenderToBuffer(screenPixels);

			for (int iRow = 0; iRow != PPU::c_displayHeight; ++iRow)
			{
				for (int iColumn=0; iColumn != PPU::c_displayWidth; ++iColumn)
				{
					const COLORREF color = screenPixels[iRow][iColumn];
					SetPixel(dc.GetSafeHdc(), iColumn*2 + 0, iRow*2 + 0, color);
					SetPixel(dc.GetSafeHdc(), iColumn*2 + 1, iRow*2 + 0, color);
					SetPixel(dc.GetSafeHdc(), iColumn*2 + 0, iRow*2 + 1, color);
					SetPixel(dc.GetSafeHdc(), iColumn*2 + 1, iRow*2 + 1, color);
				}
			}
		}
	}

	m_debugFileOutput.flush();

	auto cyclesString = std::to_wstring(m_nes.GetCyclesRanSoFar());
	SetDlgItemTextW(IDC_CYCLES_DISPLAY, cyclesString.c_str());

	wchar_t wzProgramCounter[32];
	swprintf_s(wzProgramCounter, _countof(wzProgramCounter),L"%04hX", m_nes.GetCpu().GetProgramCounter());
	SetDlgItemTextW(IDC_PROGRAM_COUNTER, wzProgramCounter);

}
