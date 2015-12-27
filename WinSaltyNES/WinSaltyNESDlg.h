
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

	static std::wstring PickRomFile();
	bool OpenRomFile(LPCWSTR pwzRomFile);
	void StartLogging();

	void RunCycles(int nCycles, bool runInfinitely);

	HICON m_hIcon;

	CFont m_logBoxFont;

	CBitmap m_nesRenderBitmap;
	BITMAPINFO m_nesRenderBitmapInfo;

	bool m_loggingEnabled = false;
	std::ofstream m_debugFileOutput;

	void SetupRenderBitmap();


	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()

private:
	void RenderFrame();
	void IncrementFrameCount(bool shouldUpdateFpsCounter);
	void PaintNESFrame(CDC* pDC);

	void PlayRandomAudio(int hz);
	void PlayRandomAudio();

	NES::NES m_nes;
	enum class NESRunMode
	{
		StepSingle,
		Continuous,
		Paused,
	};

	NESRunMode m_runMode = NESRunMode::Paused;

	PPU::RenderOptions m_renderOptions;

	MovingAverage<LONGLONG, 30> m_fpsAverage;
	Stopwatch m_frameStopwatch;

	IXAudio2* m_pXAudio = nullptr;
	IXAudio2SourceVoice* m_pSourceVoice = nullptr;
	int m_buffersInUse = 0;

public:
	afx_msg void OnBnClickedOpenRom();
	afx_msg void OnBnClickedRunCycles();
	afx_msg void OnBnClickedRunInfinite();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnBnClickedPlayMusic();
	afx_msg void OnBnClickedDebugRendering();
	afx_msg void OnBnClickedEnablesound();
	afx_msg void OnBnClickedStopMusic();
};
