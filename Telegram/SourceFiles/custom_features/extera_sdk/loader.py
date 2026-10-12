"""
exteraGram Desktop SDK - Plugin Manager & Loader
Scans, isolates, loads and manages external Python plugins for Telegram Desktop.
"""

import os
import sys
import glob
import logging
import inspect
import json
import zipfile
import importlib.util
import importlib.machinery
from typing import Dict, List, Any, Tuple, Optional

# Ensure SDK root and java stubs are loaded
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import java  # triggers install_java_importer()

from base_plugin import BasePlugin, HookResult, HookStrategy, MessageParams

logger = logging.getLogger("extera_loader")

def setup_logging(log_file: str):
    logging.basicConfig(
        filename=log_file,
        level=logging.INFO,
        format="%(asctime)s [%(levelname)s] [%(name)s] %(message)s",
        encoding="utf-8"
    )
    console = logging.StreamHandler()
    console.setLevel(logging.INFO)
    logging.getLogger("").addHandler(console)

class PluginManager:
    def __init__(self, plugins_dir: str, settings_dir: str):
        self.plugins_dir = os.path.abspath(plugins_dir)
        self.settings_dir = os.path.abspath(settings_dir)
        self.plugins: Dict[str, BasePlugin] = {}
        self.plugin_modules: Dict[str, Any] = {}
        self.send_message_subscribers: List[BasePlugin] = []
        self.incoming_message_subscribers: List[BasePlugin] = []

    def load_all_plugins(self):
        logger.info(f"Scanning plugins from {self.plugins_dir}...")
        if not os.path.exists(self.plugins_dir):
            os.makedirs(self.plugins_dir, exist_ok=True)
            logger.info("Plugins directory created.")
            return

        plugin_files = (
            glob.glob(os.path.join(self.plugins_dir, "*.py")) +
            glob.glob(os.path.join(self.plugins_dir, "*.plugin")) +
            glob.glob(os.path.join(self.plugins_dir, "*.zip"))
        )
        for filepath in plugin_files:
            self.load_plugin(filepath)

    def load_plugin(self, filepath: str):
        filename = os.path.basename(filepath)
        module_name = os.path.splitext(filename)[0].replace("-", "_")

        try:
            entry_file = filepath
            if zipfile.is_zipfile(filepath):
                extract_dir = os.path.join(self.plugins_dir, f"_{module_name}_pkg")
                os.makedirs(extract_dir, exist_ok=True)
                with zipfile.ZipFile(filepath, "r") as zf:
                    zf.extractall(extract_dir)
                if os.path.exists(os.path.join(extract_dir, "main.py")):
                    entry_file = os.path.join(extract_dir, "main.py")
                elif os.path.exists(os.path.join(extract_dir, "__init__.py")):
                    entry_file = os.path.join(extract_dir, "__init__.py")
                else:
                    py_files = glob.glob(os.path.join(extract_dir, "**", "*.py"), recursive=True)
                    if py_files:
                        entry_file = py_files[0]
                    else:
                        logger.error(f"No entrypoint python script found inside zip {filename}")
                        return
                sys.path.insert(0, os.path.dirname(entry_file))

            loader = importlib.machinery.SourceFileLoader(module_name, entry_file)
            spec = importlib.util.spec_from_loader(module_name, loader)
            if not spec or not spec.loader:
                logger.error(f"Cannot create spec for {entry_file}")
                return

            module = importlib.util.module_from_spec(spec)
            sys.modules[module_name] = module
            spec.loader.exec_module(module)

            # Find BasePlugin subclass
            plugin_class = None
            for attr_name in dir(module):
                attr = getattr(module, attr_name)
                if (
                    inspect.isclass(attr)
                    and issubclass(attr, BasePlugin)
                    and attr is not BasePlugin
                ):
                    plugin_class = attr
                    break

            if not plugin_class:
                logger.warning(f"No BasePlugin subclass found in {filename}")
                return

            plugin_id = getattr(module, "__id__", module_name)
            plugin_name = getattr(module, "__name__", plugin_id)
            plugin_version = getattr(module, "__version__", "1.0.0")
            plugin_author = getattr(module, "__author__", "")
            plugin_description = getattr(module, "__description__", "")
            settings_path = os.path.join(self.settings_dir, f"{plugin_id}.json")

            try:
                instance = plugin_class(plugin_id=plugin_id, name=plugin_name, settings_path=settings_path)
            except TypeError:
                instance = plugin_class()
                instance.id = plugin_id
                instance.name = plugin_name
                instance._settings_path = settings_path
                instance._load_settings()

            instance.version = plugin_version
            instance.author = plugin_author
            instance.description = plugin_description

            instance.on_plugin_load()

            self.plugins[plugin_id] = instance
            self.plugin_modules[plugin_id] = module
            if instance._send_message_hook_registered:
                self.send_message_subscribers.append(instance)
            if instance._incoming_message_hook_registered:
                self.incoming_message_subscribers.append(instance)

            logger.info(f"Loaded plugin: [{plugin_id}] '{plugin_name}' (v{plugin_version}) from {filename}")

        except Exception as e:
            logger.exception(f"Failed to load plugin {filename}: {e}")

    def dispatch_send_message(self, text: str, chat_id: int = 0) -> Tuple[str, bool]:
        current_text = text
        is_cancelled = False

        params = MessageParams(message=current_text, chat_id=chat_id)

        for plugin in self.send_message_subscribers:
            try:
                result = plugin.on_send_message_hook(account=0, params=params)
                if result:
                    if result.strategy == HookStrategy.CANCEL:
                        logger.info(f"Plugin [{plugin.id}] CANCELLED the outgoing message.")
                        return current_text, True
                    elif result.strategy == HookStrategy.MODIFY and result.params:
                        current_text = getattr(result.params, "message", current_text)
                        params.message = current_text
            except Exception as e:
                logger.exception(f"Error in plugin [{plugin.id}] on_send_message_hook: {e}")

        return current_text, is_cancelled

    def dispatch_incoming_message(self, title: str, text: str, peer_id: int) -> bool:
        handled = False
        for plugin in self.incoming_message_subscribers:
            try:
                res = plugin.on_incoming_message_hook(title=title, text=text, peer_id=peer_id)
                if res:
                    handled = True
            except Exception as e:
                logger.exception(f"Error in plugin [{plugin.id}] on_incoming_message_hook: {e}")
        return handled

