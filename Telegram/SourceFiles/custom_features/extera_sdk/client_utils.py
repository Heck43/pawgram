"""
exteraGram Desktop SDK - Client Utilities
Provides helpers for interacting with the client and displaying in-app UI.
"""

import ctypes
from ctypes import wintypes
import os
import sys
import logging
import threading
import time

logger = logging.getLogger("client_utils")

# Win32 GDI structures for native in-app Bulletin rendering
HANDLE = wintypes.HANDLE
HWND = wintypes.HWND
HDC = wintypes.HDC
BOOL = wintypes.BOOL
UINT = wintypes.UINT
WPARAM = wintypes.WPARAM
LPARAM = wintypes.LPARAM
LPCWSTR = wintypes.LPCWSTR

LRESULT = ctypes.c_longlong
WNDPROC = ctypes.WINFUNCTYPE(LRESULT, HWND, UINT, WPARAM, LPARAM)

ctypes.windll.user32.DefWindowProcW.argtypes = [HWND, UINT, WPARAM, LPARAM]
ctypes.windll.user32.DefWindowProcW.restype = LRESULT

class WNDCLASSEXW(ctypes.Structure):
    _fields_ = [
        ('cbSize', UINT), ('style', UINT), ('lpfnWndProc', WNDPROC),
        ('cbClsExtra', ctypes.c_int), ('cbWndExtra', ctypes.c_int),
        ('hInstance', HANDLE), ('hIcon', HANDLE), ('hCursor', HANDLE),
        ('hbrBackground', HANDLE), ('lpszMenuName', LPCWSTR),
        ('lpszClassName', LPCWSTR), ('hIconSm', HANDLE)
    ]

class RECT(ctypes.Structure):
    _fields_ = [('left', wintypes.LONG), ('top', wintypes.LONG), ('right', wintypes.LONG), ('bottom', wintypes.LONG)]

class PAINTSTRUCT(ctypes.Structure):
    _fields_ = [('hdc', HDC), ('fErase', BOOL), ('rcPaint', RECT), ('fRestore', BOOL), ('fIncUpdate', BOOL), ('rgbReserved', ctypes.c_byte * 32)]

class POINT(ctypes.Structure):
    _fields_ = [('x', wintypes.LONG), ('y', wintypes.LONG)]

_bulletin_class_atom = None
_bulletin_lock = threading.Lock()
_current_bulletin_hwnd = None

