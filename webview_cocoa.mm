#include <webview.h>

#if defined(WEBVIEW_COCOA)
#if (!defined MAC_OS_X_VERSION_10_12) ||                                       \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_12
#define NSAlertStyleWarning NSWarningAlertStyle
#define NSAlertStyleCritical NSCriticalAlertStyle
#define NSWindowStyleMaskResizable NSResizableWindowMask
#define NSWindowStyleMaskMiniaturizable NSMiniaturizableWindowMask
#define NSWindowStyleMaskTitled NSTitledWindowMask
#define NSWindowStyleMaskClosable NSClosableWindowMask
#define NSWindowStyleMaskFullScreen NSFullScreenWindowMask
#define NSEventMaskAny NSAnyEventMask
#define NSEventModifierFlagCommand NSCommandKeyMask
#define NSEventModifierFlagOption NSAlternateKeyMask
#define NSAlertStyleInformational NSInformationalAlertStyle
#endif /* MAC_OS_X_VERSION_10_12 */
#if (!defined MAC_OS_X_VERSION_10_13) ||                                       \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_13
#define NSModalResponseOK NSFileHandlingPanelOKButton
#endif /* MAC_OS_X_VERSION_10_12, MAC_OS_X_VERSION_10_13 */
static void webview_window_will_close(id self, SEL cmd, id notification) {
	webview_t *w =
		(webview_t *)objc_getAssociatedObject(self, "webview");
	webview_terminate(w);
}

static BOOL webview_is_selector_excluded_from_web_script(id self, SEL cmd,
	SEL selector) {
	return selector != @selector(invoke:);
}

static NSString *webview_webscript_name_for_selector(id self, SEL cmd,
	SEL selector) {
	return selector == @selector(invoke:) ? @"invoke" : nil;
}

static void webview_did_clear_window_object(id self, SEL cmd, id webview,
	id script, id frame) {
	[script setValue : self forKey : @"external"];
}

static void webview_external_invoke(id self, SEL cmd, id arg) {
	webview_t *w =
		(webview_t *)objc_getAssociatedObject(self, "webview");
	if (w == NULL || w->external_invoke_cb == NULL) {
		return;
	}
	if ([arg isKindOfClass : [NSString class]] == NO) {
		return;
	}
	w->external_invoke_cb(w, [(NSString *)(arg)UTF8String]);
}

