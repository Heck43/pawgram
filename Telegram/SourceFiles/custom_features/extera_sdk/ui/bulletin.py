"""
exteraGram Desktop SDK - UI Bulletin Helpers
Provides BulletinFactory and BulletinHelper for in-app notifications.
"""

import client_utils

class SimpleBulletin:
    def __init__(self, title: str, subtitle: str = ""):
        self.title = str(title)
        self.subtitle = str(subtitle)
        self.duration = 3.0

    def show(self, top: bool = True):
        client_utils.show_bulletin(self.title, self.subtitle, self.duration)

    def allowBlur(self):
        pass

class BulletinFactory:
    @staticmethod
    def of(fragment=None):
        return BulletinFactory()

    def createSimpleBulletin(self, title: str, subtitle: str = ""):
        return SimpleBulletin(title, subtitle)

class BulletinHelper:
    @staticmethod
    def show_bulletin(title: str, subtitle: str = ""):
        client_utils.show_bulletin(title, subtitle)
