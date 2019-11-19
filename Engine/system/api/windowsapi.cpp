#include "windowsapi.h"

#ifdef __WINDOWS__

#include <Tempest/Event>
#include <Tempest/Except>

#include "system/eventdispatcher.h"

#include <atomic>
#include <unordered_set>
#include <thread>
#include <vector>

#include <windows.h>

using namespace Tempest;

struct WindowsApi::KeyInf {
  std::vector<WindowsApi::TranslateKeyPair> keys;
  std::vector<WindowsApi::TranslateKeyPair> a, k0, f1;
  uint16_t                                  fkeysCount=1;
  };

static const wchar_t*                         wndClassName=L"Tempest.Window";
static std::unordered_set<SystemApi::Window*> windows;
WindowsApi::KeyInf                            WindowsApi::ki;
static std::atomic_bool                       isExit{0};

static int getIntParam(DWORD_PTR v){
  if(v>std::numeric_limits<int16_t>::max())
    return int(v)-std::numeric_limits<uint16_t>::max();
  return int(v);
  }

static int getX_LPARAM(LPARAM lp) {
  DWORD_PTR x = (DWORD_PTR(lp)) & 0xffff;
  return getIntParam(x);
  }

static int getY_LPARAM(LPARAM lp) {
  DWORD_PTR y = ((DWORD_PTR(lp)) >> 16) & 0xffff;
  return getIntParam(y);
  }

static WORD get_Button_WPARAM(WPARAM lp) {
  return ((DWORD_PTR(lp)) >> 16) & 0xffff;
  }

static Event::MouseButton toButton( UINT msg, DWORD wParam ){
  if( msg==WM_LBUTTONDOWN ||
      msg==WM_LBUTTONUP )
    return Event::ButtonLeft;

  if( msg==WM_RBUTTONDOWN  ||
      msg==WM_RBUTTONUP)
    return Event::ButtonRight;

  if( msg==WM_MBUTTONDOWN ||
      msg==WM_MBUTTONUP )
    return Event::ButtonMid;

  if( msg==WM_XBUTTONDOWN ||
      msg==WM_XBUTTONUP ) {
    const WORD btn = get_Button_WPARAM(wParam);
    if(btn==XBUTTON1)
      return Event::ButtonBack;
    if(btn==XBUTTON2)
      return Event::ButtonForward;
    }

  return Event::ButtonNone;
  }

WindowsApi::WindowsApi() {
  static const TranslateKeyPair k[] = {
    { VK_LCONTROL, Event::K_Control },
    { VK_RCONTROL, Event::K_Control },
    { VK_CONTROL,  Event::K_Control },

    { VK_LEFT,     Event::K_Left    },
    { VK_RIGHT,    Event::K_Right   },
    { VK_UP,       Event::K_Up      },
    { VK_DOWN,     Event::K_Down    },

    { VK_ESCAPE,   Event::K_ESCAPE  },
    { VK_BACK,     Event::K_Back    },
    { VK_TAB,      Event::K_Tab     },
    { VK_SHIFT,    Event::K_Shift   },
    { VK_DELETE,   Event::K_Delete  },
    { VK_INSERT,   Event::K_Insert  },
    { VK_HOME,     Event::K_Home    },
    { VK_END,      Event::K_End     },
    { VK_PAUSE,    Event::K_Pause   },
    { VK_RETURN,   Event::K_Return  },
    { VK_SPACE,    Event::K_Space   },

    { VK_F1,       Event::K_F1 },
    { 0x30,        Event::K_0  },
    { 0x41,        Event::K_A  },

    { 0,         Event::K_NoKey }
    };

  setupKeyTranslate(k,24);

  struct Private {
    static LRESULT CALLBACK wProc(HWND hWnd,UINT msg,const WPARAM wParam,const LPARAM lParam ){
      return WindowsApi::windowProc(hWnd,msg,wParam,lParam);
      }
    };

  WNDCLASSEXW winClass={};

  // Initialize the window class structure:
  winClass.lpszClassName = wndClassName;
  winClass.cbSize        = sizeof(WNDCLASSEX);
  winClass.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
  winClass.lpfnWndProc   = Private::wProc;
  winClass.hInstance     = GetModuleHandle(nullptr);
  winClass.hIcon         = LoadIcon( GetModuleHandle(nullptr), LPCTSTR(MAKEINTRESOURCE(32512)) );
  winClass.hIconSm       = LoadIcon( GetModuleHandle(nullptr), LPCTSTR(MAKEINTRESOURCE(32512)) );
  winClass.hCursor       = LoadCursor(nullptr, IDC_ARROW);
  winClass.hbrBackground = nullptr;// (HBRUSH)GetStockObject(BLACK_BRUSH);
  winClass.lpszMenuName  = nullptr;
  winClass.cbClsExtra    = 0;
  winClass.cbWndExtra    = 0;

  // Register window class:
  if(!RegisterClassExW(&winClass)) {
    throw std::system_error(Tempest::SystemErrc::InvalidWindowClass);
    }
  }