def _bulletin_wnd_proc(hwnd, msg, wparam, lparam):
    global _current_bulletin_hwnd
    user32 = ctypes.windll.user32
    gdi32 = ctypes.windll.gdi32

    WM_PAINT = 0x000F
    WM_LBUTTONUP = 0x0202
    WM_TIMER = 0x0113
    WM_DESTROY = 0x0002

    if msg == WM_PAINT:
        ps = PAINTSTRUCT()
        hdc = user32.BeginPaint(hwnd, ctypes.byref(ps))
        rc = RECT()
        user32.GetClientRect(hwnd, ctypes.byref(rc))

        w = rc.right - rc.left
        h = rc.bottom - rc.top

        mem_dc = gdi32.CreateCompatibleDC(hdc)
        mem_bmp = gdi32.CreateCompatibleBitmap(hdc, w, h)
        old_bmp = gdi32.SelectObject(mem_dc, mem_bmp)

        # Background capsule: dark charcoal (#28242C)
        bg_brush = gdi32.CreateSolidBrush(0x002C2428) # BGR
        border_pen = gdi32.CreatePen(0, 1, 0x00453842)
        old_brush = gdi32.SelectObject(mem_dc, bg_brush)
        old_pen = gdi32.SelectObject(mem_dc, border_pen)
        gdi32.RoundRect(mem_dc, 0, 0, w, h, h, h)

        # Left avatar circle: vibrant blue/purple (#3036C0)
        icon_size = 38
        icon_x = 10
        icon_y = (h - icon_size) // 2
        icon_brush = gdi32.CreateSolidBrush(0x00C03630) # BGR
        gdi32.SelectObject(mem_dc, icon_brush)
        gdi32.Ellipse(mem_dc, icon_x, icon_y, icon_x + icon_size, icon_y + icon_size)

        # Chat bubble icon inside circle
        white_brush = gdi32.CreateSolidBrush(0x00FFFFFF)
        white_pen = gdi32.CreatePen(0, 1, 0x00FFFFFF)
        gdi32.SelectObject(mem_dc, white_brush)
        gdi32.SelectObject(mem_dc, white_pen)
        gdi32.RoundRect(mem_dc, icon_x + 9, icon_y + 10, icon_x + 29, icon_y + 25, 6, 6)

        tri = (POINT * 3)(POINT(icon_x + 13, icon_y + 23), POINT(icon_x + 10, icon_y + 28), POINT(icon_x + 18, icon_y + 23))
        gdi32.Polygon(mem_dc, tri, 3)

        gdi32.DeleteObject(white_brush)
        gdi32.DeleteObject(white_pen)
        gdi32.DeleteObject(icon_brush)

        # Text rendering
        gdi32.SetBkMode(mem_dc, 1) # TRANSPARENT
        text_left = icon_x + icon_size + 14
        text_right = w - 110

        title = getattr(_bulletin_wnd_proc, "_title", "Уведомление")
        subtitle = getattr(_bulletin_wnd_proc, "_subtitle", "")

        # Title font (Bold)
        title_font = gdi32.CreateFontW(-14, 0, 0, 0, 700, 0, 0, 0, 1, 0, 0, 5, 0, "Segoe UI")
        old_font = gdi32.SelectObject(mem_dc, title_font)
        gdi32.SetTextColor(mem_dc, 0x00FFFFFF)
        rc_title = RECT(text_left, 10 if subtitle else (h - 18) // 2, text_right, 30 if subtitle else h)
        user32.DrawTextW(mem_dc, title, -1, ctypes.byref(rc_title), 0x00000020 | 0x00000004) # DT_SINGLELINE | DT_NOPREFIX

        # Subtitle font (Regular)
        if subtitle:
            sub_font = gdi32.CreateFontW(-13, 0, 0, 0, 400, 0, 0, 0, 1, 0, 0, 5, 0, "Segoe UI")
            gdi32.SelectObject(mem_dc, sub_font)
            gdi32.SetTextColor(mem_dc, 0x00B8AAB0) # Muted gray
            rc_sub = RECT(text_left, 30, text_right, 50)
            user32.DrawTextW(mem_dc, subtitle, -1, ctypes.byref(rc_sub), 0x00000020 | 0x00000004)
            gdi32.DeleteObject(sub_font)

        # Right action button: "Открыть" in Telegram blue
        btn_font = gdi32.CreateFontW(-14, 0, 0, 0, 600, 0, 0, 0, 1, 0, 0, 5, 0, "Segoe UI")
        gdi32.SelectObject(mem_dc, btn_font)
        gdi32.SetTextColor(mem_dc, 0x00FFA658) # Telegram Blue (#58A6FF in BGR)
        rc_btn = RECT(w - 120, 0, w - 24, h)
        user32.DrawTextW(mem_dc, "Открыть", -1, ctypes.byref(rc_btn), 0x00000020 | 0x00000002 | 0x00000004)

        gdi32.SelectObject(mem_dc, old_font)
        gdi32.DeleteObject(title_font)
        gdi32.DeleteObject(btn_font)

        gdi32.BitBlt(hdc, 0, 0, w, h, mem_dc, 0, 0, 0x00CC0020) # SRCCOPY

        gdi32.SelectObject(mem_dc, old_brush)
        gdi32.SelectObject(mem_dc, old_pen)
        gdi32.DeleteObject(bg_brush)
        gdi32.DeleteObject(border_pen)
        gdi32.SelectObject(mem_dc, old_bmp)
        gdi32.DeleteObject(mem_bmp)
        gdi32.DeleteDC(mem_dc)

        user32.EndPaint(hwnd, ctypes.byref(ps))
        return 0

    elif msg in (WM_LBUTTONUP, WM_TIMER):
        user32.DestroyWindow(hwnd)
        return 0

    elif msg == WM_DESTROY:
        _current_bulletin_hwnd = None
        user32.PostQuitMessage(0)
        return 0

    return user32.DefWindowProcW(hwnd, msg, wparam, lparam)

_wndproc_cb = WNDPROC(_bulletin_wnd_proc)

def _show_native_bulletin_win32(title: str, subtitle: str = "", duration_sec: float = 3.5):
    def run():
        global _bulletin_class_atom, _current_bulletin_hwnd
        user32 = ctypes.windll.user32
        gdi32 = ctypes.windll.gdi32
        kernel32 = ctypes.windll.kernel32

        cls_name = "ExteraGramNativeBulletinWindow"
        with _bulletin_lock:
            if not _bulletin_class_atom:
                wc = WNDCLASSEXW()
                wc.cbSize = ctypes.sizeof(WNDCLASSEXW)
                wc.style = 3
                wc.lpfnWndProc = _wndproc_cb
                wc.hInstance = kernel32.GetModuleHandleW(None)
                wc.hCursor = user32.LoadCursorW(None, 32649) # IDC_HAND
                wc.lpszClassName = cls_name
                _bulletin_class_atom = user32.RegisterClassExW(ctypes.byref(wc))

        if _current_bulletin_hwnd and user32.IsWindow(_current_bulletin_hwnd):
            user32.DestroyWindow(_current_bulletin_hwnd)

        # Store text on proc
        _bulletin_wnd_proc._title = str(title)
        _bulletin_wnd_proc._subtitle = str(subtitle)

        # Find foreground window
        fg = user32.GetForegroundWindow()
        rc_fg = RECT(100, 100, 1000, 800)
        if fg and user32.IsWindow(fg):
            user32.GetWindowRect(fg, ctypes.byref(rc_fg))

        tg_w = rc_fg.right - rc_fg.left
        width = min(580, max(420, int(tg_w * 0.65)))
        height = 58
        x = rc_fg.left + (tg_w - width) // 2 + 50
        y = rc_fg.top + 60

        WS_POPUP = 0x80000000
        WS_EX_TOPMOST = 0x00000008
        WS_EX_TOOLWINDOW = 0x00000080
        WS_EX_NOACTIVATE = 0x08000000
        WS_EX_LAYERED = 0x00080000

        hwnd = user32.CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED,
            cls_name, "Bulletin",
            WS_POPUP,
            x, y, width, height,
            fg, None, kernel32.GetModuleHandleW(None), None
        )
        if not hwnd:
            return

        _current_bulletin_hwnd = hwnd

        # Capsule shape
        hrgn = gdi32.CreateRoundRectRgn(0, 0, width + 1, height + 1, height, height)
        user32.SetWindowRgn(hwnd, hrgn, True)

        # Set transparency
        LWA_ALPHA = 2
        user32.SetLayeredWindowAttributes(hwnd, 0, 245, LWA_ALPHA)
        user32.ShowWindow(hwnd, 8) # SW_SHOWNOACTIVATE
        user32.UpdateWindow(hwnd)

        # Timer
        timer_ms = max(1000, int(duration_sec * 1000))
        user32.SetTimer(hwnd, 1, timer_ms, None)

        msg = wintypes.MSG()
        while user32.GetMessageW(ctypes.byref(msg), None, 0, 0) > 0:
            user32.TranslateMessage(ctypes.byref(msg))
            user32.DispatchMessageW(ctypes.byref(msg))

    t = threading.Thread(target=run, daemon=True)
    t.start()