_manager: Optional[PluginManager] = None

def init_manager(plugins_dir: str, settings_dir: str):
    global _manager
    log_path = os.path.join(os.path.dirname(plugins_dir), "extera_desktop.log")
    setup_logging(log_path)
    _manager = PluginManager(plugins_dir, settings_dir)
    _manager.load_all_plugins()
    return _manager

def get_manager() -> Optional[PluginManager]:
    return _manager

def hook_send_message(text: str) -> str:
    global _manager
    if not _manager:
        return text

    stripped = text.strip()
    if stripped in (".plugins", ".settings"):
        import client_utils
        client_utils.open_settings()
        return "__CANCEL__"

    final_text, cancelled = _manager.dispatch_send_message(text)
    if cancelled:
        return "__CANCEL__"
    return final_text

def hook_incoming_message(title: str, text: str, peer_id: int) -> bool:
    global _manager
    if not _manager:
        return False
    return _manager.dispatch_incoming_message(title, text, peer_id)

def get_plugins_ui_json() -> str:
    """Returns JSON array of all plugins and their settings for the C++ settings UI."""
    global _manager
    if not _manager:
        return "[]"
    data = []
    for plugin_id, plugin in _manager.plugins.items():
        settings_items = []
        try:
            raw_settings = plugin.create_settings()
            for s in raw_settings:
                if s is None:
                    continue
                s_dict = {
                    "type": getattr(s, "type", "text"),
                    "key": getattr(s, "key", ""),
                    "text": getattr(s, "text", ""),
                    "subtext": getattr(s, "subtext", ""),
                    "value": str(plugin.get_setting(getattr(s, "key", ""), getattr(s, "default", ""))),
                    "boolValue": bool(plugin.get_setting(getattr(s, "key", ""), getattr(s, "default", False))) if getattr(s, "type", "") == "switch" else False,
                }
                settings_items.append(s_dict)
        except Exception as e:
            logger.error(f"Error getting settings for {plugin_id}: {e}")

        data.append({
            "id": plugin.id,
            "name": plugin.name,
            "version": getattr(plugin, "version", "1.0.0"),
            "author": getattr(plugin, "author", ""),
            "description": getattr(plugin, "description", ""),
            "enabled": True,
            "settings": settings_items
        })
    return json.dumps(data, ensure_ascii=False)

def save_plugin_setting(plugin_id: str, key: str, val: str) -> bool:
    global _manager
    if not _manager or plugin_id not in _manager.plugins:
        return False
    plugin = _manager.plugins[plugin_id]
    if val.lower() == "true":
        v = True
    elif val.lower() == "false":
        v = False
    else:
        v = val
    plugin.set_setting(key, v, force_save=True)
    logger.info(f"Saved setting [{key} = {v}] for plugin [{plugin_id}]")
    return True

def toggle_plugin(plugin_id: str, enable: bool) -> bool:
    global _manager
    if not _manager:
        return False
    logger.info(f"Toggle plugin [{plugin_id}] => {enable}")
    return True

def install_plugin(file_path: str) -> bool:
    global _manager
    if not _manager or not os.path.exists(file_path):
        return False
    import shutil
    try:
        dest = os.path.join(_manager.plugins_dir, os.path.basename(file_path))
        if os.path.abspath(file_path) != os.path.abspath(dest):
            shutil.copy2(file_path, dest)
        _manager.load_plugin(dest)
        logger.info(f"Successfully installed plugin from {file_path} to {dest}")
        import client_utils
        client_utils.show_toast("Плагин успешно установлен!")
        return True
    except Exception as e:
        logger.exception(f"Failed to install plugin {file_path}: {e}")
        return False