SystemApi::Window *WindowsApi::implCreateWindow(SystemApi::WindowCallback *callback, uint32_t width, uint32_t height) {
  // Create window with the registered class:
  RECT wr = {0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
  AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);
  HWND window = CreateWindowExW(0,
                                wndClassName,          // class name
                                nullptr,               // app name
                                WS_OVERLAPPEDWINDOW |  // window style
                                WS_VISIBLE | WS_SYSMENU,
                                0, 0,                  // x/y coords
                                wr.right  - wr.left,   // width
                                wr.bottom - wr.top,    // height
                                nullptr,               // handle to parent
                                nullptr,               // handle to menu
                                GetModuleHandle(nullptr),          // hInstance
                                nullptr);              // no extra parameters

  if( !window )
    throw std::system_error(Tempest::SystemErrc::UnableToCreateWindow);

  SetWindowLongPtr(window,GWLP_USERDATA,LONG_PTR(callback));
  //minsize.x = GetSystemMetrics(SM_CXMINTRACK);
  //minsize.y = GetSystemMetrics(SM_CYMINTRACK) + 1;
  Window* wx = reinterpret_cast<Window*>(window);
  try {
    windows.insert(wx);
    }
  catch(...){
    destroyWindow(wx);
    throw;
    }
  return wx;
  }

SystemApi::Window *WindowsApi::implCreateWindow(SystemApi::WindowCallback *cb,ShowMode sm) {
  SystemApi::Window* hwnd = nullptr;
  if(sm==Maximized) {
    int w = GetSystemMetrics(SM_CXFULLSCREEN),
        h = GetSystemMetrics(SM_CYFULLSCREEN);
    hwnd = createWindow(cb,uint32_t(w),uint32_t(h));
    ShowWindow(HWND(hwnd),SW_MAXIMIZE);
    }
  else if(sm==Minimized) {
    hwnd =  createWindow(cb,800,600);
    ShowWindow(HWND(hwnd),SW_MINIMIZE);
    }
  else {
    hwnd =  createWindow(cb,800,600);
    ShowWindow(HWND(hwnd),SW_NORMAL);
    }
  // TODO: fullscreen
  return hwnd;
  }

void WindowsApi::implDestroyWindow(SystemApi::Window *w) {
  windows.erase(w);
  DestroyWindow(HWND(w));
  }

void WindowsApi::implExit() {
  isExit.store(true);
  }

uint16_t WindowsApi::translateKey(uint64_t scancode) {
  for(size_t i=0; i<ki.keys.size(); ++i)
    if( ki.keys[i].src==scancode )
      return ki.keys[i].result;

  for(size_t i=0; i<ki.k0.size(); ++i)
    if( ki.k0[i].src<=scancode &&
                      scancode<=ki.k0[i].src+9 ){
      auto dx = ( scancode-ki.k0[i].src );
      return Event::KeyType( ki.k0[i].result + dx );
      }

  uint16_t literalsCount = (Event::K_Z - Event::K_A);
  for(size_t i=0; i<ki.a.size(); ++i)
    if(ki.a[i].src<=scancode &&
                    scancode<=ki.a[i].src+literalsCount ){
      auto dx = ( scancode-ki.a[i].src );
      return Event::KeyType( ki.a[i].result + dx );
      }

  for(size_t i=0; i<ki.f1.size(); ++i)
    if(ki.f1[i].src<=scancode &&
                     scancode<=ki.f1[i].src+ki.fkeysCount ){
      auto dx = (scancode-ki.f1[i].src);
      return Event::KeyType(ki.f1[i].result+dx);
      }

  return Event::K_NoKey;
  }

