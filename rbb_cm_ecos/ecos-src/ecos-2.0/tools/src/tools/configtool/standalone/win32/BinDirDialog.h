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
#if !defined(AFX_BINDIRDIALOG_H__EDC0D540_56E7_11D2_80CF_00A0C949ADAC__INCLUDED_)
#define AFX_BINDIRDIALOG_H__EDC0D540_56E7_11D2_80CF_00A0C949ADAC__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// BinDirDialog.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CBinDirDialog dialog
#include "FolderDialog.h"
#include "resource.h"

class CBinDirDialog : public CFolderDialog
{
// Construction
public:
	CBinDirDialog(const CStringArray &arstrPaths, const CString &strDefault);

// Dialog Data
	//{{AFX_DATA(CBinDirDialog)
	enum { IDD = IDD_TOOLCHAINFOLDER_DIALOG };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CBinDirDialog)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	const CString & m_strDefault;
	const CStringArray &m_arstrPaths;
	// Generated message map functions
	//{{AFX_MSG(CBinDirDialog)
	virtual BOOL OnInitDialog();
	afx_msg void OnEditchangeFolder();
	afx_msg void OnSelchangeFolder();
	afx_msg void OnBrowse();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_BINDIRDIALOG_H__EDC0D540_56E7_11D2_80CF_00A0C949ADAC__INCLUDED_)
