
// WinSaltyNESDlg.cpp : implementation file
//

#include "stdafx.h"
#include "WinSaltyNES.h"
#include "WinSaltyNESDlg.h"
#include "afxdialogex.h"

#include "NESRom.h"
#include "Ppu.h"
#include "Cpu6502.h"
#include "Stopwatch.h"

#include <Shlobj.h>
#include <stdexcept>

#include <xaudio2.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


//#define WMU_FRAME_PULSE ()
const DWORD WMU_FRAME_PULSE = WM_USER + 1;

const DWORD TIMER_REDRAW = 0;

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
	ON_BN_CLICKED(IDC_OPEN_ROM, &CWinSaltyNESDlg::OnBnClickedOpenRom)
	ON_BN_CLICKED(IDC_RUN_CYCLES, &CWinSaltyNESDlg::OnBnClickedRunCycles)
	ON_BN_CLICKED(IDC_RUN_INFINITE, &CWinSaltyNESDlg::OnBnClickedRunInfinite)
	ON_MESSAGE(WMU_FRAME_PULSE, &CWinSaltyNESDlg::OnFramePulse)
	ON_WM_ERASEBKGND()
	ON_WM_TIMER()
	ON_BN_CLICKED(IDC_PLAY_MUSIC, &CWinSaltyNESDlg::OnBnClickedPlayMusic)
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

	OpenRomFile(L"C:\\Games\\Emulation\\NES_Roms\\Donkey Kong Jr. (World) (Rev A).nes");
	SetupRenderBitmap();

	return TRUE;  // return TRUE  unless you set the focus to a control
}

std::wstring CWinSaltyNESDlg::PickRomFile()
{
	IFileDialog* pFileDialog = nullptr;
	//IFileDialogEvents* pFileDialogEvents = nullptr;

	HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pFileDialog));

	IShellItem* psiResult;

	if (FAILED(hr = pFileDialog->Show(NULL)))
		return std::wstring();

	hr = pFileDialog->GetResult(&psiResult);

	PWSTR pszFilePath = nullptr;
	psiResult->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);

	std::wstring result = pszFilePath;

	CoTaskMemFree(pszFilePath);

	return result;
}

void CWinSaltyNESDlg::OpenRomFile(LPCWSTR pwzRomFile)
{
	CWin32ReadOnlyFile romFile(pwzRomFile);

	if (!romFile.IsValid())
		return;

	m_nes.LoadRomFile(&romFile);
	m_nes.Reset();
	
	if (m_loggingEnabled)
		StartLoggiong();
}

void CWinSaltyNESDlg::StartLoggiong()
{
	PWSTR pwzLocalAppDataPath = nullptr;
	SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &pwzLocalAppDataPath);

	pwzLocalAppDataPath;
	std::wstring logFileDirectory = pwzLocalAppDataPath;
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

		//if (m_runMode == NESRunMode::Continuous)
		//{
		//	for (;;)
		//	{
		//		m_nes.RunCycle();

		//		if (m_nes.GetPpu().ShouldRender())
		//		{
		//			CPaintDC dcPaint(this);
		//			PaintNESFrame(&dcPaint);
		//			break;
		//		}
		//	}

		//	IncrementFrameCount();

		//	CDialogEx::OnPaint();
		//}
		//else
		//{
		//	CDialogEx::OnPaint();
		//}
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
	PPU::ppuDisplayBuffer_t screenPixels;

	m_nes.GetPpu().RenderToBuffer(screenPixels);

	::SetDIBits(pDC->GetSafeHdc(), (HBITMAP)m_nesRenderBitmap.GetSafeHandle(), 0, PPU::c_displayHeight, screenPixels, &m_nesRenderBitmapInfo, DIB_RGB_COLORS);

	CDC memDC;
	memDC.CreateCompatibleDC(pDC);
	CBitmap* pOldBitmap = memDC.SelectObject(&m_nesRenderBitmap);
	pDC->StretchBlt(0, 0, 2 * PPU::c_displayWidth, 2 * PPU::c_displayHeight, &memDC, 0, 0, PPU::c_displayWidth, PPU::c_displayHeight, SRCCOPY);
	memDC.SelectObject(pOldBitmap);
}


