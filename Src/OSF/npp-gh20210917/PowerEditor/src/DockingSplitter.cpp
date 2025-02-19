// This file is part of Notepad++ project
// Copyright (C)2006 Jens Lorenz <jens.plugin.npp@gmx.de>
// @license GNU GPL
//
#include <npp-internal.h>
#pragma hdrstop

BOOL DockingSplitter::_isVertReg = FALSE;
BOOL DockingSplitter::_isHoriReg = FALSE;

void DockingSplitter::init(HINSTANCE hInst, HWND hWnd, HWND hMessage, UINT flags)
{
	Window::init(hInst, hWnd);
	_hMessage = hMessage;
	_flags = flags;
	WNDCLASS wc;
	if(flags & DMS_HORIZONTAL) {
		//double sided arrow pointing north-south as cursor
		wc.hCursor       = ::LoadCursor(NULL, IDC_SIZENS);
		wc.lpszClassName = TEXT("nsdockspliter");
	}
	else {
		// double sided arrow pointing east-west as cursor
		wc.hCursor       = ::LoadCursor(NULL, IDC_SIZEWE);
		wc.lpszClassName = TEXT("wedockspliter");
	}
	if(((_isHoriReg == FALSE) && (flags & DMS_HORIZONTAL)) || ((_isVertReg == FALSE) && (flags & DMS_VERTICAL))) {
		wc.style = CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc = staticWinProc;
		wc.cbClsExtra = 0;
		wc.cbWndExtra = 0;
		wc.hInstance = _hInst;
		wc.hIcon = NULL;
		wc.hbrBackground = (HBRUSH)(COLOR_3DFACE+1);
		wc.lpszMenuName = NULL;
		if(!::RegisterClass(&wc)) {
			throw std::runtime_error("DockingSplitter::init : RegisterClass() function failed");
		}
		else if(flags & DMS_HORIZONTAL) {
			_isHoriReg      = TRUE;
		}
		else {
			_isVertReg      = TRUE;
		}
	}
	/* create splitter windows and initialize it */
	_hSelf = ::CreateWindowEx(0, wc.lpszClassName, TEXT(""), WS_CHILD | WS_VISIBLE,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, _hParent, NULL, _hInst, (LPVOID)this);
	if(!_hSelf) {
		throw std::runtime_error("DockingSplitter::init : CreateWindowEx() function return null");
	}
}

LRESULT CALLBACK DockingSplitter::staticWinProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	DockingSplitter * pDockingSplitter = NULL;
	switch(message) {
		case WM_NCCREATE:
		    pDockingSplitter = reinterpret_cast<DockingSplitter *>(reinterpret_cast<LPCREATESTRUCT>(lParam)->lpCreateParams);
		    pDockingSplitter->_hSelf = hwnd;
		    ::SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pDockingSplitter));
		    return TRUE;
		default:
		    pDockingSplitter = reinterpret_cast<DockingSplitter *>(::GetWindowLongPtr(hwnd, GWLP_USERDATA));
		    if(!pDockingSplitter)
			    return ::DefWindowProc(hwnd, message, wParam, lParam);
		    return pDockingSplitter->runProc(hwnd, message, wParam, lParam);
	}
}

LRESULT DockingSplitter::runProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message) {
		case WM_LBUTTONDOWN:
		    ::SetCapture(_hSelf);
		    ::GetCursorPos(&_ptOldPos);
		    _isLeftButtonDown = TRUE;
		    break;
		case WM_LBUTTONUP:
		case WM_NCLBUTTONUP:
		    ::ReleaseCapture();
		    _isLeftButtonDown = FALSE;
		    break;
		case WM_MOUSEMOVE:
		case WM_NCMOUSEMOVE:
		    if(_isLeftButtonDown == TRUE) {
			    POINT pt;
			    ::GetCursorPos(&pt);
			    if((_flags & DMS_HORIZONTAL) && (_ptOldPos.y != pt.y)) {
				    ::SendMessage(_hMessage, DMM_MOVE_SPLITTER, _ptOldPos.y - pt.y, reinterpret_cast<LPARAM>(_hSelf));
			    }
			    else if(_ptOldPos.x != pt.x) {
				    ::SendMessage(_hMessage, DMM_MOVE_SPLITTER, _ptOldPos.x - pt.x, reinterpret_cast<LPARAM>(_hSelf));
			    }
			    _ptOldPos = pt;
		    }
		    break;
		case WM_ERASEBKGND:
		    if(!NppDarkMode::isEnabled()) {
			    break;
		    }
			else {
				RECT rc = { 0 };
				getClientRect(rc);
				::FillRect(reinterpret_cast<HDC>(wParam), &rc, NppDarkMode::getBackgroundBrush());
				return TRUE;
			}
		default:
		    break;
	}
	return ::DefWindowProc(hwnd, message, wParam, lParam);
}
