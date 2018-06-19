#include "webview.h"


#if defined(WEBVIEW_WINAPI)

#include <commctrl.h>
#include <exdisp.h>
#include <mshtmhst.h>
#include <mshtml.h>
#include <shobjidl.h>
#include <ExDispid.h>

#include <string>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

#define WM_WEBVIEW_DISPATCH (WM_APP + 1)
#define WM_WEBVIEW_KEYSTROKE (WM_APP + 2)


#ifdef __cplusplus
#define iid_ref(x) &(x)
#define iid_unref(x) *(x)
#else
#define iid_ref(x) (x)
#define iid_unref(x) (x)
#endif

static inline WCHAR *webview_to_utf16(const char *s) {
	DWORD size = MultiByteToWideChar(CP_UTF8, 0, s, -1, 0, 0);
	WCHAR *ws = (WCHAR *)GlobalAlloc(GMEM_FIXED, sizeof(WCHAR) * size);
	if (ws == NULL) {
		return NULL;
	}
	MultiByteToWideChar(CP_UTF8, 0, s, -1, ws, size);
	return ws;
}

static inline char *webview_from_utf16(WCHAR *ws) {
	int n = WideCharToMultiByte(CP_UTF8, 0, ws, -1, NULL, 0, NULL, NULL);
	char *s = (char *)GlobalAlloc(GMEM_FIXED, n);
	if (s == NULL) {
		return NULL;
	}
	WideCharToMultiByte(CP_UTF8, 0, ws, -1, s, n, NULL, NULL);
	return s;
}

static int iid_eq(REFIID a, const IID *b) {
	return memcmp((const void *)iid_ref(a), (const void *)b, sizeof(GUID)) == 0;
}

struct WebViewEvents : public DWebBrowserEvents2
{
	struct WebViewClient * client;

	// Inherited via DWebBrowserEvents2
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void ** ppvObject) override
	{
		if (iid_eq(riid, &IID_IUnknown) || iid_eq(riid, &IID_IDispatch) || 
			iid_eq(riid, &__uuidof(DWebBrowserEvents2))) {
			*ppvObject = this;
			return S_OK;
		} else {
			*ppvObject = NULL;
			return E_NOINTERFACE;
		}
	}
	virtual ULONG STDMETHODCALLTYPE AddRef(void) override
	{
		return 1;
	}
	virtual ULONG STDMETHODCALLTYPE Release(void) override
	{
		return 1;
	}
	virtual HRESULT STDMETHODCALLTYPE GetTypeInfoCount(UINT * pctinfo) override
	{
		return E_NOTIMPL;
	}
	virtual HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT iTInfo, LCID lcid, ITypeInfo ** ppTInfo) override
	{
		return E_NOTIMPL;
	}
	virtual HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID riid, LPOLESTR * rgszNames, UINT cNames, LCID lcid, DISPID * rgDispId) override
	{
		return E_NOTIMPL;
	}
	virtual HRESULT STDMETHODCALLTYPE Invoke(DISPID dispIdMember, REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS * pDispParams, VARIANT * pVarResult, EXCEPINFO * pExcepInfo, UINT * puArgErr) override
	{
		switch (dispIdMember) {
		case DISPID_BEFORENAVIGATE2:
		{
			VARIANTARG & va_url = pDispParams->rgvarg[5];
			VARIANTARG & va_cancel = pDispParams->rgvarg[0];

			const wchar_t * url = va_url.pvarVal->bstrVal;
			VARIANT_BOOL * cancel = va_cancel.pboolVal;

			BOOL do_cancel = false;
			HandleURL(url, &do_cancel);
			*cancel = do_cancel ? 1 : 0;
			break;
		}
		case DISPID_NAVIGATECOMPLETE2:
		{
			VARIANTARG & va_url = pDispParams->rgvarg[0];
			const wchar_t * url = va_url.pvarVal->bstrVal;

			BOOL do_cancel = false;
			HandleURL(url, &do_cancel);

			break;
		}
		}

		return S_OK;
	}

	void HandleURL(const wchar_t * url, BOOL * result);

	DWORD eventConnCookie;
};

