
// WinSaltyNESDlg.h : header file
//

#pragma once

#include <sstream>
#include <fstream>

#include "NES/NES.h"
#include "MovingAverage.h"
#include "Stopwatch.h"
#include <xaudio2.h>

// CWinSaltyNESDlg dialog
class CWinSaltyNESDlg : public CDialogEx
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

	virtual BOOL PreTranslateMessage(MSG* pMsg);

	static std::wstring PickRomFile();
	void OpenRomFile(LPCWSTR pwzRomFile);
	void StartLogging();

	void RunCycles(int nCycles, bool runInfinitely);

	HICON m_hIcon;

	CFont m_logBoxFont;

	CBitmap m_nesRenderBitmap;
	BITMAPINFO m_nesRenderBitmapInfo;

	//std::wostringstream m_debugOutput;
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

	NES::NES m_nes;
	enum class NESRunMode
	{
		StepSingle,
		Continuous,
		Paused,
	};

	NESRunMode m_runMode = NESRunMode::Paused;

	MovingAverage<LONGLONG, 30> m_fpsAverage;
	Stopwatch m_frameStopwatch;

	IXAudio2* m_pXAudio = nullptr;
	IXAudio2SourceVoice* m_pSourceVoice;

public:
	afx_msg void OnBnClickedButton1();
	afx_msg void OnBnClickedOpenRom();
	afx_msg void OnBnClickedRunCycles();
	afx_msg void OnBnClickedRunInfinite();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnBnClickedPlayMusic();
};