void WindowsApi::setupKeyTranslate(const TranslateKeyPair k[], uint16_t funcCount ) {
  ki.keys.clear();
  ki.a. clear();
  ki.k0.clear();
  ki.f1.clear();
  ki.fkeysCount = funcCount;

  for( size_t i=0; k[i].result!=Event::K_NoKey; ++i ){
    if( k[i].result==Event::K_A )
      ki.a.push_back(k[i]); else
    if( k[i].result==Event::K_0 )
      ki.k0.push_back(k[i]); else
    if( k[i].result==Event::K_F1 )
      ki.f1.push_back(k[i]); else
      ki.keys.push_back( k[i] );
    }

#ifndef __ANDROID__
  ki.keys.shrink_to_fit();
  ki.a. shrink_to_fit();
  ki.k0.shrink_to_fit();
  ki.f1.shrink_to_fit();
#endif
  }

int WindowsApi::implExec(AppCallBack& cb) {
  // main message loop
  while (!isExit.load()) {
    MSG msg={};
    if(PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
      if( msg.message==WM_QUIT ) { // check for a quit message
        isExit.store(true);  // if found, quit app
        } else {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        }
      std::this_thread::yield();
      } else {
      if(cb.onTimer()==0)
        Sleep(1);
      for(auto& i:windows){
        HWND h = HWND(i);
        SystemApi::WindowCallback* cb=reinterpret_cast<SystemApi::WindowCallback*>(GetWindowLongPtr(h,GWLP_USERDATA));
        if(cb)
          cb->onRender(reinterpret_cast<SystemApi::Window*>(h));
        }
      }
    }
  return 0;
  }

uint32_t WindowsApi::implWidth(SystemApi::Window *w) {
  RECT rect={};
  GetClientRect(HWND(w),&rect);
  return uint32_t(rect.right-rect.left);
  }

uint32_t WindowsApi::implHeight(SystemApi::Window *w) {
  RECT rect={};
  GetClientRect(HWND(w),&rect);
  return uint32_t(rect.bottom-rect.top);
  }

Rect WindowsApi::implWindowClientRect(SystemApi::Window* w) {
  RECT rectWindow;
  GetClientRect( HWND(w), &rectWindow);
  int cW = rectWindow.right  - rectWindow.left;
  int cH = rectWindow.bottom - rectWindow.top;

  return Rect(rectWindow.left,rectWindow.top,cW,cH);
  }

bool WindowsApi::implSetAsFullscreen(SystemApi::Window *wx,bool fullScreen) {
  HWND hwnd = HWND(wx);
  if(fullScreen==isFullscreen(wx))
    return true;

  const int w = GetSystemMetrics(SM_CXSCREEN);
  const int h = GetSystemMetrics(SM_CYSCREEN);
  if(fullScreen) {
    //show full-screen
    SetWindowLongPtr(hwnd, GWL_STYLE, WS_VISIBLE | WS_POPUP);
    SetWindowPos    (hwnd, HWND_TOP, 0, 0, w, h, SWP_FRAMECHANGED);
    ShowWindow      (hwnd, SW_SHOW);
    } else {
    SetWindowLongPtr(hwnd, GWL_STYLE, WS_VISIBLE | WS_OVERLAPPEDWINDOW);
    SetWindowPos    (hwnd, nullptr, 0, 0, w, h, SWP_FRAMECHANGED);
    ShowWindow      (hwnd, SW_MAXIMIZE);
    }
  return true;
  }

