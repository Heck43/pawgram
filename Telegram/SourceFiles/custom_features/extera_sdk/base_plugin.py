"""
exteraGram Desktop SDK - Base Plugin Architecture
Provides complete compatible API matching exteraGram's Python Plugin SDK.
"""

from __future__ import annotations
from enum import IntEnum, Enum
from typing import Any, List, Optional, Callable
import logging
import json
import os

class HookStrategy(IntEnum):
    DEFAULT = 0
    PASS = 0
    NONE = 0
    MODIFY = 1
    CANCEL = 2

class HookResult:
    def __init__(self, strategy: HookStrategy = HookStrategy.DEFAULT, params: Any = None):
        self.strategy = strategy
        self.params = params

    def __repr__(self) -> str:
        return f"HookResult(strategy={self.strategy}, params={self.params})"

class MessageParams:
    """Wrapper around outgoing message parameters."""
    def __init__(self, message: str, chat_id: int = 0, reply_to_id: int = 0):
        self.message = message
        self.chat_id = chat_id
        self.reply_to_id = reply_to_id

    def __repr__(self) -> str:
        return f"MessageParams(message={self.message!r}, chat_id={self.chat_id})"

class AppEvent(str, Enum):
    APP_START = "app_start"
    APP_STOP = "app_stop"
    RESUME = "resume"
    PAUSE = "pause"
    CONFIG_CHANGED = "config_changed"

class HookFilter:
    def __init__(self, *args, **kwargs):
        pass

    @classmethod
    def Condition(cls, cond: str):
        return cls()

    @classmethod
    def __getattr__(cls, name):
        return lambda *args, **kwargs: cls()

def hook_filters(*filters):
    def decorator(fn):
        return fn
    return decorator

class MethodHook:
    def __init__(self, *args, **kwargs):
        pass

    def before_hook(self, param: Any) -> None:
        pass

    def after_hook(self, param: Any) -> None:
        pass

class MethodReplacement(MethodHook):
    def replace_hook(self, param: Any) -> Any:
        return None

class BasePlugin:
    def __init__(
        self,
        plugin_id: str = "",
        name: str = "",
        version: str = "1.0.0",
        author: str = "",
        description: str = "",
        settings_path: Optional[str] = None
    ):
        self.id = plugin_id
        self.name = name or plugin_id
        self.version = version
        self.author = author
        self.description = description
        self.logger = logging.getLogger(f"Plugin[{self.id}]")
        self.hooks: List[Any] = []
        self._send_message_hook_registered = False
        self._update_hook_registered = False
        self._settings_path = settings_path
        self._settings: dict[str, Any] = {}
        self._load_settings()

    def _load_settings(self):
        if self._settings_path and os.path.exists(self._settings_path):
            try:
                with open(self._settings_path, "r", encoding="utf-8") as f:
                    self._settings = json.load(f)
            except Exception as e:
                self.logger.warning(f"Failed to load settings: {e}")

    def _save_settings(self):
        if self._settings_path:
            try:
                os.makedirs(os.path.dirname(self._settings_path), exist_ok=True)
                with open(self._settings_path, "w", encoding="utf-8") as f:
                    json.dump(self._settings, f, indent=2, ensure_ascii=False)
            except Exception as e:
                self.logger.warning(f"Failed to save settings: {e}")

    def get_setting(self, key: str, default: Any = None) -> Any:
        return self._settings.get(key, default)

    def set_setting(self, key: str, value: Any, force_save: bool = True) -> None:
        self._settings[key] = value
        if force_save:
            self._save_settings()

    def add_on_send_message_hook(self):
        self._send_message_hook_registered = True

    def add_on_update_hook(self):
        self._update_hook_registered = True

    def add_hook(self, *args, **kwargs):
        pass

    def register_hook(self, *args, **kwargs):
        pass

    def hook_method(self, target_method: Any, hook_instance: Any) -> Any:
        """Register a method hook token."""
        self.hooks.append((target_method, hook_instance))
        return hook_instance

    def hook_all_methods(self, target_class: Any, method_name: str, hook_instance: Any) -> Any:
        """Register hooks for all methods matching name."""
        self.hooks.append((target_class, method_name, hook_instance))
        return [hook_instance]

    def _unhook_all(self):
        self.hooks.clear()

    def on_plugin_load(self):
        """Called when the plugin is loaded."""
        pass

    def on_plugin_unload(self):
        """Called when the plugin is unloaded."""
        pass

    def on_send_message_hook(self, account: int, params: Any) -> HookResult:
        """Called before an outgoing message is sent. Return HookResult."""
        return HookResult()

    def on_update_hook(self, account: int, update: Any) -> HookResult:
        """Called on incoming updates."""
        return HookResult()

    def create_settings(self) -> List[Any]:
        """Override to provide settings UI elements."""
        return []
