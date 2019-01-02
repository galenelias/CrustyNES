
// CrustyWin32.h : main header file for the PROJECT_NAME application
//

#pragma once

#ifndef __AFXWIN_H__
	#error "include 'stdafx.h' before including this file for PCH"
#endif

#include "resource.h"		// main symbols


// CCrustyWin32App:
// See CrustyWin32.cpp for the implementation of this class
//

class CCrustyWin32App : public CWinApp
{
public:
	CCrustyWin32App();

// Overrides
public:
	virtual BOOL InitInstance();

// Implementation

	DECLARE_MESSAGE_MAP()
};

extern CCrustyWin32App theApp;