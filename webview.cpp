#include "webview.h"

int webview(const char *title, const char *url, int width,
	int height, int resizable) {
	webview_t webview;
	memset(&webview, 0, sizeof(webview));
	webview.title = title;
	webview.url = url;
	webview.width = width;
	webview.height = height;
	webview.resizable = resizable;
	int r = webview_init(&webview);
	if (r != 0) {
		return r;
	}
	while (webview_loop(&webview, 1) == 0) {
	}
	webview_exit(&webview);
	return 0;
}

void webview_debug(const char *format, ...) {
	char buf[4096];
	va_list ap;
	va_start(ap, format);
	vsnprintf(buf, sizeof(buf), format, ap);
	webview_print_log(buf);
	va_end(ap);
}

static int webview_js_encode(const char *s, char *esc, size_t n) {
	int r = 1; /* At least one byte for trailing zero */
	for (; *s; s++) {
		const unsigned char c = *s;
		if (c >= 0x20 && c < 0x80 && strchr("<>\\'\"", c) == NULL) {
			if (n > 0) {
				*esc++ = c;
				n--;
			}
			r++;
		}
		else {
			if (n > 0) {
				snprintf(esc, n, "\\x%02x", (int)c);
				esc += 4;
				n -= 4;
			}
			r += 4;
		}
	}
	return r;
}

WEBVIEW_API int webview_inject_css(webview_t *w, const char *css) {
	int n = webview_js_encode(css, NULL, 0);
	char *esc = (char *)calloc(1, sizeof(CSS_INJECT_FUNCTION) + n + 4);
	if (esc == NULL) {
		return -1;
	}
	char *js = (char *)calloc(1, n);
	webview_js_encode(css, js, n);
	snprintf(esc, sizeof(CSS_INJECT_FUNCTION) + n + 4, "%s(\"%s\")",
		CSS_INJECT_FUNCTION, js);
	int r = webview_eval(w, esc);
	free(js);
	free(esc);
	return r;
}

