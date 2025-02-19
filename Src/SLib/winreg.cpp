// WINREG.CPP
// Copyright (c) A.Sobolev 2003, 2005, 2007, 2008, 2010, 2013, 2014, 2016, 2017, 2018, 2019, 2020, 2021
// @codepage UTF-8
//
#include <slib-internal.h>
#pragma hdrstop
#include <shlwapi.h>

/*static*/SVerT SDynLibrary::GetVersion(const char * pFileName)
{
	//DWORD GetDllVersion(LPCTSTR lpszDllName)
	SVerT result;
	// For security purposes, LoadLibrary should be provided with a
	// fully-qualified path to the DLL. The lpszDllName variable should be
	// tested to ensure that it is a fully qualified path before it is used. 
	SDynLibrary dl(pFileName);
	if(dl.IsValid()) {
		DLLGETVERSIONPROC dll_get_ver_proc = reinterpret_cast<DLLGETVERSIONPROC>(dl.GetProcAddr("DllGetVersion"));
		if(dll_get_ver_proc) {
			DLLVERSIONINFO dvi;
			INITWINAPISTRUCT(dvi);
			HRESULT hr = dll_get_ver_proc(&dvi);
			if(SUCCEEDED(hr)) {
				result.Set(dvi.dwMajorVersion, dvi.dwMinorVersion, 0);
			}
		}
	}
	return result;
}

SDynLibrary::SDynLibrary(const char * pFileName) : H(0)
{
	if(pFileName)
		Load(pFileName);
}

SDynLibrary::~SDynLibrary()
{
	if(H)
		::FreeLibrary(H);
}

int FASTCALL SDynLibrary::Load(const char * pFileName)
{
	if(H) {
		::FreeLibrary(H);
		H = 0;
	}
	H = ::LoadLibrary(SUcSwitch(pFileName));
	if(H)
		return 1;
	else {
		SLS.SetError(SLERR_DLLLOADFAULT, pFileName);
		return 0;
	}
}

bool SDynLibrary::IsValid() const
{
	if(H)
		return true;
	else {
		if(SLS.GetTLA().LastErr != SLERR_DLLLOADFAULT)
			SLS.SetError(SLERR_DLLLOADFAULT, "");
		return false;
	}
}

FARPROC FASTCALL SDynLibrary::GetProcAddr(const char * pProcName)
{
	FARPROC proc = 0;
	if(H) {
		proc = ::GetProcAddress(H, pProcName);
		if(!proc)
			SLS.SetOsError(pProcName);
	}
	return proc;
}

FARPROC FASTCALL SDynLibrary::GetProcAddr(const char * pProcName, int unicodeSuffix)
{
	FARPROC proc = 0;
	if(H) {
		if(unicodeSuffix) {
			SString temp_buf(pProcName);
#ifdef UNICODE
			temp_buf.CatChar('W');
#else
			temp_buf.CatChar('A');
#endif
			proc = ::GetProcAddress(H, temp_buf);
		}
		else
			proc = ::GetProcAddress(H, pProcName);
		if(!proc)
			SLS.SetOsError(pProcName);
	}
	return proc;
}
//
//
//
WinRegValue::WinRegValue(size_t bufSize) : Type(0), P_Buf(0), BufSize(0), DataSize(0)
{
	Alloc(bufSize);
}

WinRegValue::~WinRegValue()
{
	SAlloc::F(P_Buf);
}

int WinRegValue::Alloc(size_t bufSize)
{
	if(bufSize || P_Buf) {
		P_Buf = SAlloc::R(P_Buf, bufSize);
		if(bufSize && P_Buf == 0) {
			BufSize = DataSize = 0;
			return 0;
		}
	}
	BufSize = bufSize;
	DataSize = MIN(BufSize, DataSize);
	return 1;
}

uint32 WinRegValue::GetDWord() const
{
	if(oneof3(Type, REG_DWORD, REG_DWORD_LITTLE_ENDIAN, REG_DWORD_BIG_ENDIAN))
		if(P_Buf && DataSize >= sizeof(DWORD))
			return static_cast<uint32>(*static_cast<const DWORD *>(P_Buf));
	return 0;
}

const void * WinRegValue::GetBinary(size_t * pDataLen) const
{
	ASSIGN_PTR(pDataLen, DataSize);
	return P_Buf;
}

int WinRegValue::GetString(SString & rBuf) const
{
	int    ok = 0;
	rBuf.Z();
	if(Type == REG_SZ) {
		rBuf = SUcSwitch(static_cast<const TCHAR *>(P_Buf));
		ok = 1;
	}
	return ok;
}

int WinRegValue::PutDWord(uint32 val)
{
	Type = REG_DWORD;
	if(Alloc(sizeof(DWORD))) {
		*static_cast<DWORD *>(P_Buf) = val;
		DataSize = sizeof(DWORD);
		return 1;
	}
	else
		return 0;
}