void CWinSaltyNESDlg::RunCycles(int nCycles, bool runInfinitely)
{
	Stopwatch stopwatch(true /*start*/);

	int nFrames = 0;

	for (int instructionsRun = 0; instructionsRun < nCycles; )
	{
		if (!runInfinitely)
			instructionsRun++;

		//std::string str = m_nes.GetCpu().GetDebugState() + "\n";
		//m_debugFileOutput.write(str.c_str(), str.size());

		m_nes.RunCycle();

		if (m_nes.GetPpu().ShouldRender())
		{
			CClientDC clientDC(this);
			nFrames++;
			PaintNESFrame(&clientDC);
		}
	}

	auto duration = stopwatch.Stop();
	WCHAR wzFramesStatus[1024];
	const int durationInMilliseconds = (int)(duration.GetMilliseconds());
	const int framesPerSecond = (int)(nFrames * 1000 / durationInMilliseconds);
	swprintf_s(wzFramesStatus, _countof(wzFramesStatus), L"Frames = %d\r\nDuration (msec) = %d\r\nFrames/s = %d\r\n", nFrames, durationInMilliseconds, framesPerSecond);

	SetDlgItemTextW(IDC_STATUSEDIT, wzFramesStatus);

	m_debugFileOutput.flush();

	auto cyclesString = std::to_wstring(m_nes.GetCyclesRanSoFar());
	SetDlgItemTextW(IDC_CYCLES_DISPLAY, cyclesString.c_str());

	wchar_t wzProgramCounter[32];
	swprintf_s(wzProgramCounter, _countof(wzProgramCounter),L"%04hX", m_nes.GetCpu().GetProgramCounter());
	SetDlgItemTextW(IDC_PROGRAM_COUNTER, wzProgramCounter);
}

void CWinSaltyNESDlg::PlayRandomAudio(int hz)
{
	HRESULT hr = S_OK;
	IXAudio2* pXAudio = nullptr;
	IXAudio2MasteringVoice * pMasteringVoice;
	IXAudio2SourceVoice * pSourceVoice;
	byte soundData[5 * 2 * 44100];
	CoInitializeEx(NULL, COINIT_MULTITHREADED);
	//create the engine
	if (FAILED(XAudio2Create(&pXAudio)))
		return;

	hr = pXAudio->CreateMasteringVoice(&pMasteringVoice);

	// Create a source voice
	WAVEFORMATEX waveformat;
	waveformat.wFormatTag = WAVE_FORMAT_PCM;
	waveformat.nChannels = 1;
	waveformat.nSamplesPerSec = 44100;
	waveformat.nAvgBytesPerSec = 44100 * 2;
	waveformat.nBlockAlign = 2;
	waveformat.wBitsPerSample = 16;
	waveformat.cbSize = 0;
	hr = pXAudio->CreateSourceVoice(&pSourceVoice, &waveformat);
	if (FAILED(hr))
		return;

	// Start the source voice
	hr = pSourceVoice->Start();
	if (FAILED(hr))
		return;

	int c_wavesPerSec = hz;
	int c_samplesPerWave = waveformat.nSamplesPerSec / c_wavesPerSec;

	// Fill the array with sound data
	for (int index = 0, second = 0; second < 5; second++)
	{
		for (int cycle = 0; cycle < c_wavesPerSec; cycle++)
		{
			for (int sample = 0; sample < c_samplesPerWave; sample++)
			{
				short value = sample < (c_samplesPerWave / 2) ? 32767 : -32768;
				soundData[index++] = value & 0xFF;
				soundData[index++] = (value >> 8) & 0xFF;
			}
		}
	}

	// Create a button to reference the byte array
	XAUDIO2_BUFFER buffer = { 0 };
	buffer.AudioBytes = 2 * 5 * 44100;
	buffer.pAudioData = soundData;
	buffer.Flags = XAUDIO2_END_OF_STREAM;
	buffer.PlayBegin = 0;
	buffer.PlayLength = 1 * 44100;
	// Submit the buffer
	hr = pSourceVoice->SubmitSourceBuffer(&buffer);
	if (FAILED(hr))
		return;
}

