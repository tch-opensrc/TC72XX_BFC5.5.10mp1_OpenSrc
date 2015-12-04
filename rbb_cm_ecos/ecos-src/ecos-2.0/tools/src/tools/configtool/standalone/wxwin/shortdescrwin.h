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
// shortdescrwin.h :
//
//===========================================================================
//#####DESCRIPTIONBEGIN####
//
// Author(s):   julians
// Contact(s):  julians
// Date:        2000/10/2
// Version:     $Id: shortdescrwin.h,v 1.1 2001/02/08 18:06:52 julians Exp $
// Purpose:
// Description: Header file for ecShortDescriptionWindow
// Requires:
// Provides:
// See also:
// Known bugs:
// Usage:
//
//####DESCRIPTIONEND####
//
//===========================================================================

#ifndef _ECOS_SHORTDESCRWIN_H_
#define _ECOS_SHORTDESCRWIN_H_

#ifdef __GNUG__
#pragma interface "shortdescrwin.h"
#endif

#include "wx/textctrl.h"

/*
 * ecShortDescriptionWindow
 *
 * Shows a description of the current option.
 */

class ecShortDescriptionWindow : public wxTextCtrl
{
public:
// Ctor(s)
    ecShortDescriptionWindow (wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style);
    ~ecShortDescriptionWindow ();

//// Event handlers

    void OnMouseEvent(wxMouseEvent& event);

//// Operations

//// Accessors
    wxMenu* GetPropertiesMenu() const { return m_propertiesMenu; }

protected:
    wxMenu*     m_propertiesMenu;

private:
    DECLARE_EVENT_TABLE()
    DECLARE_CLASS(ecShortDescriptionWindow )
};


#endif
        // _ECOS_SHORTDESCRWIN_H_