int WinRegValue::PutBinary(const void * pBuf, size_t dataSize)
{
	Type = REG_BINARY;
	if(Alloc(dataSize)) {
		memcpy(P_Buf, pBuf, dataSize);
		DataSize = dataSize;
		return 1;
	}
	else
		return 0;
}

int WinRegValue::PutString(const char * pStr)
{
	Type = REG_SZ;
	size_t len = pStr ? sstrlen(pStr)+1 : 0;
	if(Alloc(len)) {
		memcpy(P_Buf, pStr, len);
		DataSize = len;
		return 1;
	}
	else
		return 0;
}
//
//
//
WinRegKey::WinRegKey() : Key(0)
{
}

WinRegKey::WinRegKey(HKEY key, const char * pSubKey, int readOnly) : Key(0)
{
	Open(key, pSubKey, readOnly);
}

WinRegKey::~WinRegKey()
{
	Close();
}

int WinRegKey::Delete(HKEY key, const char * pSubKey)
{
	return (SHDeleteKey(key, SUcSwitch(pSubKey)) == ERROR_SUCCESS) ? 1 : SLS.SetOsError(pSubKey); // @unicodeproblem
}

int WinRegKey::DeleteValue(HKEY key, const char * pSubKey, const char * pValue)
{
	return (SHDeleteValue(key, SUcSwitch(pSubKey), SUcSwitch(pValue)) == ERROR_SUCCESS) ? 1 : SLS.SetOsError(pValue); // @unicodeproblem
}

int WinRegKey::Open(HKEY key, const char * pSubKey, int readOnly, int onlyOpen)
{
	LONG   r = 0;
	DWORD  dispos = 0;
	Close();
	if(onlyOpen)
		r = RegOpenKeyEx(key, SUcSwitch(pSubKey), 0, readOnly ? KEY_READ : (KEY_READ|KEY_WRITE), &Key); // @unicodeproblem
	else
		r = RegCreateKeyEx(key, SUcSwitch(pSubKey), 0, 0, REG_OPTION_NON_VOLATILE, readOnly ? KEY_READ : (KEY_READ|KEY_WRITE), NULL, &Key, &dispos); // @unicodeproblem
	return (r == ERROR_SUCCESS) ? 1 : SLS.SetOsError(pSubKey);
}

void WinRegKey::Close()
{
	if(Key) {
		RegCloseKey(Key);
		Key = 0;
	}
}

int WinRegKey::GetDWord(const char * pParam, uint32 * pVal)
{
	int    ok = 1;
	if(Key) {
		DWORD  type = 0;
		DWORD  val  = 0;
		DWORD  size = sizeof(val);
		LONG   r = RegQueryValueEx(Key, SUcSwitch(pParam), 0, &type, reinterpret_cast<LPBYTE>(&val), &size); // @unicodeproblem
		if(r == ERROR_SUCCESS && type == REG_DWORD) {
			ASSIGN_PTR(pVal, val);
		}
		else {
			ASSIGN_PTR(pVal, 0);
			ok = SLS.SetOsError(pParam);
		}
	}
	else
		ok = 0;
	return ok;
}

/*int WinRegKey::GetString(const char * pParam, char * pBuf, size_t bufLen)
{
	if(Key == 0)
		return 0;
	DWORD type = 0;
	DWORD size = static_cast<DWORD>(bufLen);
	LONG  r = RegQueryValueEx(Key, SUcSwitch(pParam), 0, &type, (LPBYTE)(pBuf), &size); // @unicodeproblem
	return oneof2(r, ERROR_SUCCESS, ERROR_MORE_DATA) ? 1 : SLS.SetOsError(pParam);
}*/

int WinRegKey::GetString(const char * pParam, SString & rBuf)
{
	int    ok = 1;
	rBuf.Z();
	if(Key == 0)
		ok = 0;
	else {
		DWORD type = 0;
		STempBuffer temp_buf(1024);
		DWORD size = static_cast<DWORD>(temp_buf.GetSize());
		LONG  r = ERROR_MORE_DATA;
		while(r == ERROR_MORE_DATA) {
			temp_buf.Alloc(size);
			size = static_cast<DWORD>(temp_buf.GetSize());
			r = RegQueryValueEx(Key, SUcSwitch(pParam), 0, &type, static_cast<LPBYTE>(temp_buf.vptr()), &size); // @unicodeproblem
		}
		if(r == ERROR_SUCCESS)
			rBuf.CatN(SUcSwitch(static_cast<const TCHAR *>(temp_buf.vcptr())), size / sizeof(TCHAR));
		else
			ok = SLS.SetOsError(pParam);
	}
	return ok;
}

