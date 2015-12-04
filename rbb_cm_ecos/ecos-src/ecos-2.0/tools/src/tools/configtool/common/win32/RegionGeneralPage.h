//####COPYRIGHTBEGIN####
//                                                                          
// ----------------------------------------------------------------------------
// Copyright (C) 1998, 1999, 2000 Red Hat, Inc.
//
// This program is part of the eCos host tools.
//
// This program is free software; you can redistribute it and/or modify it 
// under the terms of the GNU General Public License as published by the Free 
// Software Foundation; either version 2 of the License, or (at your option) 
// any later version.
// 
// This program is distributed in the hope that it will be useful, but WITHOUT 
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or 
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for 
// more details.
// 
// You should have received a copy of the GNU General Public License along with
// this program; if not, write to the Free Software Foundation, Inc., 
// 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// ----------------------------------------------------------------------------
//                                                                          
//####COPYRIGHTEND####
//=================================================================
//
//        RegionGeneralPage.h
//
//        Memory Layout Tool region general property page interface
//
//=================================================================
//=================================================================
//#####DESCRIPTIONBEGIN####
//
// Author(s):     John Dallaway
// Contact(s):    jld
// Date:          1998/07/29 $RcsDate$ {or whatever}
// Version:       0.00+  $RcsVersion$ {or whatever}
// Purpose:       Provides a derivation of the MFC CPropertyPage class for
//                general region property selection
// See also:      RegionGeneralPage.cpp
// Known bugs:    <UPDATE_ME_AT_RELEASE_TIME>
//
//####DESCRIPTIONEND####

#if !defined(AFX_REGIONGENERALPAGE_H__FA2F38F3_1FA8_11D2_BFBB_00A0C9554250__INCLUDED_)
#define AFX_REGIONGENERALPAGE_H__FA2F38F3_1FA8_11D2_BFBB_00A0C9554250__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// RegionGeneralPage.h : header file
//

#include "resource.h"
#include "eCosPropertyPage.h"

/////////////////////////////////////////////////////////////////////////////
// CRegionGeneralPage dialog

class CRegionGeneralPage : public CeCosPropertyPage
{
	DECLARE_DYNCREATE(CRegionGeneralPage)

// Construction
public:
	CRegionGeneralPage();
	~CRegionGeneralPage();
    DWORD m_dwRegionSize;
    DWORD m_dwRegionStartAddress;

// Dialog Data
	//{{AFX_DATA(CRegionGeneralPage)
	enum { IDD = IDD_REGION_GENERAL };
	CEdit	m_edtRegionSize;
	CEdit	m_edtRegionStartAddress;
	CEdit	m_edtRegionName;
	CString	m_strRegionName;
	BOOL	m_bRegionReadOnly;
	CString	m_strRegionStartAddress;
	CString	m_strRegionSize;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CRegionGeneralPage)
	public:
	virtual BOOL OnKillActive();
	virtual BOOL OnSetActive();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CRegionGeneralPage)
		// NOTE: the ClassWizard will add member functions here
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_REGIONGENERALPAGE_H__FA2F38F3_1FA8_11D2_BFBB_00A0C9554250__INCLUDED_)
