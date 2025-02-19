// This file is part of Notepad++ project
// Copyright (C)2021 Don HO <don.h@free.fr>

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// at your option any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include <npp-internal.h>
#pragma hdrstop

const int WS_TOOLBARSTYLE = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | TBSTYLE_TOOLTIPS |TBSTYLE_FLAT | CCS_TOP |
    BTNS_AUTOSIZE | CCS_NOPARENTALIGN | CCS_NORESIZE | CCS_NODIVIDER;

void ToolBar::initTheme(TiXmlDocument * toolIconsDocRoot)
{
	_toolIcons =  toolIconsDocRoot->FirstChild(TEXT("NotepadPlus"));
	if(_toolIcons) {
		_toolIcons = _toolIcons->FirstChild(TEXT("ToolBarIcons"));
		if(_toolIcons) {
			_toolIcons = _toolIcons->FirstChild(TEXT("Theme"));
			if(_toolIcons) {
				const TCHAR * themeDir = (_toolIcons->ToElement())->Attribute(TEXT("pathPrefix"));

				for(TiXmlNode * childNode = _toolIcons->FirstChildElement(TEXT("Icon"));
				    childNode;
				    childNode = childNode->NextSibling(TEXT("Icon"))) {
					int iIcon;
					const TCHAR * res = (childNode->ToElement())->Attribute(TEXT("id"), &iIcon);
					if(res) {
						TiXmlNode * grandChildNode = childNode->FirstChildElement(TEXT("normal"));
						if(grandChildNode) {
							TiXmlNode * valueNode = grandChildNode->FirstChild();
							//putain, enfin!!!
							if(valueNode) {
								generic_string locator = themeDir ? themeDir : TEXT("");

								locator += valueNode->Value();
								_customIconVect.push_back(iconLocator(0, iIcon, locator));
							}
						}

						grandChildNode = childNode->FirstChildElement(TEXT("hover"));
						if(grandChildNode) {
							TiXmlNode * valueNode = grandChildNode->FirstChild();
							//putain, enfin!!!
							if(valueNode) {
								generic_string locator = themeDir ? themeDir : TEXT("");

								locator += valueNode->Value();
								_customIconVect.push_back(iconLocator(1, iIcon, locator));
							}
						}

						grandChildNode = childNode->FirstChildElement(TEXT("disabled"));
						if(grandChildNode) {
							TiXmlNode * valueNode = grandChildNode->FirstChild();
							//putain, enfin!!!
							if(valueNode) {
								generic_string locator = themeDir ? themeDir : TEXT("");

								locator += valueNode->Value();
								_customIconVect.push_back(iconLocator(2, iIcon, locator));
							}
						}
					}
				}
			}
		}
	}
}

bool ToolBar::init(HINSTANCE hInst, HWND hPere, toolBarStatusType type, ToolBarButtonUnit * buttonUnitArray, int arraySize)
{
	Window::init(hInst, hPere);

	_state = type;
	int iconSize = NppParameters::getInstance()._dpiManager.scaleX(_state == TB_LARGE || _state == TB_LARGE2 ? 32 : 16);

	_toolBarIcons.init(buttonUnitArray, arraySize, _vDynBtnReg);
	_toolBarIcons.create(_hInst, iconSize);

	INITCOMMONCONTROLSEX icex;
	icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
	icex.dwICC  = ICC_WIN95_CLASSES|ICC_COOL_CLASSES|ICC_BAR_CLASSES|ICC_USEREX_CLASSES;
	InitCommonControlsEx(&icex);

	//Create the list of buttons
	_nbButtons    = arraySize;
	_nbDynButtons = _vDynBtnReg.size();
	_nbTotalButtons = _nbButtons + (_nbDynButtons ? _nbDynButtons + 1 : 0);
	_pTBB = new TBBUTTON[_nbTotalButtons];  //add one for the extra separator

	int cmd = 0;
	int bmpIndex = -1, style;
	size_t i = 0;
	for(; i < _nbButtons; ++i) {
		cmd = buttonUnitArray[i]._cmdID;
		if(cmd != 0) {
			++bmpIndex;
			style = BTNS_BUTTON;
		}
		else {
			style = BTNS_SEP;
		}

		_pTBB[i].iBitmap = (cmd != 0 ? bmpIndex : 0);
		_pTBB[i].idCommand = cmd;
		_pTBB[i].fsState = TBSTATE_ENABLED;
		_pTBB[i].fsStyle = (BYTE)style;
		_pTBB[i].dwData = 0;
		_pTBB[i].iString = 0;
	}

	if(_nbDynButtons > 0) {
		//add separator
		_pTBB[i].iBitmap = 0;
		_pTBB[i].idCommand = 0;
		_pTBB[i].fsState = TBSTATE_ENABLED;
		_pTBB[i].fsStyle = BTNS_SEP;
		_pTBB[i].dwData = 0;
		_pTBB[i].iString = 0;
		++i;

		//add plugin buttons
		for(size_t j = 0; j < _nbDynButtons; ++j, ++i) {
			cmd = _vDynBtnReg[j]._message;
			++bmpIndex;

			_pTBB[i].iBitmap = bmpIndex;
			_pTBB[i].idCommand = cmd;
			_pTBB[i].fsState = TBSTATE_ENABLED;
			_pTBB[i].fsStyle = BTNS_BUTTON;
			_pTBB[i].dwData = 0;
			_pTBB[i].iString = 0;
		}
	}

	reset(true);    //load icons etc

	return true;
}

