
// WinSaltyNESDlg.h : header file
//

#pragma once

#include <sstream>
#include <fstream>

#include "NES/NES.h"
#include "Util/MovingAverage.h"
#include "Util/Stopwatch.h"
#include <xaudio2.h>

// CWinSaltyNESDlg dialog
class CWinSaltyNESDlg : public CDialogEx, public IXAudio2VoiceCallback
{
// Construction
public:
	CWinSaltyNESDlg(CWnd* pParent = NULL);	// standard constructor

	void DoFrameTimerProc();

// Dialog Data
	enum { IDD = IDD_WINSALTYNES_DIALOG };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support


// Implementation
protected:
	// IXAudio2VoiceCallback methods.  We only use OnBufferEnd
	STDMETHOD_(void, OnVoiceProcessingPassStart) (THIS_ UINT32 /*BytesRequired*/) override {}
    STDMETHOD_(void, OnVoiceProcessingPassEnd) (THIS) override {}
    STDMETHOD_(void, OnStreamEnd) (THIS) override {}
    STDMETHOD_(void, OnBufferStart) (THIS_ void* /*pBufferContext*/) override {}
    STDMETHOD_(void, OnBufferEnd) (THIS_ void* pBufferContext) override;
    STDMETHOD_(void, OnLoopEnd) (THIS_ void* /*pBufferContext*/) override {}
    STDMETHOD_(void, OnVoiceError) (THIS_ void* /*pBufferContext*/, HRESULT /*Error*/) override {}

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
	void RenderFrame();
	void StopTimer();
	void IncrementFrameCount(bool shouldUpdateFpsCounter);

	void PaintNESFrame(CDC* pDC);
	void SetupRenderBitmap();

	void UpdateRuntimeStats();

	static std::wstring PickRomFile();
	bool OpenRomFile(LPCWSTR pwzRomFile);
	void StartLogging();

	void PlayRandomAudio(int hz);
	void PlayRandomAudio();

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

	PPU::RenderOptions m_renderOptions;
	CBitmap m_nesRenderBitmap;
	BITMAPINFO m_nesRenderBitmapInfo;

	MovingAverage<LONGLONG, 30> m_fpsAverage;
	Stopwatch m_frameStopwatch;
	HANDLE m_frameTimer = INVALID_HANDLE_VALUE;

	// Random sound test stuff
	IXAudio2* m_pXAudio = nullptr;
	IXAudio2SourceVoice* m_pSourceVoice = nullptr;
	int m_buffersInUse = 0;

public:
	afx_msg void OnBnClickedOpenRom();
	afx_msg void OnBnClickedRunInfinite();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnBnClickedPlayMusic();
	afx_msg void OnBnClickedDebugRendering();
	afx_msg void OnBnClickedEnablesound();
	afx_msg void OnBnClickedStopMusic();

	afx_msg LRESULT RenderNextFrame(WPARAM wParam, LPARAM lParam);
};
