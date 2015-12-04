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
// choosereposdlg.h :
//
//===========================================================================
//#####DESCRIPTIONBEGIN####
//
// Author(s):   julians
// Contact(s):  julians
// Date:        2000/10/02
// Version:     $Id: choosereposdlg.h,v 1.2 2001/03/23 13:38:04 julians Exp $
// Purpose:
// Description: Header file for ecChooseRepositoryDialog
// Requires:
// Provides:
// See also:
// Known bugs:
// Usage:
//
//####DESCRIPTIONEND####
//
//===========================================================================

#ifndef _ECOS_CHOOSEREPOSDLG_H_
#define _ECOS_CHOOSEREPOSDLG_H_

#ifdef __GNUG__
#pragma interface "choosereposdlg.cpp"
#endif

#include "ecutils.h"

class ecChooseRepositoryDialog : public ecDialog
{
public:
// Ctor(s)
    ecChooseRepositoryDialog(wxWindow* parent);

//// Event handlers

    void OnBrowse(wxCommandEvent& event);
    void OnOK(wxCommandEvent& event);
    void OnCancel(wxCommandEvent& event);

//// Operations
    void CreateControls(wxWindow* parent);

    wxString GetFolder() ;

protected:

private:
    DECLARE_EVENT_TABLE()
};

#define ecID_CHOOSE_REPOSITORY_TEXT 10089
#define ecID_CHOOSE_REPOSITORY_BROWSE 10090

#endif
        // _ECOS_CHOOSEREPOSDLG_H_
