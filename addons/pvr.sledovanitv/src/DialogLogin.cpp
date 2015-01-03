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

#include "DialogLogin.h"
#include "libXBMC_gui.h"

using namespace ADDON;

#define BUTTON_OK                       1
#define BUTTON_CANCEL                   2
#define BUTTON_CLOSE                   22

CDialogLogin::CDialogLogin(CHelper_libXBMC_addon* xbmc, CHelper_libXBMC_gui* gui, std::string username, std::string password)
{
	XBMC = xbmc;
	GUI = gui;
	Username = username;
	Password = password;

	// specify the xml of the window here
	_confirmed = -1;
	_window = GUI->Window_create("DialogLogin.xml", "Confluence", false, true);

	// needed for every dialog
	_window->m_cbhdl = this;
	_window->CBOnInit = OnInitCB;
	_window->CBOnFocus = OnFocusCB;
	_window->CBOnClick = OnClickCB;
	_window->CBOnAction = OnActionCB;
}

CDialogLogin::~CDialogLogin()
{
	GUI->Window_destroy(_window);
}

bool CDialogLogin::OnInit()
{
	_editUsername = GUI->Control_getEdit(_window, 8);
	_editPassword = GUI->Control_getEdit(_window, 9);

	if (_editUsername)
		_editUsername->SetLabel2(Username.c_str());
	if (_editPassword)
		_editPassword->SetLabel2(Password.c_str());

	return true;
}

bool CDialogLogin::OnClick(int controlId)
{
	switch (controlId)
	{
		case BUTTON_OK:				// save value from GUI, then FALLS THROUGH TO CANCEL
			Username = _editUsername ? _editUsername->GetLabel2() : "";
			Password = _editPassword ? _editPassword->GetLabel2() : "";
		case BUTTON_CANCEL:			// handle releasing of objects
		case BUTTON_CLOSE:
			if (_confirmed == -1)	// if not previously confirmed, set to cancel value
				_confirmed = 0;
			_window->Close();
			GUI->Control_releaseEdit(_editUsername);
			GUI->Control_releaseEdit(_editPassword);
			break;
	}

	return true;
}

bool CDialogLogin::OnInitCB(GUIHANDLE cbhdl)
{
	CDialogLogin* dialog = static_cast<CDialogLogin*>(cbhdl);
	return dialog->OnInit();
}

bool CDialogLogin::OnClickCB(GUIHANDLE cbhdl, int controlId)
{
	CDialogLogin* dialog = static_cast<CDialogLogin*>(cbhdl);
	if (controlId == BUTTON_OK)
		dialog->_confirmed = 1;
	return dialog->OnClick(controlId);
}

bool CDialogLogin::OnFocusCB(GUIHANDLE cbhdl, int controlId)
{
	CDialogLogin* dialog = static_cast<CDialogLogin*>(cbhdl);
	return dialog->OnFocus(controlId);
}

bool CDialogLogin::OnActionCB(GUIHANDLE cbhdl, int actionId)
{
	CDialogLogin* dialog = static_cast<CDialogLogin*>(cbhdl);
	return dialog->OnAction(actionId);
}

bool CDialogLogin::Show()
{
	if (_window)
		return _window->Show();

	return false;
}

void CDialogLogin::Close()
{
	if (_window)
		_window->Close();
}

int CDialogLogin::DoModal()
{
	if (_window)
		_window->DoModal();
	return _confirmed;		// return true if user didn't cancel dialog
}

bool CDialogLogin::OnFocus(int controlId)
{
	return true;
}

bool CDialogLogin::OnAction(int actionId)
{
	if (actionId == ADDON_ACTION_CLOSE_DIALOG || actionId == ADDON_ACTION_PREVIOUS_MENU || actionId == 92/*back*/)
		return OnClick(BUTTON_CANCEL);
	else
		return false;
}
