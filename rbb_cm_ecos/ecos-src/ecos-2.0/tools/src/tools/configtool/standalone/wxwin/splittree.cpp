/////////////////////////////////////////////////////////////////////////////
// Name:        splittree.cpp
// Purpose:     Classes to achieve a remotely-scrolled tree in a splitter
//              window that can be scrolled by a scrolled window higher in the
//              hierarchy
// Author:      Julian Smart
// Modified by:
// Created:     8/7/2000
// RCS-ID:      $Id: splittree.cpp,v 1.9 2002/02/28 18:30:35 julians Exp $
// Copyright:   (c) Julian Smart
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
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------
#ifdef __GNUG__
#pragma implementation "splittree.h"
#endif

// For compilers that support precompilation, includes "wx/wx.h".
#include "ecpch.h"

#ifdef __WXMSW__
#include "wx/msw/private.h"
#endif

#ifdef __BORLANDC__
#pragma hdrstop
#endif

#include "wx/generic/treectlg.h"

#include "splittree.h"
#include <math.h>

/*
* wxRemotelyScrolledTreeCtrl
*/

#if USE_GENERIC_TREECTRL
IMPLEMENT_CLASS(wxRemotelyScrolledTreeCtrl, wxGenericTreeCtrl)
#else
IMPLEMENT_CLASS(wxRemotelyScrolledTreeCtrl, wxTreeCtrl)
#endif

#if USE_GENERIC_TREECTRL
BEGIN_EVENT_TABLE(wxRemotelyScrolledTreeCtrl, wxGenericTreeCtrl)
#else
BEGIN_EVENT_TABLE(wxRemotelyScrolledTreeCtrl, wxTreeCtrl)
#endif
EVT_SIZE(wxRemotelyScrolledTreeCtrl::OnSize)
EVT_TREE_ITEM_EXPANDED(-1, wxRemotelyScrolledTreeCtrl::OnExpand)
EVT_TREE_ITEM_COLLAPSED(-1, wxRemotelyScrolledTreeCtrl::OnExpand)
EVT_SCROLLWIN(wxRemotelyScrolledTreeCtrl::OnScroll)
END_EVENT_TABLE()

wxRemotelyScrolledTreeCtrl::wxRemotelyScrolledTreeCtrl(wxWindow* parent, wxWindowID id, const wxPoint& pt,
                                                       const wxSize& sz, long style):
    wxTreeCtrl(parent, id, pt, sz, style
#if wxVERSION_NUMBER > 2301
	       & ~wxTR_ROW_LINES
#endif
	       )
{
    m_companionWindow = NULL;
}

wxRemotelyScrolledTreeCtrl::~wxRemotelyScrolledTreeCtrl()
{
}

void wxRemotelyScrolledTreeCtrl::HideVScrollbar()
{
#ifdef __WXMSW__
    if (!IsKindOf(CLASSINFO(wxGenericTreeCtrl)))
    {
        ::ShowScrollBar((HWND) GetHWND(), SB_VERT, FALSE);
    }
    else
#endif
    {
        // Implicit in overriding SetScrollbars
    }
}

// Number of pixels per user unit (0 or -1 for no scrollbar)
// Length of virtual canvas in user units
// Length of page in user units
void wxRemotelyScrolledTreeCtrl::SetScrollbars(int pixelsPerUnitX, int pixelsPerUnitY,
                                               int noUnitsX, int noUnitsY,
                                               int xPos, int yPos,
                                               bool noRefresh)
{
    if (IsKindOf(CLASSINFO(wxGenericTreeCtrl)))
    {
        wxGenericTreeCtrl* win = (wxGenericTreeCtrl*) this;
        // Pass TRUE for noRefresh so that it doesn't
        // draw part of the tree as if the scroll view is
        // at zero vertically.
        win->wxGenericTreeCtrl::SetScrollbars(pixelsPerUnitX, pixelsPerUnitY, noUnitsX, 0, xPos, 0, /* noRefresh */ TRUE);
        
        ecScrolledWindow* scrolledWindow = GetScrolledWindow();
        if (scrolledWindow)
        {
            scrolledWindow->SetScrollbars(0, pixelsPerUnitY, 0, noUnitsY, 0, yPos, noRefresh);
        }
    }
}