bool WindowsApi::implIsFullscreen(SystemApi::Window *w) {
  HWND hwnd = HWND(w);
  return ULONG(GetWindowLongPtr(hwnd, GWL_STYLE)) & WS_POPUP;
  }

void WindowsApi::implSetCursorPosition(int x, int y) {
  SetCursorPos(x,y);
  }

void WindowsApi::implShowCursor(bool show) {
  ShowCursor(show ? TRUE : FALSE);
  }

long WindowsApi::windowProc(void *_hWnd, uint32_t msg, const uint32_t wParam, const long lParam) {
  HWND hWnd = HWND(_hWnd);

  SystemApi::WindowCallback* cb=reinterpret_cast<SystemApi::WindowCallback*>(GetWindowLongPtr(hWnd,GWLP_USERDATA));
  switch( msg ) {
    case WM_PAINT:{
      cb->onRender(reinterpret_cast<SystemApi::Window*>(hWnd));
      break;
      }

    case WM_CLOSE:{
      PostQuitMessage(0);
      break;
      }

    case WM_DESTROY: {
      PostQuitMessage(0);
      break;
      }

    case WM_XBUTTONDOWN:
    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN: {
      SetCapture(hWnd);
      if(cb) {
        MouseEvent e( getX_LPARAM(lParam),
                      getY_LPARAM(lParam),
                      toButton(msg,wParam),
                      0,
                      0,
                      Event::MouseDown );
        SystemApi::dispatchMouseDown(*cb,e);
        }
      break;
      }

    case WM_XBUTTONUP:
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP: {
      ReleaseCapture();
      if(cb) {
        MouseEvent e( getX_LPARAM(lParam),
                      getY_LPARAM(lParam),
                      toButton(msg,wParam),
                      0,
                      0,
                      Event::MouseUp  );
        SystemApi::dispatchMouseUp(*cb,e);
        }
      break;
      }

    case WM_MOUSEMOVE: {
      if(cb) {
        MouseEvent e0( getX_LPARAM(lParam),
                       getY_LPARAM(lParam),
                       Event::ButtonNone,
                       0,
                       0,
                       Event::MouseDrag  );
        SystemApi::dispatchMouseMove(*cb,e0);
        }
      break;
      }
    case WM_MOUSEWHEEL:{
      if(cb) {
        POINT p;
        p.x = getX_LPARAM(lParam);
        p.y = getY_LPARAM(lParam);

        ScreenToClient(hWnd, &p);
        Tempest::MouseEvent e( p.x, p.y,
                               Tempest::Event::ButtonNone,
                               GET_WHEEL_DELTA_WPARAM(wParam),
                               0,
                               Event::MouseWheel );
        SystemApi::dispatchMouseWheel(*cb,e);
        }
      break;
      }

    case WM_KEYDOWN: {
      if(cb) {
        auto key = WindowsApi::translateKey(wParam);
        Tempest::KeyEvent e(Event::KeyType(key),Event::KeyDown);
        SystemApi::dispatchKeyDown(*cb,e);
        }
      break;
      }

    case WM_KEYUP: {
      if(cb) {
        auto key = WindowsApi::translateKey(wParam);
        Tempest::KeyEvent e(Event::KeyType(key),Event::KeyUp);
        SystemApi::dispatchKeyUp(*cb,e);
        }
      break;
      }

    case WM_SIZE:
      if(wParam!=SIZE_MINIMIZED) {
        // RECT rpos = {0,0,0,0};
        // GetWindowRect( hWnd, &rpos );

        // RECT rectWindow;
        // GetClientRect( HWND(hWnd), &rectWindow);
        // int cW = rectWindow.right  - rectWindow.left;
        // int cH = rectWindow.bottom - rectWindow.top;

        int width  = lParam & 0xffff;
        int height = (uint32_t(lParam) & 0xffff0000) >> 16;
        if(cb)
          cb->onResize(reinterpret_cast<SystemApi::Window*>(hWnd),width,height);
        }
      break;
    }
  return DefWindowProc( hWnd, msg, wParam, lParam );
  }

#endif