void ToolBar::destroy()
{
	if(_pRebar) {
		_pRebar->removeBand(_rbBand.wID);
		_pRebar = NULL;
	}
	delete [] _pTBB;
	::DestroyWindow(_hSelf);
	_hSelf = NULL;
	_toolBarIcons.destroy();
}

void ToolBar::enable(int cmdID, bool doEnable) const 
{
	::SendMessage(_hSelf, TB_ENABLEBUTTON, cmdID, static_cast<LPARAM>(doEnable));
}

int ToolBar::getWidth() const
{
	RECT btnRect;
	int totalWidth = 0;
	for(size_t i = 0; i < _nbCurrentButtons; ++i) {
		::SendMessage(_hSelf, TB_GETITEMRECT, i, reinterpret_cast<LPARAM>(&btnRect));
		totalWidth += btnRect.right - btnRect.left;
	}
	return totalWidth;
}

int ToolBar::getHeight() const
{
	DWORD size = static_cast<DWORD>(SendMessage(_hSelf, TB_GETBUTTONSIZE, 0, 0));
	DWORD padding = static_cast<DWORD>(SendMessage(_hSelf, TB_GETPADDING, 0, 0));
	int totalHeight = HIWORD(size) + HIWORD(padding) - 3;
	return totalHeight;
}

void ToolBar::reduce()
{
	int iconDpiDynamicalSize = NppParameters::getInstance()._dpiManager.scaleX(16);
	_toolBarIcons.resizeIcon(iconDpiDynamicalSize);
	setState(TB_SMALL);
	reset(true);    //recreate toolbar if previous state was Std icons or Big icons
	Window::redraw();
}

void ToolBar::enlarge()
{
	int iconDpiDynamicalSize = NppParameters::getInstance()._dpiManager.scaleX(32);
	_toolBarIcons.resizeIcon(iconDpiDynamicalSize);
	setState(TB_LARGE);
	reset(true);    //recreate toolbar if previous state was Std icons or Small icons
	Window::redraw();
}

void ToolBar::reduceToSet2()
{
	int iconDpiDynamicalSize = NppParameters::getInstance()._dpiManager.scaleX(16);
	_toolBarIcons.resizeIcon(iconDpiDynamicalSize);

	setState(TB_SMALL2);
	reset(true);
	Window::redraw();
}

void ToolBar::enlargeToSet2()
{
	int iconDpiDynamicalSize = NppParameters::getInstance()._dpiManager.scaleX(32);
	_toolBarIcons.resizeIcon(iconDpiDynamicalSize);
	setState(TB_LARGE2);
	reset(true);    //recreate toolbar if previous state was Std icons or Small icons
	Window::redraw();
}

void ToolBar::setToBmpIcons()
{
	bool recreate = true;
	setState(TB_STANDARD);
	reset(recreate);        //must recreate toolbar if setting to internal bitmaps
	Window::redraw();
}

bool ToolBar::getCheckState(int ID2Check) const 
{
	return bool(::SendMessage(_hSelf, TB_GETSTATE, ID2Check, 0) & TBSTATE_CHECKED);
}

void ToolBar::setCheck(int ID2Check, bool willBeChecked) const 
{
	::SendMessage(_hSelf, TB_CHECKBUTTON, ID2Check, MAKELONG(willBeChecked, 0));
}

