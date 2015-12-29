
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

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define WM_RENDERFRAME (WM_APP + 1)

const DWORD TIMER_REDRAW = 0;
const DWORD TIMER_SOUNDREFRESH = 1;

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
	ON_BN_CLICKED(IDC_OPEN_ROM, &CWinSaltyNESDlg::OnBnClickedOpenRom)
	ON_BN_CLICKED(IDC_RUN_INFINITE, &CWinSaltyNESDlg::OnBnClickedRunInfinite)
	ON_WM_ERASEBKGND()
	ON_WM_TIMER()
	ON_BN_CLICKED(IDC_PLAY_MUSIC, &CWinSaltyNESDlg::OnBnClickedPlayMusic)
	ON_BN_CLICKED(IDC_DEBUG_RENDERING, &CWinSaltyNESDlg::OnBnClickedDebugRendering)
	ON_BN_CLICKED(IDC_ENABLESOUND, &CWinSaltyNESDlg::OnBnClickedEnablesound)
	ON_BN_CLICKED(IDC_STOP_MUSIC, &CWinSaltyNESDlg::OnBnClickedStopMusic)
	ON_MESSAGE(WM_RENDERFRAME, RenderNextFrame)
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
	OpenRomFile(L"C:\\Users\\Galen\\OneDrive\\Documents\\NES_Rom_Backups\\LegendOfZelda.nes");
	SetupRenderBitmap();

	// Set controls initial states
	CButton* pButton = static_cast<CButton*>(this->GetDlgItem(IDC_ENABLESOUND));
	pButton->SetCheck(TRUE);
	SetDlgItemTextW(IDC_EDIT_WAVE_HZ, L"200");


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

	auto cyclesString = std::to_wstring(m_nes.GetCyclesRanSoFar());
	SetDlgItemTextW(IDC_CYCLES_DISPLAY, cyclesString.c_str());

	wchar_t wzProgramCounter[32];
	swprintf_s(wzProgramCounter, _countof(wzProgramCounter),L"%04hX", m_nes.GetCpu().GetProgramCounter());
	SetDlgItemTextW(IDC_PROGRAM_COUNTER, wzProgramCounter);

}


void CWinSaltyNESDlg::PlayRandomAudio()
{
	CStringW strHz;
	GetDlgItemTextW(IDC_EDIT_WAVE_HZ, strHz);

	int hz = wcstol((LPCWSTR)strHz, nullptr, 10);
	PlayRandomAudio(hz);
}