// In case we're using the generic tree control.
int wxRemotelyScrolledTreeCtrl::GetScrollPos(int orient) const
{
    ecScrolledWindow* scrolledWindow = GetScrolledWindow();
    
    if (IsKindOf(CLASSINFO(wxGenericTreeCtrl)))
    {
        wxGenericTreeCtrl* win = (wxGenericTreeCtrl*) this;
        
        if (orient == wxHORIZONTAL)
            return win->wxGenericTreeCtrl::GetScrollPos(orient);
        else
        {
            return scrolledWindow->GetScrollPos(orient);
        }
    }
    return 0;
}


// In case we're using the generic tree control.
// Get the view start
void wxRemotelyScrolledTreeCtrl::GetViewStart(int *x, int *y) const
{
    ecScrolledWindow* scrolledWindow = GetScrolledWindow();
    
    if (IsKindOf(CLASSINFO(wxGenericTreeCtrl)))
    {
        
        wxGenericTreeCtrl* win = (wxGenericTreeCtrl*) this;
        int x1, y1, x2, y2;
        win->wxGenericTreeCtrl::GetViewStart(& x1, & y1);
        * x = x1; * y = y1;
        if (!scrolledWindow)
            return;
        
        scrolledWindow->GetViewStart(& x2, & y2);
        * y = y2;
    }
    else
    {
        // x is wrong since the horizontal scrollbar is controlled by the
        // tree control, but we probably don't need it.
        scrolledWindow->GetViewStart(x, y);
    }
}

// In case we're using the generic tree control.
void wxRemotelyScrolledTreeCtrl::PrepareDC(wxDC& dc)
{
    if (IsKindOf(CLASSINFO(wxGenericTreeCtrl)))
    {
        ecScrolledWindow* scrolledWindow = GetScrolledWindow();
        
        wxGenericTreeCtrl* win = (wxGenericTreeCtrl*) this;
        
        int startX, startY;
        GetViewStart(& startX, & startY);
        
        int xppu1, yppu1, xppu2, yppu2;
        win->wxGenericTreeCtrl::GetScrollPixelsPerUnit(& xppu1, & yppu1);
        scrolledWindow->GetScrollPixelsPerUnit(& xppu2, & yppu2);
        
        dc.SetDeviceOrigin( -startX * xppu1, -startY * yppu2 );
        // dc.SetUserScale( win->GetScaleX(), win->GetScaleY() );
    }
}

// Scroll to the given line (in scroll units where each unit is
// the height of an item)
void wxRemotelyScrolledTreeCtrl::ScrollToLine(int posHoriz, int posVert)
{
#ifdef __WXMSW__
    if (!IsKindOf(CLASSINFO(wxGenericTreeCtrl)))
    {
        UINT sbCode = SB_THUMBPOSITION;
        HWND vertScrollBar = 0;
        MSWDefWindowProc((WXUINT) WM_VSCROLL, MAKELONG(sbCode, posVert), (WXHWND) vertScrollBar);
    }
    else
#endif
    {
        wxGenericTreeCtrl* win = (wxGenericTreeCtrl*) this;
        win->Refresh();
        /* Doesn't work yet because scrolling is ignored by Scroll
        int xppu, yppu;
        wxScrolledWindow* scrolledWindow = GetScrolledWindow();
        if (scrolledWindow)
        {
        scrolledWindow->GetScrollPixelsPerUnit(& xppu, & yppu);
        win->Scroll(-1, posVert*yppu);
        }
        */
    }
}

void wxRemotelyScrolledTreeCtrl::OnSize(wxSizeEvent& event)
{
    HideVScrollbar();
    AdjustRemoteScrollbars();
    event.Skip();
}

void wxRemotelyScrolledTreeCtrl::OnExpand(wxTreeEvent& event)
{
    AdjustRemoteScrollbars();
    event.Skip();
    
    // If we don't have this, we get some bits of lines still remaining
    if (event.GetEventType() == wxEVT_COMMAND_TREE_ITEM_COLLAPSED)
        Refresh();
    
    // Pass on the event
    if (m_companionWindow)
        m_companionWindow->GetEventHandler()->ProcessEvent(event);
}