def show_bulletin(title: str, subtitle: str = "", duration_sec: float = 3.5):
    """
    Displays an animated in-app Bulletin banner at the top of the Telegram window.
    Pure Win32 GDI capsule matching exteraGram Android UI.
    """
    try:
        _show_native_bulletin_win32(title, subtitle, duration_sec)
    except Exception as e:
        logger.error(f"Error displaying native bulletin: {e}")
    logger.info(f"[In-App Bulletin] {title}: {subtitle}")

def show_toast(text: str, duration_sec: float = 2.5):
    """Displays a simple in-app Toast notification."""
    show_bulletin(title=text, subtitle="", duration_sec=duration_sec)

def run_on_ui_thread(callback):
    """Runs a callable on the GUI thread."""
    try:
        callback()
    except Exception as e:
        logger.error(f"Error in run_on_ui_thread: {e}")

def get_last_fragment():
    return None

def get_messages_controller():
    class MessagesControllerStub:
        def isDialogMuted(self, dialog_id, topic_id=None):
            return False
        def getClass(self):
            from java import JavaClassStub
            return JavaClassStub("org.telegram.messenger.MessagesController")
    return MessagesControllerStub()

def get_user_config():
    class UserConfigStub:
        def getCurrentUser(self):
            class UserStub:
                id = 0
                username = "me"
                first_name = "User"
            return UserStub()

        def getActivatedAccountsCount(self):
            return 1
    return UserConfigStub()

def get_messages_storage():
    return None

def open_settings():
    logger.info("OpenPluginsSettingsWindow requested.")