struct WebViewClient : public IOleClientSite, IOleInPlaceSite, IDocHostUIHandler
{
public:	
	// Inherited via IOleInPlaceSite
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void ** ppvObject) override;
	virtual ULONG STDMETHODCALLTYPE AddRef(void) override;
	virtual ULONG STDMETHODCALLTYPE Release(void) override;
	virtual HRESULT STDMETHODCALLTYPE GetWindow(HWND * phwnd) override;
	virtual HRESULT STDMETHODCALLTYPE ContextSensitiveHelp(BOOL fEnterMode) override;
	virtual HRESULT STDMETHODCALLTYPE CanInPlaceActivate(void) override;
	virtual HRESULT STDMETHODCALLTYPE OnInPlaceActivate(void) override;
	virtual HRESULT STDMETHODCALLTYPE OnUIActivate(void) override;
	virtual HRESULT STDMETHODCALLTYPE GetWindowContext(IOleInPlaceFrame ** ppFrame, IOleInPlaceUIWindow ** ppDoc, LPRECT lprcPosRect, LPRECT lprcClipRect, LPOLEINPLACEFRAMEINFO lpFrameInfo) override;
	virtual HRESULT STDMETHODCALLTYPE Scroll(SIZE scrollExtant) override;
	virtual HRESULT STDMETHODCALLTYPE OnUIDeactivate(BOOL fUndoable) override;
	virtual HRESULT STDMETHODCALLTYPE OnInPlaceDeactivate(void) override;
	virtual HRESULT STDMETHODCALLTYPE DiscardUndoState(void) override;
	virtual HRESULT STDMETHODCALLTYPE DeactivateAndUndo(void) override;
	virtual HRESULT STDMETHODCALLTYPE OnPosRectChange(LPCRECT lprcPosRect) override;

	// Inherited via IOleClientSite
	virtual HRESULT STDMETHODCALLTYPE SaveObject(void) override;
	virtual HRESULT STDMETHODCALLTYPE GetMoniker(DWORD dwAssign, DWORD dwWhichMoniker, IMoniker ** ppmk) override;
	virtual HRESULT STDMETHODCALLTYPE GetContainer(IOleContainer ** ppContainer) override;
	virtual HRESULT STDMETHODCALLTYPE ShowObject(void) override;
	virtual HRESULT STDMETHODCALLTYPE OnShowWindow(BOOL fShow) override;
	virtual HRESULT STDMETHODCALLTYPE RequestNewObjectLayout(void) override;

	// Inherited via IDocHostUIHandler2
	virtual HRESULT STDMETHODCALLTYPE ShowContextMenu(DWORD dwID, POINT * ppt, IUnknown * pcmdtReserved, IDispatch * pdispReserved) override;
	virtual HRESULT STDMETHODCALLTYPE GetHostInfo(DOCHOSTUIINFO * pInfo) override;
	virtual HRESULT STDMETHODCALLTYPE ShowUI(DWORD dwID, IOleInPlaceActiveObject * pActiveObject, IOleCommandTarget * pCommandTarget, IOleInPlaceFrame * pFrame, IOleInPlaceUIWindow * pDoc) override;
	virtual HRESULT STDMETHODCALLTYPE HideUI(void) override;
	virtual HRESULT STDMETHODCALLTYPE UpdateUI(void) override;
	virtual HRESULT STDMETHODCALLTYPE EnableModeless(BOOL fEnable) override;
	virtual HRESULT STDMETHODCALLTYPE OnDocWindowActivate(BOOL fActivate) override;
	virtual HRESULT STDMETHODCALLTYPE OnFrameWindowActivate(BOOL fActivate) override;
	virtual HRESULT STDMETHODCALLTYPE ResizeBorder(LPCRECT prcBorder, IOleInPlaceUIWindow * pUIWindow, BOOL fRameWindow) override;
	virtual HRESULT STDMETHODCALLTYPE GetOptionKeyPath(LPOLESTR * pchKey, DWORD dw) override;
	virtual HRESULT STDMETHODCALLTYPE GetDropTarget(IDropTarget * pDropTarget, IDropTarget ** ppDropTarget) override;
	virtual HRESULT STDMETHODCALLTYPE GetExternal(IDispatch ** ppDispatch) override;
	virtual HRESULT STDMETHODCALLTYPE TranslateUrl(DWORD dwTranslate, LPWSTR pchURLIn, LPWSTR * ppchURLOut) override;
	virtual HRESULT STDMETHODCALLTYPE FilterDataObject(IDataObject * pDO, IDataObject ** ppDORet) override;
	virtual HRESULT STDMETHODCALLTYPE TranslateAccelerator(LPMSG lpMsg, const GUID *pguidCmdGroup, DWORD nCmdID) override;

	//IDispatch external;
	IWebBrowser2 * webBrowser2;
	IOleObject * browser;

	std::wstring oauthPrefix;
	std::wstring oauthResult;

	HWND parentWnd;

	IOleInPlaceActiveObject * activeObject;

	WebViewEvents events;
};


#if 0
static HRESULT STDMETHODCALLTYPE JS_QueryInterface(IDispatch FAR *This,
	REFIID riid,
	LPVOID FAR *ppvObj) {
	if (iid_eq(riid, &IID_IDispatch)) {
		*ppvObj = This;
		return S_OK;
	}
	*ppvObj = 0;
	return E_NOINTERFACE;
}
static ULONG STDMETHODCALLTYPE JS_AddRef(IDispatch FAR *This) { return 1; }
static ULONG STDMETHODCALLTYPE JS_Release(IDispatch FAR *This) { return 1; }
static HRESULT STDMETHODCALLTYPE JS_GetTypeInfoCount(IDispatch FAR *This,
	UINT *pctinfo) {
	return S_OK;
}
static HRESULT STDMETHODCALLTYPE JS_GetTypeInfo(IDispatch FAR *This,
	UINT iTInfo, LCID lcid,
	ITypeInfo **ppTInfo) {
	return S_OK;
}
#define WEBVIEW_JS_INVOKE_ID 0x1000
static HRESULT STDMETHODCALLTYPE JS_GetIDsOfNames(IDispatch FAR *This,
	REFIID riid,
	LPOLESTR *rgszNames,
	UINT cNames, LCID lcid,
	DISPID *rgDispId) {
	if (cNames != 1) {
		return S_FALSE;
	}
	if (wcscmp(rgszNames[0], L"invoke") == 0) {
		rgDispId[0] = WEBVIEW_JS_INVOKE_ID;
		return S_OK;
	}
	return S_FALSE;
}

