/*
 * MIT License
 *
 * Copyright (c) 2017 Serge Zaitsev
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef WEBVIEW_H
#define WEBVIEW_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef WEBVIEW_STATIC
#define WEBVIEW_API static
#else
#define WEBVIEW_API extern
#endif

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if defined(WEBVIEW_GTK)
#include <JavaScriptCore/JavaScript.h>
#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

	struct webview_priv {
		GtkWidget *window;
		GtkWidget *scroller;
		GtkWidget *webview;
		GtkWidget *inspector_window;
		GAsyncQueue *queue;
		int ready;
		int js_busy;
		int should_exit;
	};
#elif defined(WEBVIEW_WINAPI)
#include <windows.h>

#include <stdio.h>

	struct webview_priv {
		HWND hwnd;
		struct WebViewClient * client;
		BOOL is_fullscreen;
		DWORD saved_style;
		DWORD saved_ex_style;
		RECT saved_rect;
	};

#undef WEBVIEW_API
#ifdef WEBVIEW_EXPORT
#define WEBVIEW_API __declspec(dllexport)
#else
#define WEBVIEW_API __declspec(dllimport)
#endif

#elif defined(WEBVIEW_COCOA)
#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>
#import <objc/runtime.h>

	struct webview_priv {
		NSAutoreleasePool *pool;
		NSWindow *window;
		WebView *webview;
		id windowDelegate;
		int should_exit;
	};
#else
#error "Define one of: WEBVIEW_GTK, WEBVIEW_COCOA or WEBVIEW_WINAPI"
#endif

	struct webview_s;

	typedef struct webview_s webview_t;

	typedef void(*webview_external_invoke_cb_t)(webview_t *w,
		const char *arg);

	struct webview_s {
		void *parent;
		const char *url;
		const char *title;
		int width;
		int height;
		int resizable;
		int debug;
		webview_external_invoke_cb_t external_invoke_cb;
		struct webview_priv priv;
		void *userdata;
	};

	enum webview_dialog_type {
		WEBVIEW_DIALOG_TYPE_OPEN = 0,
		WEBVIEW_DIALOG_TYPE_SAVE = 1,
		WEBVIEW_DIALOG_TYPE_ALERT = 2
	};

#define WEBVIEW_DIALOG_FLAG_FILE (0 << 0)
#define WEBVIEW_DIALOG_FLAG_DIRECTORY (1 << 0)

#define WEBVIEW_DIALOG_FLAG_INFO (1 << 1)
#define WEBVIEW_DIALOG_FLAG_WARNING (2 << 1)
#define WEBVIEW_DIALOG_FLAG_ERROR (3 << 1)
#define WEBVIEW_DIALOG_FLAG_ALERT_MASK (3 << 1)

	typedef void(*webview_dispatch_fn)(webview_t *w, void *arg);

	struct webview_dispatch_arg {
		webview_dispatch_fn fn;
		webview_t *w;
		void *arg;
	};

#define DEFAULT_URL "about:blank"

#define CSS_INJECT_FUNCTION                                                    \
  "(function(e){var "                                                          \
  "t=document.createElement('style'),d=document.head||document."               \
  "getElementsByTagName('head')[0];t.setAttribute('type','text/"               \
  "css'),t.styleSheet?t.styleSheet.cssText=e:t.appendChild(document."          \
  "createTextNode(e)),d.appendChild(t)})"

	static const char *webview_check_url(const char *url) {
		if (url == NULL || strlen(url) == 0) {
			return DEFAULT_URL;
		}
		return url;
	}

	WEBVIEW_API int webview(const char *title, const char *url, int width,
		int height, int resizable);

	WEBVIEW_API int webview_init(webview_t *w);
	WEBVIEW_API int webview_loop(webview_t *w, int blocking);
	WEBVIEW_API int webview_eval(webview_t *w, const char *js);
	WEBVIEW_API int webview_inject_css(webview_t *w, const char *css);
	WEBVIEW_API void webview_set_title(webview_t *w, const char *title);
	WEBVIEW_API void webview_set_fullscreen(webview_t *w, int fullscreen);
	WEBVIEW_API void webview_set_color(webview_t *w, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
	WEBVIEW_API void webview_dialog(webview_t *w, enum webview_dialog_type dlgtype, int flags, 
		const char *title, const char *arg, char *result, size_t resultsz);
	WEBVIEW_API void webview_dispatch(webview_t *w, webview_dispatch_fn fn, void *arg);
	WEBVIEW_API void webview_terminate(webview_t *w);
	WEBVIEW_API void webview_exit(webview_t *w);
	WEBVIEW_API void webview_debug(const char *format, ...);
	WEBVIEW_API void webview_print_log(const char *s);

#ifdef __cplusplus
}
#endif


#endif /* WEBVIEW_H */
