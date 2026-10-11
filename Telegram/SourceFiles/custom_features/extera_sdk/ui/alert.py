"""
exteraGram Desktop SDK - Alert Dialog Stubs
"""

import logging

logger = logging.getLogger("ui.alert")

class AlertDialogBuilder:
    def __init__(self, *args, **kwargs):
        self.title_text = ""
        self.message_text = ""

    def setTitle(self, title: str):
        self.title_text = title
        return self

    def setMessage(self, message: str):
        self.message_text = message
        return self

    def setPositiveButton(self, text: str, listener=None):
        return self

    def setNegativeButton(self, text: str, listener=None):
        return self

    def setNeutralButton(self, text: str, listener=None):
        return self

    def show(self):
        logger.info(f"[Alert Dialog] {self.title_text}: {self.message_text}")
        return self

    def create(self):
        return self