static HRESULT STDMETHODCALLTYPE
JS_Invoke(IDispatch FAR *This, DISPID dispIdMember, REFIID riid, LCID lcid,
	WORD wFlags, DISPPARAMS *pDispParams, VARIANT *pVarResult,
	EXCEPINFO *pExcepInfo, UINT *puArgErr) {
	size_t offset = (size_t) & ((WvOleClientSite *)NULL)->external;
	_IOleClientSiteEx *ex = (_IOleClientSiteEx *)((char *)(This) - offset);
	webview_t *w = (webview_t *)GetWindowLongPtr(ex->inplace.frame.window, GWLP_USERDATA);
	if (pDispParams->cArgs == 1 && pDispParams->rgvarg[0].vt == VT_BSTR) {
		BSTR bstr = pDispParams->rgvarg[0].bstrVal;
		char *s = webview_from_utf16(bstr);
		if (s != NULL) {
			if (dispIdMember == WEBVIEW_JS_INVOKE_ID) {
				if (w->external_invoke_cb != NULL) {
					w->external_invoke_cb(w, s);
				}
			}
			else {
				return S_FALSE;
			}
			GlobalFree(s);
		}
	}
	return S_OK;
}

static IDispatchVtbl ExternalDispatchTable = {
	JS_QueryInterface, JS_AddRef,        JS_Release, JS_GetTypeInfoCount,
	JS_GetTypeInfo,    JS_GetIDsOfNames, JS_Invoke };
#endif

//-----------------------

HRESULT STDMETHODCALLTYPE WebViewClient::SaveObject() {
	return E_NOTIMPL;
}
HRESULT STDMETHODCALLTYPE WebViewClient::GetMoniker(DWORD dwAssign, DWORD dwWhichMoniker, IMoniker **ppmk) {
	return E_NOTIMPL;
}
HRESULT STDMETHODCALLTYPE WebViewClient::GetContainer(IOleContainer ** ppContainer) {
	*ppContainer = 0;
	return E_NOINTERFACE;
}
HRESULT STDMETHODCALLTYPE WebViewClient::ShowObject() {
	return NOERROR;
}
HRESULT STDMETHODCALLTYPE WebViewClient::OnShowWindow(BOOL fShow) {
	return E_NOTIMPL;
}
HRESULT STDMETHODCALLTYPE WebViewClient::RequestNewObjectLayout() {
	return E_NOTIMPL;
}

//----------------

HRESULT STDMETHODCALLTYPE WebViewClient::QueryInterface(REFIID riid, void **ppvObject) {
	if (iid_eq(riid, &IID_IUnknown) || iid_eq(riid, &IID_IOleClientSite)) {
		*ppvObject = static_cast< IOleClientSite* >(this);
	} else if (iid_eq(riid, &IID_IOleInPlaceSite)) {
		*ppvObject = static_cast< IOleInPlaceSite* >(this);
	} else if (iid_eq(riid, &IID_IDocHostUIHandler)) {
		*ppvObject = static_cast<IDocHostUIHandler*>(this);
	} else {
		*ppvObject = 0;
		return (E_NOINTERFACE);
	}
	return S_OK;
}

//----------------

ULONG STDMETHODCALLTYPE WebViewClient::AddRef() {
	return 1;
}
ULONG STDMETHODCALLTYPE WebViewClient::Release() {
	return 1;
}
HRESULT STDMETHODCALLTYPE WebViewClient::GetWindow(HWND FAR *lphwnd) {
	*lphwnd = parentWnd;
	return S_OK;
}
HRESULT STDMETHODCALLTYPE
WebViewClient::ContextSensitiveHelp(BOOL fEnterMode) {
	return E_NOTIMPL;
}
HRESULT STDMETHODCALLTYPE
WebViewClient::CanInPlaceActivate() {
	return S_OK;
}
HRESULT STDMETHODCALLTYPE
WebViewClient::OnInPlaceActivate() {
	return S_OK;
}
HRESULT STDMETHODCALLTYPE
WebViewClient::OnUIActivate() {
	return S_OK;
}
HRESULT STDMETHODCALLTYPE WebViewClient::GetWindowContext(IOleInPlaceFrame ** ppFrame, IOleInPlaceUIWindow ** ppDoc, LPRECT lprcPosRect, LPRECT lprcClipRect, LPOLEINPLACEFRAMEINFO lpFrameInfo) {
	*ppFrame = 0;
	*ppDoc = 0;
	lpFrameInfo->fMDIApp = FALSE;
	lpFrameInfo->hwndFrame = parentWnd;
	lpFrameInfo->haccel = 0;
	lpFrameInfo->cAccelEntries = 0;
	return S_OK;
}
HRESULT STDMETHODCALLTYPE WebViewClient::Scroll(SIZE scrollExtent) {
	return E_NOTIMPL;
}
HRESULT STDMETHODCALLTYPE WebViewClient::OnUIDeactivate(BOOL fUndoable) {
	return S_OK;
}
HRESULT STDMETHODCALLTYPE WebViewClient::OnInPlaceDeactivate() {
	return S_OK;
}
HRESULT STDMETHODCALLTYPE WebViewClient::DiscardUndoState() {
	return E_NOTIMPL;
}
HRESULT STDMETHODCALLTYPE WebViewClient::DeactivateAndUndo() {
	return E_NOTIMPL;
}
HRESULT STDMETHODCALLTYPE WebViewClient::OnPosRectChange(LPCRECT lprcPosRect) {
	IOleInPlaceObject *inplace;
	if (!browser->QueryInterface(iid_unref(&IID_IOleInPlaceObject), (void **)&inplace)) {
		inplace->SetObjectRects(lprcPosRect, lprcPosRect);
		inplace->Release();
	}
	return S_OK;
}