// Adjust the containing wxScrolledWindow's scrollbars appropriately
void wxRemotelyScrolledTreeCtrl::AdjustRemoteScrollbars()
{
    if (IsKindOf(CLASSINFO(wxGenericTreeCtrl)))
    {
        // This is for the generic tree control.
        // It calls SetScrollbars which has been overridden
        // to adjust the parent scrolled window vertical
        // scrollbar.
        ((wxGenericTreeCtrl*) this)->AdjustMyScrollbars();
        return;
    }
    else
    {
        // This is for the wxMSW tree control
        ecScrolledWindow* scrolledWindow = GetScrolledWindow();
        if (scrolledWindow)
        {
            wxRect itemRect;
            if (GetBoundingRect(GetRootItem(), itemRect))
            {
                // Actually, the real height seems to be 1 less than reported
                // (e.g. 16 instead of 16)
                int itemHeight = itemRect.GetHeight() - 1;
                
                int w, h;
                GetClientSize(&w, &h);
                
                wxRect rect(0, 0, 0, 0);
                CalcTreeSize(rect);
                
                double f = ((double) (rect.GetHeight()) / (double) itemHeight)  ;
                int treeViewHeight = (int) ceil(f);
                
                int scrollPixelsPerLine = itemHeight;
                int scrollPos = - (itemRect.y / itemHeight);
                
                scrolledWindow->SetScrollbars(0, scrollPixelsPerLine, 0, treeViewHeight, 0, scrollPos);
                
                // Ensure that when a scrollbar becomes hidden or visible,
                // the contained window sizes are right.
                // Problem: this is called too early (?)
                wxSizeEvent event(scrolledWindow->GetSize(), scrolledWindow->GetId());
                scrolledWindow->GetEventHandler()->ProcessEvent(event);
            }
        }
    }
}


// Calculate the area that contains both rectangles
static wxRect CombineRectangles(const wxRect& rect1, const wxRect& rect2)
{
    wxRect rect;
    
    int right1 = rect1.GetRight();
    int bottom1 = rect1.GetBottom();
    int right2 = rect2.GetRight();
    int bottom2 = rect2.GetBottom();
    
    wxPoint topLeft = wxPoint(wxMin(rect1.x, rect2.x), wxMin(rect1.y, rect2.y));
    wxPoint bottomRight = wxPoint(wxMax(right1, right2), wxMax(bottom1, bottom2));
    
    rect.x = topLeft.x; rect.y = topLeft.y;
    rect.SetRight(bottomRight.x);
    rect.SetBottom(bottomRight.y);
    
    return rect;
}


// Calculate the tree overall size so we can set the scrollbar
// correctly

// static int g_count = 0;

void wxRemotelyScrolledTreeCtrl::CalcTreeSize(wxRect& rect)
{
    //    g_count = 0;
    CalcTreeSize(GetRootItem(), rect);
}

void wxRemotelyScrolledTreeCtrl::CalcTreeSize(const wxTreeItemId& id, wxRect& rect)
{
    // More efficient implementation would be to find the last item (but how?)
    // Q: is the bounding rect relative to the top of the virtual tree workspace
    // or the top of the window? How would we convert?
    wxRect itemSize;
    if (GetBoundingRect(id, itemSize))
    {
        rect = CombineRectangles(rect, itemSize);
    }

    long cookie;
    wxTreeItemId childId = GetFirstChild(id, cookie);
    while (childId != 0)
    {
        CalcTreeSize(childId, rect);
        childId = GetNextChild(childId, cookie);
    }
}

// Find the scrolled window that contains this control
ecScrolledWindow* wxRemotelyScrolledTreeCtrl::GetScrolledWindow() const
{
    wxWindow* parent = wxWindow::GetParent();
    while (parent)
    {
        if (parent->IsKindOf(CLASSINFO(ecScrolledWindow)))
            return (ecScrolledWindow*) parent;
        parent = parent->GetParent();
    }
    return NULL;
}

void wxRemotelyScrolledTreeCtrl::OnScroll(wxScrollWinEvent& event)
{
    int orient = event.GetOrientation();
    if (orient == wxHORIZONTAL)
    {
        event.Skip();
        return;
    }
    ecScrolledWindow* scrollWin = GetScrolledWindow();
    if (!scrollWin)
        return;
    
    int x, y;
    scrollWin->GetViewStart(& x, & y);
    
    ScrollToLine(-1, y);
}

/*
* wxTreeCompanionWindow
*
* A window displaying values associated with tree control items.
*/

IMPLEMENT_CLASS(wxTreeCompanionWindow, wxWindow)

BEGIN_EVENT_TABLE(wxTreeCompanionWindow, wxWindow)
EVT_PAINT(wxTreeCompanionWindow::OnPaint)
EVT_SCROLLWIN(wxTreeCompanionWindow::OnScroll)
EVT_TREE_ITEM_EXPANDED(-1, wxTreeCompanionWindow::OnExpand)
EVT_TREE_ITEM_COLLAPSED(-1, wxTreeCompanionWindow::OnExpand)
END_EVENT_TABLE()