int WinRegKey::GetBinary(const char * pParam, void * pBuf, size_t bufLen)
{
	if(Key == 0)
		return 0;
	DWORD type = 0;
	DWORD size = (DWORD)bufLen;
	LONG  r = RegQueryValueEx(Key, SUcSwitch(pParam), 0, &type, static_cast<LPBYTE>(pBuf), &size);
	return oneof2(r, ERROR_SUCCESS, ERROR_MORE_DATA) ? 1 : SLS.SetOsError(pParam);
}

int WinRegKey::GetRecSize(const char * pParam, size_t * pRecSize)
{
	if(Key == 0)
		return 0;
	DWORD type = 0;
	DWORD size = 0;
	LONG  r = RegQueryValueEx(Key, SUcSwitch(pParam), 0, &type, 0, &size);
	if(oneof2(r, ERROR_SUCCESS, ERROR_MORE_DATA)) {
		ASSIGN_PTR(pRecSize, size);
		return 1;
	}
	else if(r == ERROR_FILE_NOT_FOUND) {
		ASSIGN_PTR(pRecSize, 0);
		return -1;
	}
	else
		return SLS.SetOsError(pParam);
}

int WinRegKey::PutDWord(const char * pParam, uint32 val)
{
	if(Key == 0)
		return 0;
	LONG   r = RegSetValueEx(Key, SUcSwitch(pParam), 0, REG_DWORD, reinterpret_cast<LPBYTE>(&val), sizeof(val));
	return (r == ERROR_SUCCESS) ? 1 : SLS.SetOsError(pParam);
}

int WinRegKey::PutString(const char * pParam, const char * pBuf)
{
	if(Key == 0)
		return 0;
	const  TCHAR * p_buf_to_store = SUcSwitch(pBuf);
	DWORD  size_to_store = static_cast<DWORD>((sstrlen(pBuf) + 1) * sizeof(TCHAR));
	LONG   r = RegSetValueEx(Key, SUcSwitch(pParam), 0, REG_SZ, reinterpret_cast<const BYTE *>(p_buf_to_store), size_to_store); // @v10.4.5 
	// @v10.4.5 LONG   r = RegSetValueEx(Key, SUcSwitch(pParam), 0, REG_SZ, (LPBYTE)pBuf, (DWORD)(sstrlen(pBuf) + 1));
	return (r == ERROR_SUCCESS) ? 1 : SLS.SetOsError(pParam);
}

int WinRegKey::PutBinary(const char * pParam, const void * pBuf, size_t bufLen)
{
	if(Key == 0)
		return 0;
	LONG   r = RegSetValueEx(Key, SUcSwitch(pParam), 0, REG_BINARY, static_cast<const uint8 *>(pBuf), (DWORD)bufLen);
	return (r == ERROR_SUCCESS) ? 1 : SLS.SetOsError(pParam);
}

int WinRegKey::PutValue(const char * pParam, const WinRegValue * pVal)
{
	if(Key == 0 || !pVal->GetType())
		return 0;
	LONG   r = RegSetValueEx(Key, SUcSwitch(pParam), 0, pVal->GetType(), static_cast<const uint8 *>(pVal->P_Buf), (DWORD)pVal->DataSize);
	return (r == ERROR_SUCCESS) ? 1 : SLS.SetOsError(pParam);
}

int WinRegKey::EnumValues(uint * pIdx, SString * pParam, WinRegValue * pVal)
{
	CALLPTRMEMB(pParam, Z()); // @v10.3.11
	const size_t init_name_len = 256;
	if(Key == 0)
		return 0;
	DWORD  idx = *pIdx;
	DWORD  data_len = 2048;
	DWORD  name_len = init_name_len;
	TCHAR  name[init_name_len];
	if(!pVal->Alloc(data_len))
		return 0;
	DWORD  typ = pVal->Type;
	LONG   r = RegEnumValue(Key, idx, name, &name_len, 0, &typ, reinterpret_cast<BYTE *>(pVal->P_Buf), &data_len);
	pVal->Type = typ;
	if(r == ERROR_SUCCESS) {
		ASSIGN_PTR(pParam, SUcSwitch(name));
		pVal->DataSize = data_len;
		*pIdx = idx+1;
		return 1;
	}
	else
		return SLS.SetOsError();
}

int WinRegKey::EnumKeys(uint * pIdx, SString & rKey)
{
	int    ok = 0;
	if(Key && pIdx) {
		const  size_t init_name_len = 256;
		DWORD  idx = *pIdx;
		DWORD  data_len = 2048;
		DWORD  name_len = init_name_len;
		FILETIME last_tm;
		TCHAR  name[init_name_len];
		LONG   r = RegEnumKeyEx(Key, idx, name, &name_len, NULL, NULL, NULL, &last_tm);
		if(r == ERROR_SUCCESS) {
			rKey.CopyFrom(SUcSwitch(name));
			*pIdx = idx + 1;
			ok = 1;
		}
	}
	return ok;
}