//----------------------
HRESULT STDMETHODCALLTYPE WebViewClient::ShowContextMenu(DWORD dwID, POINT *ppt, IUnknown * pcmdtReserved, IDispatch * pdispReserved) {
//	if (dwID == 0)
		return S_OK;
//	else
//		return E_NOTIMPL;
}
HRESULT STDMETHODCALLTYPE WebViewClient::GetHostInfo(DOCHOSTUIINFO *pInfo) {
	pInfo->cbSize = sizeof(DOCHOSTUIINFO);
	pInfo->dwFlags = DOCHOSTUIFLAG_NO3DBORDER;
	pInfo->dwDoubleClick = DOCHOSTUIDBLCLK_DEFAULT;
	return S_OK;
}
HRESULT STDMETHODCALLTYPE WebViewClient::ShowUI(DWORD dwID, IOleInPlaceActiveObject * pActiveObject, IOleCommandTarget * pCommandTarget,
	IOleInPlaceFrame * pFrame, IOleInPlaceUIWindow * pDoc) {
	return S_OK;
}
HRESULT STDMETHODCALLTYPE WebViewClient::HideUI() {
	return S_OK;
}
HRESULT STDMETHODCALLTYPE WebViewClient::UpdateUI() {
	return S_OK;
}
HRESULT STDMETHODCALLTYPE WebViewClient::EnableModeless(BOOL fEnable) {
	return S_OK;
}
HRESULT STDMETHODCALLTYPE WebViewClient::OnDocWindowActivate(BOOL fActivate) {
	return S_OK;
}
HRESULT STDMETHODCALLTYPE WebViewClient::OnFrameWindowActivate(BOOL fActivate) {
	return S_OK;
}
HRESULT STDMETHODCALLTYPE WebViewClient::ResizeBorder(LPCRECT prcBorder, IOleInPlaceUIWindow *pUIWindow, BOOL fRameWindow) {
	return S_OK;
}
HRESULT STDMETHODCALLTYPE WebViewClient::TranslateAccelerator(LPMSG lpMsg, const GUID *pguidCmdGroup, DWORD nCmdID) {
	return S_FALSE;
}
HRESULT STDMETHODCALLTYPE WebViewClient::GetOptionKeyPath(LPOLESTR *pchKey, DWORD dw) {
	return S_FALSE;
}
HRESULT STDMETHODCALLTYPE WebViewClient::GetDropTarget(IDropTarget *pDropTarget, IDropTarget **ppDropTarget) {
	return S_FALSE;
}
HRESULT STDMETHODCALLTYPE WebViewClient::GetExternal(IDispatch ** ppDispatch) {
#if 0
	*ppDispatch = (IDispatch *)(This + 1);
	return S_OK;
#else
	return E_NOTIMPL;
#endif
}
HRESULT STDMETHODCALLTYPE WebViewClient::TranslateUrl(DWORD dwTranslate, LPWSTR pchURLIn, LPWSTR * ppchURLOut) {
	*ppchURLOut = 0;
	return S_FALSE;
}
HRESULT STDMETHODCALLTYPE WebViewClient::FilterDataObject(IDataObject * pDO, IDataObject ** ppDORet) {
	*ppDORet = 0;
	return S_FALSE;
}

//----------------------

void WebViewEvents::HandleURL(const wchar_t * url, BOOL * cancel)
{
	if (wcsnicmp(url, client->oauthPrefix.c_str(), client->oauthPrefix.size()) == 0) {
		printf("Got required URL!");
		*cancel = true;
	}
}

//-----------------------

static const TCHAR *classname = "WebView";
static const SAFEARRAYBOUND ArrayBound = { 1, 0 };

static void UnEmbedBrowserObject(webview_t *w) 
{
	if (w->priv.client != NULL) {
		if (w->priv.client->activeObject)
			w->priv.client->activeObject->Release();

		if (w->priv.client->webBrowser2)
			w->priv.client->webBrowser2->Release();

		if (w->priv.client->browser) {
			w->priv.client->browser->Close(OLECLOSE_NOSAVE);
			w->priv.client->browser->Release();
		}

		delete w->priv.client;

		w->priv.client = NULL;
	}
}

