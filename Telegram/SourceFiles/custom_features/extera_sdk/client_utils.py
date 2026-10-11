"""
exteraGram Desktop SDK - Client Utilities
Provides helpers for interacting with the client and displaying in-app UI.
"""

import ctypes
import os
import sys
import logging

logger = logging.getLogger("client_utils")

# Find extera_desktop.dll
_dll_instance = None

def _get_dll():
    global _dll_instance
    if _dll_instance is None:
        try:
            exe_dir = os.path.dirname(sys.executable)
            dll_candidates = [
                os.path.join(exe_dir, "extera_desktop.dll"),
                os.path.abspath("extera_desktop.dll"),
            ]
            for candidate in dll_candidates:
                if os.path.exists(candidate):
                    _dll_instance = ctypes.CDLL(candidate)
                    break
            if _dll_instance is None:
                # Try loading by name from process directory
                _dll_instance = ctypes.CDLL("extera_desktop.dll")
        except Exception as e:
            logger.error(f"Failed to load extera_desktop.dll for client_utils: {e}")
    return _dll_instance

def show_bulletin(title: str, subtitle: str = "", duration_sec: float = 3.0):
    """
    Displays an animated in-app Bulletin banner at the top of the Telegram Desktop window.
    Equivalent to BulletinFactory in exteraGram mobile.
    """
    dll = _get_dll()
    if dll and hasattr(dll, "ShowDesktopBulletin"):
        try:
            dll.ShowDesktopBulletin.argtypes = [ctypes.c_wchar_p, ctypes.c_wchar_p, ctypes.c_int]
            dll.ShowDesktopBulletin(str(title), str(subtitle), int(duration_sec * 1000))
            return
        except Exception as e:
            logger.error(f"Error calling ShowDesktopBulletin: {e}")
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
    """Opens native plugins configuration window."""
    dll = _get_dll()
    if dll and hasattr(dll, "OpenPluginsSettingsWindow"):
        try:
            dll.OpenPluginsSettingsWindow()
            return
        except Exception as e:
            logger.error(f"Error calling OpenPluginsSettingsWindow: {e}")
    logger.info("OpenPluginsSettingsWindow requested.")
