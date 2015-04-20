
// WinSaltyNESDlg.h : header file
//

#pragma once

#include <sstream>
#include <fstream>

#include "NES.h"

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
	HICON m_hIcon;

	CFont m_logBoxFont;

	//std::wostringstream m_debugOutput;
	std::ofstream m_debugFileOutput;

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedButton1();


private:
	NES::NES m_nes;
};