static int EmbedBrowserObject(webview_t *w) 
{
	RECT rect;
	IWebBrowser2 *webBrowser2 = NULL;
	LPCLASSFACTORY pClassFactory = NULL;
	WebViewClient * client = NULL;

	client = new WebViewClient();
	w->priv.client = client;

	wchar_t * prefix = webview_to_utf16(w->oauth_callback_prefix);

	client->parentWnd = w->priv.hwnd;
	client->oauthPrefix = prefix;

	GlobalFree(prefix);

	if (CoGetClassObject(__uuidof(WebBrowser), CLSCTX_INPROC_SERVER | CLSCTX_INPROC_HANDLER, NULL, __uuidof(IClassFactory), (void **)&pClassFactory) != S_OK) {
		goto error;
	}

	if (pClassFactory == NULL) {
		goto error;
	}

	if (pClassFactory->CreateInstance(0, iid_unref(&IID_IOleObject), (void **)&client->browser) != S_OK) {
		goto error;
	}
	pClassFactory->Release();
	if (client->browser->SetClientSite((IOleClientSite *)client) != S_OK) {
		goto error;
	}
	client->browser->SetHostNames(L"My Host Name", 0);

	if (OleSetContainedObject((struct IUnknown *)client->browser, TRUE) != S_OK) {
		goto error;
	}
	if (client->browser->QueryInterface(iid_unref(&IID_IOleInPlaceActiveObject), (void **)&(client->activeObject)) != S_OK) {
		goto error;
	}
	GetClientRect(w->priv.hwnd, &rect);
	if (client->browser->DoVerb(OLEIVERB_SHOW, NULL,
		(IOleClientSite *)client, -1,
		w->priv.hwnd, &rect) != S_OK) {
		goto error;
	}
	if (client->browser->QueryInterface(iid_unref(&IID_IWebBrowser2), (void **)&webBrowser2) != S_OK) {
		goto error;
	}

	IConnectionPointContainer * cpc;

	HRESULT hr;
	hr = webBrowser2->QueryInterface< IConnectionPointContainer >(&cpc);
	if (hr == S_OK) {
		IConnectionPoint * cp;
		hr = cpc->FindConnectionPoint(__uuidof(DWebBrowserEvents2), &cp);
		if (hr == S_OK) {
			hr = cp->Advise(&client->events, &client->events.eventConnCookie);
			client->events.client = client;
		}
	}

	webBrowser2->put_Left(0);
	webBrowser2->put_Top(0);
	webBrowser2->put_Width(rect.right);
	webBrowser2->put_Height(rect.bottom);

	client->webBrowser2 = webBrowser2;

	return 0;
error:
	UnEmbedBrowserObject(w);
	if (pClassFactory != NULL) {
		pClassFactory->Release();
	}
	if (client != NULL) {
		delete client;
	}
	return -1;
}

#define WEBVIEW_DATA_URL_PREFIX "data:text/html,"
static int DisplayHTMLPage(webview_t *w) 
{
	VARIANT myURL;
	LPDISPATCH lpDispatch;
	IHTMLDocument2 *htmlDoc2;
	BSTR bstr;
	SAFEARRAY *sfArray;
	VARIANT *pVar;

	IWebBrowser2 *webBrowser2 = w->priv.client->webBrowser2;

	int isDataURL = 0;

	const char *webview_url = webview_check_url(w->url);
	LPCSTR webPageName;
	isDataURL = (strncmp(webview_url, WEBVIEW_DATA_URL_PREFIX, strlen(WEBVIEW_DATA_URL_PREFIX)) == 0);
	if (isDataURL) {
		webPageName = "about:blank";
	} else {
		webPageName = (LPCSTR)webview_url;
	}
	VariantInit(&myURL);
	myURL.vt = VT_BSTR;
#ifndef UNICODE
	{
		wchar_t *buffer = webview_to_utf16(webPageName);
		if (buffer == NULL) {
			goto badalloc;
		}
		myURL.bstrVal = SysAllocString(buffer);
		GlobalFree(buffer);
	}
#else
	myURL.bstrVal = SysAllocString(webPageName);
#endif
	if (!myURL.bstrVal) {
	badalloc:
		return (-6);
	}
	webBrowser2->Navigate2(&myURL, 0, 0, 0, 0);
	VariantClear(&myURL);
	if (!isDataURL) {
		return 0;
	}

	char *url = (char *)calloc(1, strlen(webview_url) + 1);
	char *q = url;
	for (const char *p = webview_url + strlen(WEBVIEW_DATA_URL_PREFIX); *q = *p; p++, q++) {
		if (*q == '%' && *(p + 1) && *(p + 2)) {
			int r;
			sscanf(p + 1, "%02x", &r);
			*q = (char)r;
			p = p + 2;
		}
	}

	if (webBrowser2->get_Document(&lpDispatch) == S_OK) {
		if (lpDispatch->QueryInterface(iid_unref(&IID_IHTMLDocument2), (void **)&htmlDoc2) == S_OK) {
			if ((sfArray = SafeArrayCreate(VT_VARIANT, 1, (SAFEARRAYBOUND *)&ArrayBound))) {
				if (!SafeArrayAccessData(sfArray, (void **)&pVar)) {
					pVar->vt = VT_BSTR;
#ifndef UNICODE
					{
						wchar_t *buffer = webview_to_utf16(url);
						if (buffer == NULL) {
							goto release;
						}
						bstr = SysAllocString(buffer);
						GlobalFree(buffer);
					}
#else
					bstr = SysAllocString(string);
#endif

					if ((pVar->bstrVal = bstr)) {
						htmlDoc2->write(sfArray);
						htmlDoc2->close();
					}
				}
				SafeArrayDestroy(sfArray);
			}
		release:
			free(url);
			htmlDoc2->Release();
		}
		lpDispatch->Release();
		return (0);
	}
	return (-5);
}

