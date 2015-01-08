/*
 *      Copyright (C) 2005-2011 Team XBMC
 *      http://xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301  USA
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "DialogOK.h"
#include "libXBMC_gui.h"

using namespace ADDON;

#define BUTTON_OK                      10
#define BUTTON_CLOSE                   2

CDialogOk::CDialogOk(CHelper_libXBMC_addon* xbmc, CHelper_libXBMC_gui* gui, std::string message)
{
	XBMC = xbmc;
	GUI = gui;
	m_Message = message;
	
	// specify the xml of the window here
	_confirmed = -1;
	_window = GUI->Window_create("DialogOK.xml", "Confluence", false, true);

	// needed for every dialog
	_window->m_cbhdl = this;
	_window->CBOnInit = OnInitCB;
	_window->CBOnFocus = OnFocusCB;
	_window->CBOnClick = OnClickCB;
	_window->CBOnAction = OnActionCB;
}

CDialogOk::~CDialogOk()
{
	GUI->Window_destroy(_window);
}

bool CDialogOk::OnInit()
{
	_window->SetControlLabel(9, m_Message.c_str());

	return true;
}

bool CDialogOk::OnClick(int controlId)
{
	switch (controlId)
	{
		case BUTTON_OK:				// save value from GUI, then FALLS THROUGH TO CANCEL
		case BUTTON_CLOSE:
			if (_confirmed == -1)	// if not previously confirmed, set to cancel value
				_confirmed = 0;
			_window->Close();
			break;
	}

	return true;
}

bool CDialogOk::OnInitCB(GUIHANDLE cbhdl)
{
	CDialogOk* dialog = static_cast<CDialogOk*>(cbhdl);
	return dialog->OnInit();
}

bool CDialogOk::OnClickCB(GUIHANDLE cbhdl, int controlId)
{
	CDialogOk* dialog = static_cast<CDialogOk*>(cbhdl);
	if (controlId == BUTTON_OK)
		dialog->_confirmed = 1;
	return dialog->OnClick(controlId);
}

bool CDialogOk::OnFocusCB(GUIHANDLE cbhdl, int controlId)
{
	CDialogOk* dialog = static_cast<CDialogOk*>(cbhdl);
	return dialog->OnFocus(controlId);
}

bool CDialogOk::OnActionCB(GUIHANDLE cbhdl, int actionId)
{
	CDialogOk* dialog = static_cast<CDialogOk*>(cbhdl);
	return dialog->OnAction(actionId);
}

bool CDialogOk::Show()
{
	if (_window)
		return _window->Show();

	return false;
}

void CDialogOk::Close()
{
	if (_window)
		_window->Close();
}

int CDialogOk::DoModal()
{
	if (_window)
		_window->DoModal();
	return _confirmed;		// return true if user didn't cancel dialog
}

bool CDialogOk::OnFocus(int controlId)
{
	return true;
}

bool CDialogOk::OnAction(int actionId)
{
	if (actionId == ADDON_ACTION_CLOSE_DIALOG || actionId == ADDON_ACTION_PREVIOUS_MENU || actionId == 92/*back*/)
		return OnClick(BUTTON_CLOSE);
	else
		return false;
}
