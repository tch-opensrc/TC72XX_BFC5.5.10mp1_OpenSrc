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
// appsettings.cpp :
//
//===========================================================================
//#####DESCRIPTIONBEGIN####
//
// Author(s):   julians
// Contact(s):  julians
// Date:        2000/09/11
// Version:     $Id: reposdlg.h,v 1.1 2001/03/16 18:21:09 julians Exp $
// Purpose:
// Description: Header file for ecRepositoryInfoDialog
// Requires:
// Provides:
// See also:
// Known bugs:
// Usage:
//
//####DESCRIPTIONEND####
//
//===========================================================================

#ifndef _ECOS_REPOSDLG_H_
#define _ECOS_REPOSDLG_H_

#ifdef __GNUG__
    #pragma interface "reposdlg.cpp"
#endif

//----------------------------------------------------------------------------
// ecRepositoryInfoDialog
//----------------------------------------------------------------------------

class ecRepositoryInfoDialog: public wxDialog
{
public:
    // constructors and destructors
    ecRepositoryInfoDialog( wxWindow *parent, wxWindowID id, const wxString &title,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = wxDEFAULT_DIALOG_STYLE );

//// Operations    
    bool AddControls(wxWindow* parent);
    bool CreateHtmlInfo(wxString& info);

private:
    DECLARE_EVENT_TABLE()
};

#endif
    // _ECOS_REPOSDLG_H_