static HHOOK msghook = NULL;

static LRESULT CALLBACK GetMsgHookProc(int code, WPARAM wparam, LPARAM lparam)
{
	MSG & msg = *(MSG*)lparam;

	if (code >= 0) {
		if (msg.message >= WM_KEYFIRST && msg.message <= WM_KEYLAST) {
			auto parent = GetForegroundWindow();
			HRESULT r = (HRESULT)SendMessageW(parent, WM_WEBVIEW_KEYSTROKE, wparam, lparam);

			if (r != S_FALSE)
				ZeroMemory(&msg, sizeof(MSG));
		}
	}

	return CallNextHookEx(NULL, code, wparam, lparam);
}

static LRESULT CALLBACK wndproc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	webview_t *w = (webview_t *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
	switch (uMsg) {
	case WM_CREATE:
		w = (webview_t *)((CREATESTRUCT *)lParam)->lpCreateParams;
		w->priv.hwnd = hwnd;

		// Set static message hook to capture all the key events. MS didn't make this easy at all.
		if (msghook == NULL) {
			msghook = SetWindowsHookExW(WH_GETMESSAGE, &GetMsgHookProc, NULL, GetCurrentThreadId());
		}

		return EmbedBrowserObject(w);
	case WM_DESTROY:
		UnEmbedBrowserObject(w);
		return TRUE;
	case WM_SIZE: {
		if (w) {
			IWebBrowser2 *webBrowser2 = w->priv.client->webBrowser2;
			RECT rect;
			GetClientRect(hwnd, &rect);
			webBrowser2->put_Width(rect.right);
			webBrowser2->put_Height(rect.bottom);
		}
		return TRUE;
	}
	case WM_WEBVIEW_DISPATCH: {
		webview_dispatch_fn f = (webview_dispatch_fn)wParam;
		void *arg = (void *)lParam;
		(*f)(w, arg);
		return TRUE;
	}
	case WM_WEBVIEW_KEYSTROKE:
	{
		HRESULT r = S_OK;
		
		IOleInPlaceActiveObject *ao = w->priv.client->activeObject;
		r = ao->TranslateAccelerator((MSG*)lParam);
		return r;
	}
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

#define WEBVIEW_KEY_FEATURE_BROWSER_EMULATION                                  \
  "Software\\Microsoft\\Internet "                                             \
  "Explorer\\Main\\FeatureControl\\FEATURE_BROWSER_EMULATION"

static int webview_fix_ie_compat_mode() {
	HKEY hKey;
	DWORD ie_version = 11000;
	TCHAR appname[MAX_PATH + 1];
	TCHAR *p;
	if (GetModuleFileName(NULL, appname, MAX_PATH + 1) == 0) {
		return -1;
	}
	for (p = &appname[strlen(appname) - 1]; p != appname && *p != '\\'; p--) {
	}
	p++;
	if (RegCreateKey(HKEY_CURRENT_USER, WEBVIEW_KEY_FEATURE_BROWSER_EMULATION, &hKey) != ERROR_SUCCESS) {
		return -1;
	}
	if (RegSetValueEx(hKey, p, 0, REG_DWORD, (BYTE *)&ie_version, sizeof(ie_version)) != ERROR_SUCCESS) {
		RegCloseKey(hKey);
		return -1;
	}
	RegCloseKey(hKey);
	return 0;
}

WEBVIEW_API int webview_init(webview_t *w) {
	HRESULT hr;

	WNDCLASSEX wc;
	HINSTANCE hInstance;
	DWORD style;
	RECT clientRect;
	RECT rect;

	if (webview_fix_ie_compat_mode() < 0) {
		return -1;
	}

	hInstance = GetModuleHandle(NULL);
	if (hInstance == NULL) {
		return -1;
	}

	hr = OleInitialize(NULL);
	if (hr != S_OK && hr != S_FALSE) {
		return -1;
	}

	if (!GetClassInfoEx(hInstance, classname, &wc)) {
		ZeroMemory(&wc, sizeof(WNDCLASSEX));
		wc.cbSize = sizeof(WNDCLASSEX);
		wc.hInstance = hInstance;
		wc.lpfnWndProc = wndproc;
		wc.lpszClassName = classname;
		RegisterClassEx(&wc);
	}

	style = WS_POPUP | WS_BORDER | WS_CAPTION | WS_SYSMENU;
	if (w->resizable) {
		style |= WS_THICKFRAME;
	}

	rect.left = 0;
	rect.top = 0;
	rect.right = w->width;
	rect.bottom = w->height;
	AdjustWindowRect(&rect, style, 0);

	GetClientRect(GetDesktopWindow(), &clientRect);
	int left = (clientRect.right / 2) - ((rect.right - rect.left) / 2);
	int top = (clientRect.bottom / 2) - ((rect.bottom - rect.top) / 2);
	rect.right = rect.right - rect.left + left;
	rect.left = left;
	rect.bottom = rect.bottom - rect.top + top;
	rect.top = top;

	w->priv.hwnd =
		CreateWindowEx(0, classname, w->title, style, 
			rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
			(HWND)w->parent, NULL, hInstance, (void *)w);
	if (w->priv.hwnd == 0) {
		return -1;
	}

	SetWindowLongPtr(w->priv.hwnd, GWLP_USERDATA, (LONG_PTR)w);

	DisplayHTMLPage(w);

	SetWindowText(w->priv.hwnd, w->title);
	ShowWindow(w->priv.hwnd, SW_SHOWDEFAULT);
	UpdateWindow(w->priv.hwnd);
	SetFocus(w->priv.hwnd);

	return 0;
}

WEBVIEW_API int webview_loop(webview_t *w, int blocking) {
	MSG msg;
	if (blocking) {
		GetMessage(&msg, w->priv.hwnd, 0, 0);
	}
	else {
		PeekMessage(&msg, w->priv.hwnd, 0, 0, PM_REMOVE);
	}
	switch (msg.message) {
	case WM_QUIT:
		return -1;
	case WM_COMMAND:
	case WM_KEYDOWN:
	case WM_KEYUP: {
		HRESULT r = S_OK;
		IOleObject *browser = w->priv.client->browser;
		IOleInPlaceActiveObject *pIOIPAO;
		if (browser->QueryInterface(iid_unref(&IID_IOleInPlaceActiveObject), (void **)&pIOIPAO) == S_OK) {
			r = pIOIPAO->TranslateAccelerator(&msg);
			pIOIPAO->Release();
		}
		if (r != S_FALSE) {
			break;
		}
	}
	default:
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return 0;
}

WEBVIEW_API int webview_eval(webview_t *w, const char *js) {
	IHTMLDocument2 *htmlDoc2;
	IDispatch *docDispatch;
	IDispatch *scriptDispatch;

	IWebBrowser2 *webBrowser2 = w->priv.client->webBrowser2;
	if (webBrowser2->get_Document(&docDispatch) != S_OK) {
		return -1;
	}
	if (docDispatch->QueryInterface(iid_unref(&IID_IHTMLDocument2), (void **)&htmlDoc2) != S_OK) {
		return -1;
	}
	if (htmlDoc2->get_Script(&scriptDispatch) != S_OK) {
		return -1;
	}
	DISPID dispid;
	BSTR evalStr = SysAllocString(L"eval");
	if (scriptDispatch->GetIDsOfNames(iid_unref(&IID_NULL), &evalStr, 1, LOCALE_SYSTEM_DEFAULT, &dispid) != S_OK) {
		SysFreeString(evalStr);
		return -1;
	}
	SysFreeString(evalStr);

	DISPPARAMS params;
	VARIANT arg;
	VARIANT result;
	EXCEPINFO excepInfo;
	UINT nArgErr = (UINT)-1;
	params.cArgs = 1;
	params.cNamedArgs = 0;
	params.rgvarg = &arg;
	arg.vt = VT_BSTR;
	static const char *prologue = "(function(){";
	static const char *epilogue = ";})();";
	int n = strlen(prologue) + strlen(epilogue) + strlen(js) + 1;
	char *eval = (char *)malloc(n);
	snprintf(eval, n, "%s%s%s", prologue, js, epilogue);
	wchar_t *buf = webview_to_utf16(eval);
	if (buf == NULL) {
		return -1;
	}
	arg.bstrVal = SysAllocString(buf);
	if (scriptDispatch->Invoke(dispid, iid_unref(&IID_NULL), 0, DISPATCH_METHOD, &params, 
		&result, &excepInfo, &nArgErr) != S_OK) {
		return -1;
	}
	SysFreeString(arg.bstrVal);
	free(eval);
	scriptDispatch->Release();
	htmlDoc2->Release();
	docDispatch->Release();
	return 0;
}

WEBVIEW_API void webview_dispatch(webview_t *w, webview_dispatch_fn fn, void *arg) {
	PostMessageW(w->priv.hwnd, WM_WEBVIEW_DISPATCH, (WPARAM)fn, (LPARAM)arg);
}

WEBVIEW_API void webview_set_title(webview_t *w, const char *title) {
	SetWindowText(w->priv.hwnd, title);
}

WEBVIEW_API void webview_set_fullscreen(webview_t *w, int fullscreen) {
	if (w->priv.is_fullscreen == !!fullscreen) {
		return;
	}
	if (w->priv.is_fullscreen == 0) {
		w->priv.saved_style = GetWindowLong(w->priv.hwnd, GWL_STYLE);
		w->priv.saved_ex_style = GetWindowLong(w->priv.hwnd, GWL_EXSTYLE);
		GetWindowRect(w->priv.hwnd, &w->priv.saved_rect);
	}
	w->priv.is_fullscreen = !!fullscreen;
	if (fullscreen) {
		MONITORINFO monitor_info;
		SetWindowLong(w->priv.hwnd, GWL_STYLE, w->priv.saved_style & ~(WS_CAPTION | WS_THICKFRAME));
		SetWindowLong(w->priv.hwnd, GWL_EXSTYLE, w->priv.saved_ex_style & ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));
		monitor_info.cbSize = sizeof(monitor_info);
		GetMonitorInfo(MonitorFromWindow(w->priv.hwnd, MONITOR_DEFAULTTONEAREST), &monitor_info);
		RECT r;
		r.left = monitor_info.rcMonitor.left;
		r.top = monitor_info.rcMonitor.top;
		r.right = monitor_info.rcMonitor.right;
		r.bottom = monitor_info.rcMonitor.bottom;
		SetWindowPos(w->priv.hwnd, NULL, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
	} else {
		SetWindowLong(w->priv.hwnd, GWL_STYLE, w->priv.saved_style);
		SetWindowLong(w->priv.hwnd, GWL_EXSTYLE, w->priv.saved_ex_style);
		SetWindowPos(w->priv.hwnd, NULL, w->priv.saved_rect.left, w->priv.saved_rect.top, w->priv.saved_rect.right - w->priv.saved_rect.left,
			w->priv.saved_rect.bottom - w->priv.saved_rect.top, SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
	}
}

WEBVIEW_API void webview_set_color(webview_t *w, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
	HBRUSH brush = CreateSolidBrush(RGB(r, g, b));
	SetClassLongPtr(w->priv.hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)brush);
}

WEBVIEW_API void webview_dialog(webview_t *w,
	enum webview_dialog_type dlgtype, int flags,
	const char *title, const char *arg,
	char *result, size_t resultsz) {
	if (dlgtype == WEBVIEW_DIALOG_TYPE_OPEN || dlgtype == WEBVIEW_DIALOG_TYPE_SAVE) {
		IFileDialog *dlg = NULL;
		IShellItem *res = NULL;
		WCHAR *ws = NULL;
		char *s = NULL;
		FILEOPENDIALOGOPTIONS opts, add_opts;
		if (dlgtype == WEBVIEW_DIALOG_TYPE_OPEN) {
			if (CoCreateInstance(iid_unref(&CLSID_FileOpenDialog), NULL, CLSCTX_INPROC_SERVER, iid_unref(&IID_IFileOpenDialog), (void **)&dlg) != S_OK) {
				goto error_dlg;
			}
			if (flags & WEBVIEW_DIALOG_FLAG_DIRECTORY) {
				add_opts |= FOS_PICKFOLDERS;
			}
			add_opts |= FOS_NOCHANGEDIR | FOS_ALLNONSTORAGEITEMS | FOS_NOVALIDATE | FOS_PATHMUSTEXIST | FOS_FILEMUSTEXIST | 
				FOS_SHAREAWARE | FOS_NOTESTFILECREATE | FOS_NODEREFERENCELINKS | FOS_FORCESHOWHIDDEN | FOS_DEFAULTNOMINIMODE;
		}
		else {
			if (CoCreateInstance(iid_unref(&CLSID_FileSaveDialog), NULL, CLSCTX_INPROC_SERVER, iid_unref(&IID_IFileSaveDialog), (void **)&dlg) != S_OK) {
				goto error_dlg;
			}
			add_opts |= FOS_OVERWRITEPROMPT | FOS_NOCHANGEDIR | FOS_ALLNONSTORAGEITEMS | FOS_NOVALIDATE | FOS_SHAREAWARE |
				FOS_NOTESTFILECREATE | FOS_NODEREFERENCELINKS | FOS_FORCESHOWHIDDEN | FOS_DEFAULTNOMINIMODE;
		}
		if (dlg->GetOptions(&opts) != S_OK) {
			goto error_dlg;
		}
		opts &= ~FOS_NOREADONLYRETURN;
		opts |= add_opts;
		if (dlg->SetOptions(opts) != S_OK) {
			goto error_dlg;
		}
		if (dlg->Show(w->priv.hwnd) != S_OK) {
			goto error_dlg;
		}
		if (dlg->GetResult(&res) != S_OK) {
			goto error_dlg;
		}
		if (res->GetDisplayName(SIGDN_FILESYSPATH, &ws) != S_OK) {
			goto error_result;
		}
		s = webview_from_utf16(ws);
		strncpy(result, s, resultsz);
		result[resultsz - 1] = '\0';
		CoTaskMemFree(ws);
	error_result:
		res->Release();
	error_dlg:
		dlg->Release();
		return;
	}
	else if (dlgtype == WEBVIEW_DIALOG_TYPE_ALERT) {
		UINT type = MB_OK;
		switch (flags & WEBVIEW_DIALOG_FLAG_ALERT_MASK) {
		case WEBVIEW_DIALOG_FLAG_INFO:
			type |= MB_ICONINFORMATION;
			break;
		case WEBVIEW_DIALOG_FLAG_WARNING:
			type |= MB_ICONWARNING;
			break;
		case WEBVIEW_DIALOG_FLAG_ERROR:
			type |= MB_ICONERROR;
			break;
		}
		MessageBoxA(w->priv.hwnd, arg, title, type);
	}
}

WEBVIEW_API void webview_terminate(webview_t *w) 
{
	PostQuitMessage(0);
}
WEBVIEW_API void webview_exit(webview_t *w) 
{
	if (w->priv.hwnd)DestroyWindow(w->priv.hwnd);
}
WEBVIEW_API void webview_print_log(const char *s) 
{
	OutputDebugStringA(s);
}

#endif /* WEBVIEW_WINAPI */