void CWinSaltyNESDlg::OnBnClickedOpenRom()
{
	std::wstring romFileName = PickRomFile();

	if (!romFileName.empty())
	{
		OpenRomFile(romFileName.c_str());
		OnBnClickedRunInfinite();
	}
}


void CWinSaltyNESDlg::OnBnClickedRunCycles()
{
	RunCycles(200000, false /*runInfinitely*/);
}


void CWinSaltyNESDlg::OnBnClickedRunInfinite()
{
	if (m_runMode != NESRunMode::Continuous)
	{
		m_runMode = NESRunMode::Continuous;
		m_frameStopwatch.Start();
		//PostMessage(WMU_FRAME_PULSE, 0, 0);
		SetTimer(TIMER_REDRAW, 0, nullptr);
	}
	else
	{
		m_runMode = NESRunMode::Paused;
		KillTimer(TIMER_REDRAW);
	}
}


LRESULT CWinSaltyNESDlg::OnFramePulse(WPARAM /*wParam*/, LPARAM /*lParam*/)
{
	RenderFrame();

	//if (m_runMode == NESRunMode::Continuous)
	//	PostMessage(WMU_FRAME_PULSE, 0, 0);

	return 0;
}

void CWinSaltyNESDlg::RenderFrame()
{
	for (;;)
	{
		if (m_loggingEnabled)
		{
			std::string str = m_nes.GetCpu().GetDebugState() + "\n";
			m_debugFileOutput.write(str.c_str(), str.size());
		}

		m_nes.RunCycle();

		if (m_nes.GetPpu().ShouldRender())
		{
			CClientDC clientDC(this);
			PaintNESFrame(&clientDC);
			break;
		}
	}
	
	IncrementFrameCount(true /*shouldUpdateFpsCounter*/);

}

void CWinSaltyNESDlg::IncrementFrameCount(bool shouldUpdateFpsCounter)
{
	Duration frameTime = m_frameStopwatch.Lap();
	int averageFrameTime = (int)m_fpsAverage.AddValue(frameTime.GetMilliseconds());

	if (shouldUpdateFpsCounter)
	{
		if (averageFrameTime == 0)
			averageFrameTime = 1;

		WCHAR wzAverageFps[128];
		swprintf_s(wzAverageFps, _countof(wzAverageFps), L"%d fps", 1000 / averageFrameTime);
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
	}

	CDialogEx::OnTimer(nIDEvent);
}


NES::ControllerInput MapVirtualKeyToNesInput(DWORD vkey)
{
	switch (vkey)
	{
	case VK_RETURN:
		return NES::ControllerInput::Start;
	case VK_SPACE:
		return NES::ControllerInput::Select;
	case 'z':
	case 'Z':
		return NES::ControllerInput::A;
	case 'x':
	case 'X':
		return NES::ControllerInput::B;
	case VK_DOWN:
		return NES::ControllerInput::Down;
	case VK_UP:
		return NES::ControllerInput::Up;
	case VK_LEFT:
		return NES::ControllerInput::Left;
	case VK_RIGHT:
		return NES::ControllerInput::Right;
	default:
		return NES::ControllerInput::_Max;
	}
}

BOOL CWinSaltyNESDlg::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN || pMsg->message == WM_KEYUP)
	{
		// Intercept key presses-
		NES::ControllerInput nesInput = MapVirtualKeyToNesInput(pMsg->wParam);
		if (nesInput != NES::ControllerInput::_Max)
		{
			m_nes.UseController1().SetInputStatus(nesInput, (pMsg->message == WM_KEYDOWN));
			return TRUE;
		}
	}
	return FALSE;
}




void CWinSaltyNESDlg::OnBnClickedPlayMusic()
{
	CStringW strHz;
	GetDlgItemTextW(IDC_EDIT_WAVE_HZ, strHz);

	int hz = wcstol((LPCWSTR)strHz, nullptr, 10);

	PlayRandomAudio(hz);
}