void ToolBar::setDefaultImageList() 
{
	::SendMessage(_hSelf, TB_SETIMAGELIST, 0, reinterpret_cast<LPARAM>(_toolBarIcons.getDefaultLst()));
}

void ToolBar::setDisableImageList() 
{
	::SendMessage(_hSelf, TB_SETDISABLEDIMAGELIST, 0, reinterpret_cast<LPARAM>(_toolBarIcons.getDisableLst()));
}

void ToolBar::setDefaultImageList2() 
{
	::SendMessage(_hSelf, TB_SETIMAGELIST, 0, reinterpret_cast<LPARAM>(_toolBarIcons.getDefaultLstSet2()));
}

void ToolBar::setDisableImageList2() 
{
	::SendMessage(_hSelf, TB_SETDISABLEDIMAGELIST, 0, reinterpret_cast<LPARAM>(_toolBarIcons.getDisableLstSet2()));
}

void ToolBar::setDefaultImageListDM() 
{
	::SendMessage(_hSelf, TB_SETIMAGELIST, 0, reinterpret_cast<LPARAM>(_toolBarIcons.getDefaultLstDM()));
}

void ToolBar::setDisableImageListDM() 
{
	::SendMessage(_hSelf, TB_SETDISABLEDIMAGELIST, 0, reinterpret_cast<LPARAM>(_toolBarIcons.getDisableLstDM()));
}

void ToolBar::setDefaultImageListDM2() 
{
	::SendMessage(_hSelf, TB_SETIMAGELIST, 0, reinterpret_cast<LPARAM>(_toolBarIcons.getDefaultLstSetDM2()));
}

void ToolBar::setDisableImageListDM2() 
{
	::SendMessage(_hSelf, TB_SETDISABLEDIMAGELIST, 0, reinterpret_cast<LPARAM>(_toolBarIcons.getDisableLstSetDM2()));
}