WEBVIEW_API int webview_init(webview_t *w) {
	w->priv.pool = [[NSAutoreleasePool alloc] init];
	[NSApplication sharedApplication];

	Class webViewDelegateClass =
		objc_allocateClassPair([NSObject class], "WebViewDelegate", 0);
	class_addMethod(webViewDelegateClass, sel_registerName("windowWillClose:"),
		(IMP)webview_window_will_close, "v@:@");
	class_addMethod(object_getClass(webViewDelegateClass),
		sel_registerName("isSelectorExcludedFromWebScript:"),
		(IMP)webview_is_selector_excluded_from_web_script, "c@::");
	class_addMethod(object_getClass(webViewDelegateClass),
		sel_registerName("webScriptNameForSelector:"),
		(IMP)webview_webscript_name_for_selector, "c@::");
	class_addMethod(webViewDelegateClass,
		sel_registerName("webView:didClearWindowObject:forFrame:"),
		(IMP)webview_did_clear_window_object, "v@:@@@");
	class_addMethod(webViewDelegateClass, sel_registerName("invoke:"),
		(IMP)webview_external_invoke, "v@:@");
	objc_registerClassPair(webViewDelegateClass);

	w->priv.windowDelegate = [[webViewDelegateClass alloc] init];
	objc_setAssociatedObject(w->priv.windowDelegate, "webview", (id)(w),
		OBJC_ASSOCIATION_ASSIGN);

	NSString *nsTitle = [NSString stringWithUTF8String : w->title];
	NSRect r = NSMakeRect(0, 0, w->width, w->height);
	NSUInteger style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
		NSWindowStyleMaskMiniaturizable;
	if (w->resizable) {
		style = style | NSWindowStyleMaskResizable;
	}
	w->priv.window = [[NSWindow alloc] initWithContentRect:r
		styleMask : style
		backing : NSBackingStoreBuffered
		defer : NO];
	[w->priv.window autorelease];
	[w->priv.window setTitle : nsTitle];
	[w->priv.window setDelegate : w->priv.windowDelegate];
	[w->priv.window center];

	[[NSUserDefaults standardUserDefaults] setBool:!!w->debug
		forKey : @"WebKitDeveloperExtras"];
	[[NSUserDefaults standardUserDefaults] synchronize];
	w->priv.webview =
		[[WebView alloc] initWithFrame:r frameName : @"WebView" groupName : nil];
	NSURL *nsURL = [NSURL
		URLWithString : [NSString stringWithUTF8String : webview_check_url(w->url)]];
	[[w->priv.webview mainFrame] loadRequest:[NSURLRequest requestWithURL : nsURL]];

	[w->priv.webview setAutoresizesSubviews : YES];
	[w->priv.webview
		setAutoresizingMask : NSViewWidthSizable | NSViewHeightSizable];
	w->priv.webview.frameLoadDelegate = w->priv.windowDelegate;
	[[w->priv.window contentView] addSubview:w->priv.webview];
	[w->priv.window orderFrontRegardless];

	[NSApp setActivationPolicy : NSApplicationActivationPolicyRegular];
	[NSApp finishLaunching];
	[NSApp activateIgnoringOtherApps : YES];

	NSMenu *menubar = [[[NSMenu alloc] initWithTitle:@""] autorelease];

	NSString *appName = [[NSProcessInfo processInfo] processName];
	NSMenuItem *appMenuItem =
		[[[NSMenuItem alloc] initWithTitle:appName action : NULL keyEquivalent : @""]
		autorelease];
	NSMenu *appMenu = [[[NSMenu alloc] initWithTitle:appName] autorelease];
	[appMenuItem setSubmenu : appMenu];
	[menubar addItem : appMenuItem];

	NSString *title = [@"Hide " stringByAppendingString:appName];
		NSMenuItem *item = [[[NSMenuItem alloc] initWithTitle:title
		action : @selector(hide : )
				 keyEquivalent:@"h"] autorelease];
	[appMenu addItem : item];
	item = [[[NSMenuItem alloc] initWithTitle:@"Hide Others"
		action:@selector(hideOtherApplications : )
			   keyEquivalent:@"h"] autorelease];
	[item setKeyEquivalentModifierMask : (NSEventModifierFlagOption |
		NSEventModifierFlagCommand)];
	[appMenu addItem : item];
	item = [[[NSMenuItem alloc] initWithTitle:@"Show All"
		action:@selector(unhideAllApplications : )
			   keyEquivalent:@""] autorelease];
	[appMenu addItem : item];
	[appMenu addItem : [NSMenuItem separatorItem]];

	title = [@"Quit " stringByAppendingString:appName];
		item = [[[NSMenuItem alloc] initWithTitle:title
		action : @selector(terminate : )
				 keyEquivalent:@"q"] autorelease];
	[appMenu addItem : item];

	[NSApp setMainMenu : menubar];

	w->priv.should_exit = 0;
	return 0;
}

WEBVIEW_API int webview_loop(webview_t *w, int blocking) {
	NSDate *until = (blocking ? [NSDate distantFuture] : [NSDate distantPast]);
	NSEvent *event = [NSApp nextEventMatchingMask : NSEventMaskAny
		untilDate : until
		inMode : NSDefaultRunLoopMode
		dequeue : YES];
	if (event) {
		[NSApp sendEvent : event];
	}
	return w->priv.should_exit;
}

WEBVIEW_API int webview_eval(webview_t *w, const char *js) {
	NSString *nsJS = [NSString stringWithUTF8String : js];
	[[w->priv.webview windowScriptObject] evaluateWebScript:nsJS];
	return 0;
}

WEBVIEW_API void webview_set_title(webview_t *w, const char *title) {
	NSString *nsTitle = [NSString stringWithUTF8String : title];
	[w->priv.window setTitle : nsTitle];
}

WEBVIEW_API void webview_set_fullscreen(webview_t *w, int fullscreen) {
	int b = ((([w->priv.window styleMask] & NSWindowStyleMaskFullScreen) ==
		NSWindowStyleMaskFullScreen)
		? 1
		: 0);
	if (b != fullscreen) {
		[w->priv.window toggleFullScreen : nil];
	}
}