wxTreeCompanionWindow::wxTreeCompanionWindow(wxWindow* parent, wxWindowID id,
                                             const wxPoint& pos,
                                             const wxSize& sz,
                                             long style):
wxWindow(parent, id, pos, sz, style)
{
    m_treeCtrl = NULL;
}

void wxTreeCompanionWindow::DrawItem(wxDC& dc, wxTreeItemId id, const wxRect& rect)
{
    if (m_treeCtrl)
    {
        wxString text = m_treeCtrl->GetItemText(id);
        dc.SetTextForeground(* wxBLACK);
        dc.SetBackgroundMode(wxTRANSPARENT);
        
        int textW, textH;
        dc.GetTextExtent(text, & textW, & textH);
        
        int x = 5;
        int y = rect.GetY() + wxMax(0, (rect.GetHeight() - textH) / 2);
        
        dc.DrawText(text, x, y);
    }
}

void wxTreeCompanionWindow::OnPaint(wxPaintEvent& event)
{
    wxPaintDC dc(this);
    
    if (!m_treeCtrl)
        return;
    
    wxPen pen(wxSystemSettings::GetSystemColour(wxSYS_COLOUR_3DLIGHT), 1, wxSOLID);
    dc.SetPen(pen);
    dc.SetBrush(* wxTRANSPARENT_BRUSH);
    wxFont font(wxSystemSettings::GetSystemFont(wxSYS_DEFAULT_GUI_FONT));
    dc.SetFont(font);
    
    wxSize clientSize = GetClientSize();
    wxRect itemRect;
    int cy=0;
    wxTreeItemId h, lastH;
    for(h=m_treeCtrl->GetFirstVisibleItem();h;h=m_treeCtrl->GetNextVisible(h))
    {
        if (m_treeCtrl->GetBoundingRect(h, itemRect))
        {
            cy = itemRect.GetTop();
            wxRect drawItemRect(0, cy, clientSize.x, itemRect.GetHeight());
            
            lastH = h;
            
            // Draw the actual item
            DrawItem(dc, h, drawItemRect);
            dc.DrawLine(0, cy, clientSize.x, cy);
        }
    }
    if (lastH.IsOk() && m_treeCtrl->GetBoundingRect(lastH, itemRect))
    {
        cy = itemRect.GetBottom();
        dc.DrawLine(0, cy, clientSize.x, cy);
    }
}

void wxTreeCompanionWindow::OnScroll(wxScrollWinEvent& event)
{
    int orient = event.GetOrientation();
    if (orient == wxHORIZONTAL)
    {
        event.Skip();
        return;
    }
    if (!m_treeCtrl)
        return;
    
    // TODO: scroll the window physically instead of just refreshing.
    Refresh(TRUE);
}

void wxTreeCompanionWindow::OnExpand(wxTreeEvent& event)
{
    // TODO: something more optimized than simply refresh the whole
    // window when the tree is expanded/collapsed. Tricky.
    Refresh();
}

/*
* wxThinSplitterWindow
*/

IMPLEMENT_CLASS(wxThinSplitterWindow, wxSplitterWindow)

BEGIN_EVENT_TABLE(wxThinSplitterWindow, wxSplitterWindow)
EVT_SIZE(wxThinSplitterWindow::OnSize)
// Not in older versions of wxWindows, unfortunately
#if 0
EVT_SPLITTER_DOUBLECLICKED(-1, wxThinSplitterWindow::OnDoubleClickSash)
#endif
END_EVENT_TABLE()

wxThinSplitterWindow::wxThinSplitterWindow(wxWindow* parent, wxWindowID id,
                                           const wxPoint& pos,
                                           const wxSize& sz,
                                           long style):
wxSplitterWindow(parent, id, pos, sz, style)
{
}

void wxThinSplitterWindow::SizeWindows()
{
    // The client size may have changed inbetween
    // the sizing of the first window and the sizing of
    // the second. So repeat SizeWindows.
    wxSplitterWindow::SizeWindows();
    wxSplitterWindow::SizeWindows();
}

// Tests for x, y over sash
bool wxThinSplitterWindow::SashHitTest(int x, int y, int tolerance)
{
    return wxSplitterWindow::SashHitTest(x, y, 4);
}

