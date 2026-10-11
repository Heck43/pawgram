#!/usr/bin/env python3
"""
exteraGram Desktop Plugin Builder & Packager CLI
Usage:
  python build_plugin.py init <plugin_id>       Create a new plugin template
  python build_plugin.py pack <source_path>     Build a distributable .plugin file
  python build_plugin.py check <plugin_path>    Validate metadata and settings
"""

import sys
import os
import argparse
import zipfile
import json
import ast
import shutil

TEMPLATE = '''"""
exteraGram Desktop Plugin
"""

from typing import Any, List
from base_plugin import BasePlugin, HookResult, HookStrategy, MessageParams
from ui.settings import Header, Switch, Input, Selector, Divider, Text
import client_utils

__id__ = "{plugin_id}"
__name__ = "{plugin_name}"
__version__ = "1.0.0"
__author__ = "@author"
__description__ = "Описание вашего нового плагина для Telegram Desktop"

class {class_name}(BasePlugin):
    def on_plugin_load(self):
        self.add_on_send_message_hook()
        client_utils.show_bulletin(
            title=self.name,
            subtitle="Плагин успешно активирован!",
            duration_sec=3.0
        )

    def on_plugin_unload(self):
        pass

    def create_settings(self) -> List[Any]:
        return [
            Header("Основные настройки"),
            Switch(
                key="feature_enabled",
                text="Включить функцию",
                subtext="Включает или выключает главную логику плагина",
                default=True
            ),
            Divider(),
            Header("Параметры"),
            Input(
                key="custom_prefix",
                text="Префикс сообщений",
                subtext="Текст, добавляемый в начало сообщений",
                default="[Modded]"
            ),
            Text(
                text="exteraGram Plugin SDK",
                subtext="Версия API: 2.0 Desktop"
            )
        ]

    def on_send_message_hook(self, account: int, params: MessageParams) -> HookResult:
        if not self.get_setting("feature_enabled", True):
            return HookResult()

        prefix = self.get_setting("custom_prefix", "")
        if prefix and not params.message.startswith(prefix):
            params.message = f"{prefix} {params.message}"
            return HookResult(HookStrategy.MODIFY, params)

        return HookResult()
'''

def cmd_init(args):
    plugin_id = args.plugin_id.strip().lower().replace("-", "_")
    name_parts = [p.capitalize() for p in plugin_id.split("_")]
    class_name = "".join(name_parts) + "Plugin"
    plugin_name = " ".join(name_parts)

    filename = f"{plugin_id}.py"
    if os.path.exists(filename):
        print(f"[!] File {filename} already exists!")
        return 1

    content = TEMPLATE.format(
        plugin_id=plugin_id,
        plugin_name=plugin_name,
        class_name=class_name
    )

    with open(filename, "w", encoding="utf-8") as f:
        f.write(content)

    print(f"[+] Created new plugin template: {filename}")
    print(f"    ID: {plugin_id}")
    print(f"    Name: {plugin_name}")
    print(f"    Class: {class_name}")
    print(f"    Now you can edit {filename} and build with: python build_plugin.py pack {filename}")
    return 0

def cmd_pack(args):
    src = os.path.abspath(args.source)
    if not os.path.exists(src):
        print(f"[!] Source not found: {src}")
        return 1

    out_dir = os.path.abspath(args.out or os.getcwd())
    os.makedirs(out_dir, exist_ok=True)

    if os.path.isfile(src):
        # Single script packaging
        with open(src, "r", encoding="utf-8", errors="replace") as f:
            code = f.read()

        # Extract metadata
        plugin_id = os.path.splitext(os.path.basename(src))[0]
        try:
            tree = ast.parse(code)
            for node in tree.body:
                if isinstance(node, ast.Assign):
                    for target in node.targets:
                        if isinstance(target, ast.Name) and target.id == "__id__":
                            if isinstance(node.value, ast.Constant):
                                plugin_id = str(node.value.value)
        except Exception as e:
            print(f"[?] Could not parse AST, using filename for id: {e}")

        out_file = os.path.join(out_dir, f"{plugin_id}.plugin")
        shutil.copy2(src, out_file)
        print(f"[+] Built single-file plugin: {out_file} ({os.path.getsize(out_file)} bytes)")
        return 0

    elif os.path.isdir(src):
        # Directory packaging into zip .plugin
        plugin_id = os.path.basename(src)
        manifest_file = os.path.join(src, "plugin.json")
        if os.path.exists(manifest_file):
            try:
                with open(manifest_file, "r", encoding="utf-8") as f:
                    m = json.load(f)
                    plugin_id = m.get("id", plugin_id)
            except Exception:
                pass

        out_file = os.path.join(out_dir, f"{plugin_id}.plugin")
        with zipfile.ZipFile(out_file, "w", zipfile.ZIP_DEFLATED) as zf:
            for root, _, files in os.walk(src):
                for file in files:
                    full_p = os.path.join(root, file)
                    rel_p = os.path.relpath(full_p, src)
                    zf.write(full_p, rel_p)

        print(f"[+] Packaged directory into: {out_file} ({os.path.getsize(out_file)} bytes)")
        return 0

    return 1

def cmd_check(args):
    path = os.path.abspath(args.path)
    if not os.path.exists(path):
        print(f"[!] Path not found: {path}")
        return 1

    print(f"[*] Validating {os.path.basename(path)}...")
    # Add SDK to sys.path
    this_dir = os.path.dirname(os.path.abspath(__file__))
    candidates = [
        os.path.join(this_dir, "..", "extera_sdk"),
        os.path.join(this_dir, "extera_sdk"),
        os.path.join(this_dir, "..", "sdk"),
    ]
    for c in candidates:
        if os.path.exists(c):
            sys.path.insert(0, os.path.abspath(c))
            break

    try:
        import loader
        mgr = loader.PluginManager(os.path.dirname(path), os.path.join(os.path.dirname(path), ".tmp_settings"))
        mgr.load_plugin(path)
        if not mgr.plugins:
            print("[!] Validation failed: No plugin loaded.")
            return 1
        p = list(mgr.plugins.values())[0]
        print(f"[+] Loaded: [{p.id}] {p.name} v{getattr(p, 'version', '1.0.0')}")
        settings = p.create_settings()
        print(f"[+] Settings items: {len([s for s in settings if s])}")
        print("[+] Plugin is VALID and ready for deployment!")
        return 0
    except Exception as e:
        print(f"[!] Validation error: {e}")
        return 1

def main():
    parser = argparse.ArgumentParser(description="exteraGram Desktop Plugin Builder")
    subparsers = parser.add_subparsers(dest="cmd", required=True)

    p_init = subparsers.add_parser("init", help="Generate a plugin template")
    p_init.add_argument("plugin_id", help="Unique plugin identifier (e.g. quick_replies)")

    p_pack = subparsers.add_parser("pack", help="Build a .plugin package")
    p_pack.add_argument("source", help="Source .py file or folder")
    p_pack.add_argument("-o", "--out", help="Output directory")

    p_check = subparsers.add_parser("check", help="Verify and test a plugin file")
    p_check.add_argument("path", help="Path to .plugin or .py file")

    args = parser.parse_args()
    if args.cmd == "init":
        sys.exit(cmd_init(args))
    elif args.cmd == "pack":
        sys.exit(cmd_pack(args))
    elif args.cmd == "check":
        sys.exit(cmd_check(args))

if __name__ == "__main__":
    main()