WEBVIEW_API void webview_set_color(webview_t *w, uint8_t r, uint8_t g,
	uint8_t b, uint8_t a) {
	[w->priv.window setBackgroundColor : [NSColor colorWithRed : (CGFloat)r / 255.0
		green : (CGFloat)g / 255.0
		blue : (CGFloat)b / 255.0
		alpha : (CGFloat)a / 255.0]];
	if (0.5 >= ((r / 255.0 * 299.0) + (g / 255.0 * 587.0) + (b / 255.0 * 114.0)) /
		1000.0) {
		[w->priv.window
			setAppearance : [NSAppearance
			appearanceNamed : NSAppearanceNameVibrantDark]];
	}
	else {
		[w->priv.window
			setAppearance : [NSAppearance
			appearanceNamed : NSAppearanceNameVibrantLight]];
	}
	[w->priv.window setOpaque : NO];
	[w->priv.window setTitlebarAppearsTransparent : YES];
	[w->priv.webview setDrawsBackground : NO];
}

WEBVIEW_API void webview_dialog(webview_t *w,
	enum webview_dialog_type dlgtype, int flags,
	const char *title, const char *arg,
	char *result, size_t resultsz) {
	if (dlgtype == WEBVIEW_DIALOG_TYPE_OPEN ||
		dlgtype == WEBVIEW_DIALOG_TYPE_SAVE) {
		NSSavePanel *panel;
		if (dlgtype == WEBVIEW_DIALOG_TYPE_OPEN) {
			NSOpenPanel *openPanel = [NSOpenPanel openPanel];
			if (flags & WEBVIEW_DIALOG_FLAG_DIRECTORY) {
				[openPanel setCanChooseFiles : NO];
				[openPanel setCanChooseDirectories : YES];
			}
			else {
				[openPanel setCanChooseFiles : YES];
				[openPanel setCanChooseDirectories : NO];
			}
			[openPanel setResolvesAliases : NO];
			[openPanel setAllowsMultipleSelection : NO];
			panel = openPanel;
		}
		else {
			panel = [NSSavePanel savePanel];
		}
		[panel setCanCreateDirectories : YES];
		[panel setShowsHiddenFiles : YES];
		[panel setExtensionHidden : NO];
		[panel setCanSelectHiddenExtension : NO];
		[panel setTreatsFilePackagesAsDirectories : YES];
		[panel beginSheetModalForWindow : w->priv.window
			completionHandler : ^ (NSInteger result) {
			[NSApp stopModalWithCode : result];
		}];
		if ([NSApp runModalForWindow : panel] == NSModalResponseOK) {
			const char *filename = [[[panel URL] path] UTF8String];
			strlcpy(result, filename, resultsz);
		}
	}
	else if (dlgtype == WEBVIEW_DIALOG_TYPE_ALERT) {
		NSAlert *a = [NSAlert new];
		switch (flags & WEBVIEW_DIALOG_FLAG_ALERT_MASK) {
		case WEBVIEW_DIALOG_FLAG_INFO:
			[a setAlertStyle : NSAlertStyleInformational];
			break;
		case WEBVIEW_DIALOG_FLAG_WARNING:
			NSLog(@"warning");
			[a setAlertStyle : NSAlertStyleWarning];
			break;
		case WEBVIEW_DIALOG_FLAG_ERROR:
			NSLog(@"error");
			[a setAlertStyle : NSAlertStyleCritical];
			break;
		}
		[a setShowsHelp : NO];
		[a setShowsSuppressionButton : NO];
		[a setMessageText : [NSString stringWithUTF8String : title]];
		[a setInformativeText : [NSString stringWithUTF8String : arg]];
		[a addButtonWithTitle : @"OK"];
		[a runModal];
		[a release];
	}
}

static void webview_dispatch_cb(void *arg) {
	webview_t_dispatch_arg *context = (webview_t_dispatch_arg *)arg;
	(context->fn)(context->w, context->arg);
	free(context);
}

WEBVIEW_API void webview_dispatch(webview_t *w, webview_dispatch_fn fn, void *arg) {
	webview_t_dispatch_arg *context = (webview_t_dispatch_arg *)malloc(sizeof(webview_t_dispatch_arg));
	context->w = w;
	context->arg = arg;
	context->fn = fn;
	dispatch_async_f(dispatch_get_main_queue(), context, webview_dispatch_cb);
}

WEBVIEW_API void webview_terminate(webview_t *w) {
	w->priv.should_exit = 1;
}
WEBVIEW_API void webview_exit(webview_t *w) { 
	[NSApp terminate : NSApp]; 
}
WEBVIEW_API void webview_print_log(const char *s) { 
	NSLog(@"%s", s); 
}

#endif /* WEBVIEW_COCOA */