void CWinSaltyNESDlg::PlayRandomAudio(int hz)
{
	HRESULT hr = S_OK;

	if (hz == 0)
		return;

	if (m_pXAudio == nullptr)
	{
		IXAudio2MasteringVoice * pMasteringVoice;
		CoInitializeEx(NULL, COINIT_MULTITHREADED);
		//create the engine
		if (FAILED(XAudio2Create(&m_pXAudio)))
			return;

		hr = m_pXAudio->CreateMasteringVoice(&pMasteringVoice);
		if (FAILED(hr))
			return;
	}

	const int c_samplesPerSecond = 44100;
	const int c_bytesPerSample = 2;

	if (m_pSourceVoice == nullptr)
	{
		// Create a source voice
		WAVEFORMATEX waveformat;
		waveformat.wFormatTag = WAVE_FORMAT_PCM;
		waveformat.nChannels = 1;
		waveformat.nSamplesPerSec = c_samplesPerSecond;
		waveformat.nAvgBytesPerSec = c_samplesPerSecond * c_bytesPerSample;
		waveformat.nBlockAlign = c_bytesPerSample;
		waveformat.wBitsPerSample = c_bytesPerSample * 8;
		waveformat.cbSize = 0;
		hr = m_pXAudio->CreateSourceVoice(&m_pSourceVoice, &waveformat, 0 /*flags*/, XAUDIO2_DEFAULT_FREQ_RATIO,
			this);
		if (FAILED(hr))
			return;

		m_pSourceVoice->SetVolume(0.1f);
	}

	const int c_samplesPerBuffer = 2;
	if (m_buffersInUse == c_samplesPerBuffer)
		return;

	// This should be redundant with our explicit buffersInUse tracking
	//XAUDIO2_VOICE_STATE voiceState;
	//m_pSourceVoice->GetState(&voiceState, XAUDIO2_VOICE_NOSAMPLESPLAYED);
	//if (voiceState.BuffersQueued > 1)
	//	return;

	// Rotate samples in a single static buffer
	const int c_bytesPerDeciSecond = c_samplesPerSecond / 10 * 2;
	static byte s_soundData[c_samplesPerBuffer * c_bytesPerDeciSecond];
	static int s_bufferSampleOffset = 0;
	byte* soundData = s_soundData + s_bufferSampleOffset * c_bytesPerDeciSecond;
	s_bufferSampleOffset++;
	s_bufferSampleOffset %= c_samplesPerBuffer;

	int c_wavesPerSec = hz;
	int c_wavesPerDeciSec = hz / 10;
	int c_samplesPerWave = 44100 / c_wavesPerSec;

	// Fill the array with sound data
	const int c_deciSeconds = 1;
	for (int index = 0, second = 0; second < c_deciSeconds; second++)
	{
		for (int cycle = 0; cycle < c_wavesPerDeciSec; cycle++)
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
	buffer.AudioBytes = c_deciSeconds * c_wavesPerDeciSec * c_samplesPerWave * c_bytesPerSample;
	buffer.pAudioData = soundData;
	buffer.Flags = XAUDIO2_END_OF_STREAM;

	// Submit the buffer
	++m_buffersInUse;
	hr = m_pSourceVoice->SubmitSourceBuffer(&buffer);
	if (FAILED(hr))
		return;
}

STDMETHODIMP_(void) CWinSaltyNESDlg::OnBufferEnd(void* /*pBufferContext*/)
{
	--m_buffersInUse;
}


void CWinSaltyNESDlg::OnBnClickedOpenRom()
{
	std::wstring romFileName = PickRomFile();

	if (!romFileName.empty())
	{
		if (OpenRomFile(romFileName.c_str()))
			OnBnClickedRunInfinite();
	}
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

void CWinSaltyNESDlg::OnBnClickedRunInfinite()
{
	if (m_runMode != NESRunMode::Continuous && m_isRomLoaded)
	{
		m_runMode = NESRunMode::Continuous;
		m_frameStopwatch.Start();
		SetTimer(TIMER_REDRAW, 0, nullptr);
		//CreateTimerQueueTimer(&m_frameTimer, NULL /*default timer queue*/, FrameTimerProc, this, 1000/60, 1000/60, WT_EXECUTEINTIMERTHREAD /*flags*/);

		//RenderNextFrame(0,0); // Run as fast as humanly possible for performance measurement reasons
	}
	else
	{
		StopTimer();
	}
}


void CWinSaltyNESDlg::RenderFrame()
{
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
				CClientDC clientDC(this);
				PaintNESFrame(&clientDC);
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
	case TIMER_SOUNDREFRESH:
		PlayRandomAudio();
		break;
	}

	CDialogEx::OnTimer(nIDEvent);
}


NES::ControllerInput MapVirtualKeyToNesInput(WPARAM vkey)
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
	PlayRandomAudio();
	SetTimer(TIMER_SOUNDREFRESH, 90, nullptr);

	if (m_pSourceVoice != nullptr)
	{
		HRESULT hr = m_pSourceVoice->Start();
		if (FAILED(hr))
			return;
	}

}


void CWinSaltyNESDlg::OnBnClickedStopMusic()
{
	// Stop the source voice
	if (m_pSourceVoice != nullptr)
	{
		HRESULT hr = m_pSourceVoice->Stop();
		if (FAILED(hr))
			return;
	}

	KillTimer(TIMER_SOUNDREFRESH);
}


void CWinSaltyNESDlg::OnBnClickedDebugRendering()
{
	bool fDebugRender = IsDlgButtonChecked(IDC_DEBUG_RENDERING) != 0;

	m_renderOptions.fDrawBackgroundGrid = fDebugRender;
	m_renderOptions.fDrawSpriteOutline = fDebugRender;

	m_nes.GetPpu().SetRenderOptions(m_renderOptions);
}


void CWinSaltyNESDlg::OnBnClickedEnablesound()
{
	bool isSoundEnabled = IsDlgButtonChecked(IDC_ENABLESOUND) != 0;
	m_nes.GetApu().EnableSound(isSoundEnabled);
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
