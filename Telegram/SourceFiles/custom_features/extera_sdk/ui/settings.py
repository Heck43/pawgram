"""
exteraGram Desktop SDK - UI Settings components
Matches the declarative settings definitions used in exteraGram.
"""

from typing import Optional, Any, List

class Header:
    def __init__(self, text: str, **kwargs):
        self.type = "header"
        self.text = text

class Text:
    def __init__(self, text: str, subtext: str = "", icon: str = "", on_click: Any = None, **kwargs):
        self.type = "text"
        self.text = text
        self.subtext = subtext
        self.icon = icon
        self.on_click = on_click

class Input:
    def __init__(self, key: str, text: str, default: str = "", subtext: str = "", icon: str = "", on_change: Any = None, **kwargs):
        self.type = "input"
        self.key = key
        self.text = text
        self.default = default
        self.subtext = subtext
        self.icon = icon
        self.on_change = on_change

class Switch:
    def __init__(self, key: str, text: str, default: bool = False, subtext: str = "", icon: str = "", on_change: Any = None, **kwargs):
        self.type = "switch"
        self.key = key
        self.text = text
        self.default = default
        self.subtext = subtext
        self.icon = icon
        self.on_change = on_change

class Selector:
    def __init__(self, key: str, text: str, items: List[Any] = None, default: Any = 0, subtext: str = "", icon: str = "", on_change: Any = None, **kwargs):
        self.type = "selector"
        self.key = key
        self.text = text
        self.items = items or []
        self.default = default
        self.subtext = subtext
        self.icon = icon
        self.on_change = on_change

class Divider:
    def __init__(self, text: str = "", **kwargs):
        self.type = "divider"
        self.text = text
