// +build ignore

#include <cassert>
#include <cstdio>
#include <functional>
#include <iostream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#define WEBVIEW_IMPLEMENTATION
#include "webview.h"

extern "C" void webview_dispatch_proxy(webview_t *w, void *arg) {
  (*static_cast<std::function<void(webview_t *)> *>(arg))(w);
}

class runner {
public:
  runner(webview_t *w) : w(w) { webview_init(this->w); }
  ~runner() { webview_exit(this->w); }
  runner &then(std::function<void(webview_t *w)> fn) {
    auto arg = new std::pair<std::function<void(webview_t *)>, void *>(
	fn, nullptr);
    this->queue.push_back([=](webview_t *w) {
      webview_dispatch(
	  w,
	  [](webview_t *w, void *arg) {
	    auto dispatch_arg = reinterpret_cast<
		std::pair<std::function<void(webview_t *)>, void *> *>(
		arg);
	    dispatch_arg->first(w);
	    delete dispatch_arg;
	  },
	  reinterpret_cast<void *>(arg));
    });
    return *this;
  }
  runner &sleep(const int millis) {
    this->queue.push_back([=](webview_t *w) {
      (void)w;
      std::this_thread::sleep_for(std::chrono::milliseconds(millis));
    });
    return *this;
  }
  void wait() {
    //this->then([](webview_t *w) { webview_terminate(w); });
    auto q = this->queue;
    auto w = this->w;
    std::thread bg_thread([w, q]() {
      for (auto f : q) {
	f(w);
      }
    });
    while (webview_loop(w, 1) == 0) {
    }
    bg_thread.join();
  }

private:
  webview_t *w;
  std::vector<std::function<void(webview_t *)>> queue;
};

static void test_minimal() {
  webview_t w = {};
  std::cout << "TEST: minimal" << std::endl;
  w.title = "Minimal test";
  w.width = 480;
  w.height = 320;
  webview_init(&w);
  webview_dispatch(&w,
		   [](webview_t *w, void *arg) {
		     (void)arg;
		     webview_terminate(w);
		   },
		   nullptr);
  while (webview_loop(&w, 1) == 0) {
  }
  webview_exit(&w);
}

static void test_window_size() {
  webview_t w = {};
  std::vector<std::string> results;
  std::cout << "TEST: window size" << std::endl;
  w.width = 480;
  w.height = 320;
  w.resizable = 1;
  w.userdata = static_cast<void *>(&results);
  w.external_invoke_cb = [](webview_t *w, const char *arg) {
    auto *v = static_cast<std::vector<std::string> *>(w->userdata);
    v->push_back(std::string(arg));
  };
  runner(&w)
      .then([](webview_t *w) {
	webview_eval(w, "window.external.invoke(''+window.screen.width+' ' + "
			"window.screen.height)");
	webview_eval(w, "window.external.invoke(''+window.innerWidth+' ' + "
			"window.innerHeight)");
      })
      .sleep(200)
      .then([](webview_t *w) { webview_set_fullscreen(w, 1); })
      .sleep(500)
      .then([](webview_t *w) {
	webview_eval(w, "window.external.invoke(''+window.innerWidth+' ' + "
			"window.innerHeight)");
      })
      .sleep(200)
      .then([](webview_t *w) { webview_set_fullscreen(w, 0); })
      .sleep(500)
      .then([](webview_t *w) {
	webview_eval(w, "window.external.invoke(''+window.innerWidth+' ' + "
			"window.innerHeight)");
      })
      .wait();
  assert(results.size() == 4);
  assert(results[1] == "480 320");
  assert(results[0] == results[2]);
  assert(results[1] == results[3]);
}

static void test_inject_js() {
  webview_t w = {};
  std::vector<std::string> results;
  std::cout << "TEST: inject JS" << std::endl;
  w.width = 480;
  w.height = 320;
  w.userdata = static_cast<void *>(&results);
  w.external_invoke_cb = [](webview_t *w, const char *arg) {
    auto *v = static_cast<std::vector<std::string> *>(w->userdata);
    v->push_back(std::string(arg));
  };
  runner(&w)
      .then([](webview_t *w) {
	webview_eval(w,
		     R"(document.body.innerHTML = '<div id="foo">Foo</div>';)");
	webview_eval(
	    w,
	    "window.external.invoke(document.getElementById('foo').innerText)");
      })
      .wait();
  assert(results.size() == 1);
  assert(results[0] == "Foo");
}

static void test_inject_css() {
  webview_t w = {};
  std::vector<std::string> results;
  std::cout << "TEST: inject CSS" << std::endl;
  w.width = 480;
  w.height = 320;
  w.userdata = static_cast<void *>(&results);
  w.external_invoke_cb = [](webview_t *w, const char *arg) {
    auto *v = static_cast<std::vector<std::string> *>(w->userdata);
    v->push_back(std::string(arg));
  };
  runner(&w)
      .then([](webview_t *w) {
	webview_inject_css(w, "#app { margin-left: 4px; }");
	webview_eval(w, "window.external.invoke(getComputedStyle(document."
			"getElementById('app')).marginLeft)");
      })
      .wait();
  assert(results.size() == 1);
  assert(results[0] == "4px");
}

int main() {
  test_minimal();
  test_window_size();
  test_inject_js();
  test_inject_css();
  return 0;
}