void ToolBar::reset(bool create)
{
	if(create && _hSelf) {
		//Store current button state information
		TBBUTTON tempBtn;
		for(size_t i = 0; i < _nbCurrentButtons; ++i) {
			::SendMessage(_hSelf, TB_GETBUTTON, i, reinterpret_cast<LPARAM>(&tempBtn));
			_pTBB[i].fsState = tempBtn.fsState;
		}
		::DestroyWindow(_hSelf);
		_hSelf = NULL;
	}
	if(!_hSelf) {
		DWORD dwExtraStyle = 0;
		if(NppDarkMode::isEnabled()) {
			dwExtraStyle = TBSTYLE_CUSTOMERASE;
		}
		_hSelf = ::CreateWindowEx(WS_EX_PALETTEWINDOW, TOOLBARCLASSNAME, TEXT(""), WS_TOOLBARSTYLE | dwExtraStyle,
			0, 0, 0, 0, _hParent, NULL, _hInst, 0);
		NppDarkMode::setDarkTooltips(_hSelf, NppDarkMode::ToolTipsType::toolbar);
		// Send the TB_BUTTONSTRUCTSIZE message, which is required for
		// backward compatibility.
		::SendMessage(_hSelf, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
		::SendMessage(_hSelf, TB_SETEXTENDEDSTYLE, 0, TBSTYLE_EX_HIDECLIPPEDBUTTONS);
	}
	if(!_hSelf) {
		throw std::runtime_error("ToolBar::reset : CreateWindowEx() function return null");
	}
	if(_state != TB_STANDARD) { //If non standard icons, use custom imagelists
		if(_state == TB_SMALL || _state == TB_LARGE) {
			if(NppDarkMode::isEnabled()) {
				setDefaultImageListDM();
				setDisableImageListDM();
			}
			else {
				setDefaultImageList();
				setDisableImageList();
			}
		}
		else {
			if(NppDarkMode::isEnabled()) {
				setDefaultImageListDM2();
				setDisableImageListDM2();
			}
			else {
				setDefaultImageList2();
				setDisableImageList2();
			}
		}
	}
	else {
		//Else set the internal imagelist with standard bitmaps
		int iconDpiDynamicalSize = NppParameters::getInstance()._dpiManager.scaleX(16);
		::SendMessage(_hSelf, TB_SETBITMAPSIZE, 0, MAKELPARAM(iconDpiDynamicalSize, iconDpiDynamicalSize));

		TBADDBITMAP addbmp = {0, 0};
		TBADDBITMAP addbmpdyn = {0, 0};
		for(size_t i = 0; i < _nbButtons; ++i) {
			int icoID = _toolBarIcons.getStdIconAt(static_cast<int32_t>(i));
			HBITMAP hBmp =
			    static_cast<HBITMAP>(::LoadImage(_hInst, MAKEINTRESOURCE(icoID), IMAGE_BITMAP, iconDpiDynamicalSize,
			    iconDpiDynamicalSize, LR_LOADMAP3DCOLORS | LR_LOADTRANSPARENT));
			addbmp.nID = reinterpret_cast<UINT_PTR>(hBmp);
			::SendMessage(_hSelf, TB_ADDBITMAP, 1, reinterpret_cast<LPARAM>(&addbmp));
		}
		if(_nbDynButtons > 0) {
			for(size_t j = 0; j < _nbDynButtons; ++j) {
				addbmpdyn.nID = reinterpret_cast<UINT_PTR>(_vDynBtnReg.at(j)._hBmp);
				::SendMessage(_hSelf, TB_ADDBITMAP, 1, reinterpret_cast<LPARAM>(&addbmpdyn));
			}
		}
	}

	if(create) { //if the toolbar has been recreated, readd the buttons
		_nbCurrentButtons = _nbTotalButtons;
		WORD btnSize = (_state == TB_LARGE ? 32 : 16);
		::SendMessage(_hSelf, TB_SETBUTTONSIZE, 0, MAKELONG(btnSize, btnSize));
		::SendMessage(_hSelf, TB_ADDBUTTONS, _nbTotalButtons, reinterpret_cast<LPARAM>(_pTBB));
	}
	::SendMessage(_hSelf, TB_AUTOSIZE, 0, 0);

	if(_pRebar) {
		_rbBand.hwndChild       = getHSelf();
		_rbBand.cxMinChild      = 0;
		_rbBand.cyIntegral      = 1;
		_rbBand.cyMinChild      = _rbBand.cyMaxChild = getHeight();
		_rbBand.cxIdeal = getWidth();

		_pRebar->reNew(REBAR_BAR_TOOLBAR, &_rbBand);
	}
}

bool ToolBar::changeIcons() 
{
	if(!_toolIcons) 
		return false;
	else {
		for(size_t i = 0, len = _customIconVect.size(); i < len; ++i)
			changeIcons(_customIconVect[i].listIndex, _customIconVect[i].iconIndex, (_customIconVect[i].iconLocation).c_str());
		return true;
	}
}

bool ToolBar::changeIcons(int whichLst, int iconIndex, const TCHAR * iconLocation)
{
	return _toolBarIcons.replaceIcon(whichLst, iconIndex, iconLocation);
}

void ToolBar::registerDynBtn(UINT messageID, toolbarIcons* iconHandles, HICON absentIco)
{
	// Note: Register of buttons only possible before init!
	if((_hSelf == NULL) && (messageID != 0) && (iconHandles->hToolbarBmp != NULL)) {
		DynamicCmdIcoBmp dynList;
		dynList._message = messageID;
		dynList._hBmp = iconHandles->hToolbarBmp;

		if(iconHandles->hToolbarIcon) {
			dynList._hIcon = iconHandles->hToolbarIcon;
		}
		else {
			BITMAP bmp;
			int nbByteBmp = ::GetObject(dynList._hBmp, sizeof(BITMAP), &bmp);
			if(!nbByteBmp) {
				dynList._hIcon = absentIco;
			}
			else {
				HBITMAP hbmMask = ::CreateCompatibleBitmap(::GetDC(NULL), bmp.bmWidth, bmp.bmHeight);

				ICONINFO iconinfoDest = { 0 };
				iconinfoDest.fIcon = TRUE;
				iconinfoDest.hbmColor = iconHandles->hToolbarBmp;
				iconinfoDest.hbmMask = hbmMask;

				dynList._hIcon = ::CreateIconIndirect(&iconinfoDest);
				::DeleteObject(hbmMask);
			}
		}

		dynList._hIcon_DM = dynList._hIcon;

		_vDynBtnReg.push_back(dynList);
	}
}

void ToolBar::registerDynBtnDM(UINT messageID, toolbarIconsWithDarkMode* iconHandles)
{
	// Note: Register of buttons only possible before init!
	if((_hSelf == NULL) && (messageID != 0) && (iconHandles->hToolbarBmp != NULL) &&
	    (iconHandles->hToolbarIcon != NULL) && (iconHandles->hToolbarIconDarkMode != NULL)) {
		DynamicCmdIcoBmp dynList;
		dynList._message = messageID;
		dynList._hBmp = iconHandles->hToolbarBmp;
		dynList._hIcon = iconHandles->hToolbarIcon;
		dynList._hIcon_DM = iconHandles->hToolbarIconDarkMode;
		_vDynBtnReg.push_back(dynList);
	}
}

void ToolBar::doPopop(POINT chevPoint)
{
	//first find hidden buttons
	int width = Window::getWidth();

	size_t start = 0;
	RECT btnRect = {0, 0, 0, 0};
	while(start < _nbCurrentButtons) {
		::SendMessage(_hSelf, TB_GETITEMRECT, start, reinterpret_cast<LPARAM>(&btnRect));
		if(btnRect.right > width)
			break;
		++start;
	}

	if(start < _nbCurrentButtons) { //some buttons are hidden
		HMENU menu = ::CreatePopupMenu();
		generic_string text;
		while(start < _nbCurrentButtons) {
			int cmd = _pTBB[start].idCommand;
			getNameStrFromCmd(cmd, text);
			if(_pTBB[start].idCommand != 0) {
				if(::SendMessage(_hSelf, TB_ISBUTTONENABLED, cmd, 0) != 0)
					AppendMenu(menu, MF_ENABLED, cmd, text.c_str());
				else
					AppendMenu(menu, MF_DISABLED|MF_GRAYED, cmd, text.c_str());
			}
			else
				AppendMenu(menu, MF_SEPARATOR, 0, TEXT(""));

			++start;
		}
		TrackPopupMenu(menu, 0, chevPoint.x, chevPoint.y, 0, _hSelf, NULL);
	}
}

void ToolBar::addToRebar(ReBar * rebar)
{
	if(_pRebar)
		return;
	_pRebar = rebar;
	INITWINAPISTRUCT(_rbBand);
	_rbBand.fMask   = RBBIM_STYLE | RBBIM_CHILD | RBBIM_CHILDSIZE |RBBIM_SIZE | RBBIM_IDEALSIZE | RBBIM_ID;
	_rbBand.fStyle  = RBBS_VARIABLEHEIGHT | RBBS_USECHEVRON | RBBS_NOGRIPPER;
	_rbBand.hwndChild       = getHSelf();
	_rbBand.wID             = REBAR_BAR_TOOLBAR;    //ID REBAR_BAR_TOOLBAR for toolbar
	_rbBand.cxMinChild      = 0;
	_rbBand.cyIntegral      = 1;
	_rbBand.cyMinChild      = _rbBand.cyMaxChild    = getHeight();
	_rbBand.cxIdeal = _rbBand.cx            = getWidth();
	_pRebar->addBand(&_rbBand, true);
	_rbBand.fMask   = RBBIM_CHILD | RBBIM_CHILDSIZE | RBBIM_IDEALSIZE | RBBIM_SIZE;
}

constexpr UINT_PTR g_rebarSubclassID = 42;

LRESULT CALLBACK RebarSubclass(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	UNREFERENCED_PARAMETER(dwRefData);
	UNREFERENCED_PARAMETER(uIdSubclass);
	switch(uMsg) {
		case WM_ERASEBKGND:
		    if(NppDarkMode::isEnabled()) {
			    RECT rc;
			    GetClientRect(hWnd, &rc);
			    FillRect((HDC)wParam, &rc, NppDarkMode::getDarkerBackgroundBrush());
			    return TRUE;
		    }
		    else {
			    break;
		    }

		case WM_NCDESTROY:
		    RemoveWindowSubclass(hWnd, RebarSubclass, g_rebarSubclassID);
		    break;
	}
	return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

ReBar::ReBar() : Window() 
{
	usedIDs.clear();
}

/*virtual*/void ReBar::destroy() 
{
	::DestroyWindow(_hSelf);
	_hSelf = NULL;
	usedIDs.clear();
}

void ReBar::init(HINSTANCE hInst, HWND hPere)
{
	Window::init(hInst, hPere);
	_hSelf = CreateWindowEx(WS_EX_TOOLWINDOW, REBARCLASSNAME, NULL,
		WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS|WS_CLIPCHILDREN|RBS_VARHEIGHT|CCS_NODIVIDER|CCS_NOPARENTALIGN, 0, 0, 0, 0, _hParent, NULL, _hInst, NULL);
	SetWindowSubclass(_hSelf, RebarSubclass, g_rebarSubclassID, 0);
	REBARINFO rbi;
	INITWINAPISTRUCT(rbi);
	rbi.fMask  = 0;
	rbi.himl   = (HIMAGELIST)NULL;
	::SendMessage(_hSelf, RB_SETBARINFO, 0, reinterpret_cast<LPARAM>(&rbi));
}

bool ReBar::addBand(REBARBANDINFO * rBand, bool useID)
{
	if(rBand->fMask & RBBIM_STYLE) {
		if(!(rBand->fStyle & RBBS_NOGRIPPER))
			rBand->fStyle |= RBBS_GRIPPERALWAYS;
	}
	else
		rBand->fStyle = RBBS_GRIPPERALWAYS;

	rBand->fMask |= RBBIM_ID | RBBIM_STYLE;
	if(useID) {
		if(isIDTaken(rBand->wID))
			return false;
	}
	else {
		rBand->wID = getNewID();
	}
	::SendMessage(_hSelf, RB_INSERTBAND, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(rBand)); //add to end of list
	return true;
}

void ReBar::reNew(int id, REBARBANDINFO * rBand)
{
	auto index = SendMessage(_hSelf, RB_IDTOINDEX, id, 0);
	::SendMessage(_hSelf, RB_SETBANDINFO, index, reinterpret_cast<LPARAM>(rBand));
}

void ReBar::removeBand(int id)
{
	auto index = SendMessage(_hSelf, RB_IDTOINDEX, id, 0);
	if(id >= REBAR_BAR_EXTERNAL)
		releaseID(id);
	::SendMessage(_hSelf, RB_DELETEBAND, index, 0);
}

void ReBar::setIDVisible(int id, bool show)
{
	auto index = SendMessage(_hSelf, RB_IDTOINDEX, id, 0);
	if(index == -1)
		return; //error
	REBARBANDINFO rbBand;
	INITWINAPISTRUCT(rbBand);
	rbBand.fMask = RBBIM_STYLE;
	::SendMessage(_hSelf, RB_GETBANDINFO, index, reinterpret_cast<LPARAM>(&rbBand));
	if(show)
		rbBand.fStyle &= (RBBS_HIDDEN ^ -1);
	else
		rbBand.fStyle |= RBBS_HIDDEN;
	::SendMessage(_hSelf, RB_SETBANDINFO, index, reinterpret_cast<LPARAM>(&rbBand));
}

bool ReBar::getIDVisible(int id)
{
	auto index = SendMessage(_hSelf, RB_IDTOINDEX, id, 0);
	if(index == -1)
		return false;   //error
	REBARBANDINFO rbBand;
	INITWINAPISTRUCT(rbBand);
	rbBand.fMask = RBBIM_STYLE;
	::SendMessage(_hSelf, RB_GETBANDINFO, index, reinterpret_cast<LPARAM>(&rbBand));
	return ((rbBand.fStyle & RBBS_HIDDEN) == 0);
}

void ReBar::setGrayBackground(int id)
{
	auto index = SendMessage(_hSelf, RB_IDTOINDEX, id, 0);
	if(index == -1)
		return; //error
	REBARBANDINFO rbBand;
	INITWINAPISTRUCT(rbBand);
	rbBand.fMask = RBBIM_BACKGROUND;
	rbBand.hbmBack = LoadBitmap((HINSTANCE)::GetModuleHandle(NULL), MAKEINTRESOURCE(IDB_INCREMENTAL_BG));
	::SendMessage(_hSelf, RB_SETBANDINFO, index, reinterpret_cast<LPARAM>(&rbBand));
}

int ReBar::getNewID()
{
	int idToUse = REBAR_BAR_EXTERNAL;
	size_t size = usedIDs.size();
	for(size_t i = 0; i < size; ++i) {
		int curVal = usedIDs.at(i);
		if(curVal < idToUse) {
			continue;
		}
		else if(curVal == idToUse) {
			++idToUse;
		}
		else {
			break;          //found gap
		}
	}

	usedIDs.push_back(idToUse);
	return idToUse;
}

void ReBar::releaseID(int id)
{
	size_t size = usedIDs.size();
	for(size_t i = 0; i < size; ++i) {
		if(usedIDs.at(i) == id) {
			usedIDs.erase(usedIDs.begin()+i);
			break;
		}
	}
}

bool ReBar::isIDTaken(int id)
{
	size_t size = usedIDs.size();
	for(size_t i = 0; i < size; ++i) {
		if(usedIDs.at(i) == id) {
			return true;
		}
	}
	return false;
}
