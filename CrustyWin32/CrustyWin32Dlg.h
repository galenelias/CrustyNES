
// CrustyWin32Dlg.h : header file
//

#pragma once

#include "NES/NES.h"
#include "Util/MovingAverage.h"
#include "Util/Stopwatch.h"
#include "UserController.h"

#include "D3D11Renderer.h"

#include <sstream>
#include <fstream>
#include <xaudio2.h>

enum class ERenderMode
{
	Win32,
	DirectX,
};

enum class ERunMode
{
	Timer,
	FullThrottle,
};

// CCrustyWin32Dlg dialog
class CCrustyWin32Dlg : public CDialogEx
{
// Construction
public:
	CCrustyWin32Dlg(CWnd* pParent = NULL);	// standard constructor

	void DoFrameTimerProc();

// Dialog Data
	enum { IDD = IDD_CRUSTYWIN32_DIALOG };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support


// Implementation
protected:
	virtual BOOL PreTranslateMessage(MSG* pMsg);

	HICON m_hIcon;
	CFont m_logBoxFont;

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()

private:
	void SetupRenderBitmap();
	void RenderFrame();
	void PaintNESFrame(CDC* pDC);
	void StopTimer();
	void IncrementFrameCount(bool shouldUpdateFpsCounter);

	void UpdateRuntimeStats();

	static std::wstring PickRomFile();
	bool OpenRomFile(LPCWSTR pwzRomFile);
	void PlayRom();
	void PauseRom();

	void StartLogging();

	bool m_loggingEnabled = false;
	std::ofstream m_debugFileOutput;

	bool m_isRomLoaded = false;
	NES::NES m_nes;
	enum class NESRunMode
	{
		StepSingle,
		Continuous,
		Paused,
	};

	NESRunMode m_runMode = NESRunMode::Paused;

	CBitmap m_nesRenderBitmap;
	BITMAPINFO m_nesRenderBitmapInfo;
	PPU::RenderOptions m_renderOptions;
	bool m_isSoundEnabled = true;
	bool m_isDebugRenderingEnabled = false;

	Win32KeyboardController m_keyboardController;
	XInputGamepadController m_xinputController1;

	MovingAverage<LONGLONG, 30> m_fpsAverage;
	Stopwatch m_frameStopwatch;
	HANDLE m_frameTimer = INVALID_HANDLE_VALUE;

	D3D11Renderer m_d3dRenderer;
	void TestRender();

	ERenderMode m_eRenderMode = ERenderMode::DirectX;	// Win32, DirectX,
	ERunMode m_eRunMode = ERunMode::Timer; // Timer, FullThrottle,


public:
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnTimer(UINT_PTR nIDEvent);

	afx_msg LRESULT RenderNextFrame(WPARAM wParam, LPARAM lParam);
	afx_msg void OnFileOpenRom();
	afx_msg void OnNesSound();
	afx_msg void OnNesPlayPause();
	afx_msg void OnDebugDebugrendering();
	afx_msg void OnNesReset();
	afx_msg void OnFileExit();
	afx_msg void OnSize(UINT nType, int cx, int cy);
};
