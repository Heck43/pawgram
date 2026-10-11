"""
exteraGram Desktop SDK - Android Compatibility Utils
"""

import logging

logger = logging.getLogger("android_utils")

def log(msg: str):
    logger.info(str(msg))

def run_on_ui_thread(callback):
    try:
        callback()
    except Exception as e:
        logger.error(f"Error in run_on_ui_thread: {e}")