void wxThinSplitterWindow::DrawSash(wxDC& dc)
{
    if ( m_sashPosition == 0 || !m_windowTwo)
        return;
    if (GetWindowStyle() & wxSP_NOSASH)
        return;
    
    int w, h;
    GetClientSize(&w, &h);
    
    if ( m_splitMode == wxSPLIT_VERTICAL )
    {
        dc.SetPen(* m_facePen);
        dc.SetBrush(* m_faceBrush);
        int h1 = h-1;
        int y1 = 0;
        if ( (GetWindowStyleFlag() & wxSP_BORDER) != wxSP_BORDER && (GetWindowStyleFlag() & wxSP_3DBORDER) != wxSP_3DBORDER )
            h1 += 1; // Not sure why this is necessary...
        if ( (GetWindowStyleFlag() & wxSP_3DBORDER) == wxSP_3DBORDER)
        {
            y1 = 2; h1 -= 3;
        }
        dc.DrawRectangle(m_sashPosition, y1, m_sashSize, h1);
    }
    else
    {
        dc.SetPen(* m_facePen);
        dc.SetBrush(* m_faceBrush);
        int w1 = w-1;
        int x1 = 0;
        if ( (GetWindowStyleFlag() & wxSP_BORDER) != wxSP_BORDER && (GetWindowStyleFlag() & wxSP_3DBORDER) != wxSP_3DBORDER )
            w1 ++;
        if ( (GetWindowStyleFlag() & wxSP_3DBORDER) == wxSP_3DBORDER)
        {
            x1 = 2; w1 -= 3;
        }
        dc.DrawRectangle(x1, m_sashPosition, w1, m_sashSize);
    }
    
    dc.SetPen(wxNullPen);
    dc.SetBrush(wxNullBrush);
}

void wxThinSplitterWindow::OnSize(wxSizeEvent& event)
{
    wxSplitterWindow::OnSize(event);
}

/*
* wxSplitterScrolledWindow
*/

IMPLEMENT_CLASS(wxSplitterScrolledWindow, ecScrolledWindow)

BEGIN_EVENT_TABLE(wxSplitterScrolledWindow, ecScrolledWindow)
EVT_SCROLLWIN(wxSplitterScrolledWindow::OnScroll)
EVT_SIZE(wxSplitterScrolledWindow::OnSize)
END_EVENT_TABLE()

wxSplitterScrolledWindow::wxSplitterScrolledWindow(wxWindow* parent, wxWindowID id,
                                                   const wxPoint& pos,
                                                   const wxSize& sz,
                                                   long style):
  ecScrolledWindow(parent, id, pos, sz, style)
{
}

void wxSplitterScrolledWindow::OnSize(wxSizeEvent& event)
{
    wxSize sz = GetClientSize();
    if (GetChildren().First())
    {
        ((wxWindow*) GetChildren().First()->Data())->SetSize(0, 0, sz.x, sz.y);
    }
}

void wxSplitterScrolledWindow::OnScroll(wxScrollWinEvent& event)
{
    // Ensure that events being propagated back up the window hierarchy
    // don't cause an infinite loop
    static bool inOnScroll = FALSE;
    if (inOnScroll)
    {
        event.Skip();
        return;
    }
    inOnScroll = TRUE;
    
    int orient = event.GetOrientation();
    
    int nScrollInc = CalcScrollInc(event);
    if (nScrollInc == 0)
    {
        inOnScroll = FALSE;
        return;
    }
    
    if (orient == wxHORIZONTAL)
    {
        inOnScroll = FALSE;
        event.Skip();
        return;
#if 0
        int newPos = m_xScrollPosition + nScrollInc;
        SetScrollPos(wxHORIZONTAL, newPos, TRUE );
#endif
    }
    else
    {
        int newPos = m_yScrollPosition + nScrollInc;
        SetScrollPos(wxVERTICAL, newPos, TRUE );
    }
    
    if (orient == wxHORIZONTAL)
    {
        m_xScrollPosition += nScrollInc;
    }
    else
    {
        m_yScrollPosition += nScrollInc;
    }
    
    // Find targets in splitter window and send the event to them
    wxNode* node = GetChildren().First();
    while (node)
    {
        wxWindow* child = (wxWindow*) node->Data();
        if (child->IsKindOf(CLASSINFO(wxSplitterWindow)))
        {
            wxSplitterWindow* splitter = (wxSplitterWindow*) child;
            if (splitter->GetWindow1())
                splitter->GetWindow1()->ProcessEvent(event);
            if (splitter->GetWindow2())
                splitter->GetWindow2()->ProcessEvent(event);
            break;
        }
        node = node->Next();
    }
    
#ifdef __WXMAC__
    m_targetWindow->MacUpdateImmediately() ;
#endif
    
    inOnScroll = FALSE;
}

