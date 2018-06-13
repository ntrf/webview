cmake_minimum_required(VERSION 2.8)

project(webview)

if(APPLE)
	set(WEBVIEW_COMPILE_DEFS "-DWEBVIEW_COCOA=1")
	set(WEBVIEW_COMPILE_OPTS "-ObjC")
	set(WEBVIEW_LIBS "-framework Cocoa" "-framework WebKit")
	set(WEBVIEW_PLATFORM_SRC "webview_cocoa.mm")
elseif(WIN32)
	set(WEBVIEW_COMPILE_DEFS "-DWEBVIEW_WINAPI=1")
	set(WEBVIEW_LIBS "ole32.lib" "comctl32.lib" "oleaut32.lib" "uuid.lib")
	set(WEBVIEW_PLATFORM_SRC "webview_winapi.cpp")
else()
	set(WEBVIEW_COMPILE_DEFS "-DWEBVIEW_GTK=1")
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
	pkg_check_modules(WEBKIT2 REQUIRED webkit2gtk-4.0)
	set(WEBVIEW_COMPILE_INCS ${GTK3_INCLUDE_DIRS} ${WEBKIT2_INCLUDE_DIRS} ${PROJECT_SOURCE_DIR})
	set(WEBVIEW_LIBS ${GTK3_LIBRARIES} ${WEBKIT2_LIBRARIES})
	set(WEBVIEW_PLATFORM_SRC "webview_gtk.cpp")
endif()

add_library(webview 
	${CMAKE_CURRENT_BINARY_DIR}/webview.c
	${WEBVIEW_PLATFORM_SRC}
	webview.h
)
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/webview.c "#define WEBVIEW_IMPLEMENTATION\n#include <webview.h>")
target_include_directories(webview PUBLIC ${PROJECT_SOURCE_DIR} ${WEBVIEW_COMPILE_INCS})
target_compile_definitions(webview PUBLIC ${WEBVIEW_COMPILE_DEFS})
target_compile_options(webview PRIVATE ${WEBVIEW_COMPILE_OPTS})
target_link_libraries(webview ${WEBVIEW_LIBS})

add_executable(webview_test MACOSX_BUNDLE webview_test.cc)
set_target_properties(webview_test PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
find_package(Threads)
if(APPLE)
	target_compile_options(webview_test PUBLIC "-ObjC++")
endif()
target_link_libraries(webview_test PRIVATE webview ${CMAKE_THREAD_LIBS_INIT})
enable_testing ()
add_test(NAME webview_test COMMAND webview_test)


add_executable(interg_test WIN32 test.cpp)
target_link_libraries(interg_test PRIVATE webview ${CMAKE_THREAD_LIBS_INIT})

