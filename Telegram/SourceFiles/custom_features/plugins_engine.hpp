#pragma once

#include <QString>
#include <QDir>
#include <QFile>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QUrl>
#include <vector>
#include <memory>
#include <mutex>
#include <windows.h>

#include "ui/toast/toast.h"
#include "crl/crl_on_main.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonValue>

namespace CustomFeatures {

struct PluginSettingItem {
    QString type; // "header", "switch", "input", "text", "divider", "selector"
    QString key;
    QString text;
    QString subtext;
    QString value;
    bool boolValue = false;
};

struct PluginUiData {
    QString id;
    QString name;
    QString version;
    QString author;
    QString description;
    bool enabled = true;
    std::vector<PluginSettingItem> settings;
};

class PluginsEngine {
public:
    static PluginsEngine &GetInstance() {
        static PluginsEngine instance;
        return instance;
    }

    bool init() {
        std::lock_guard<std::mutex> lock(_initMutex);
        if (_initialized) return true;

        const auto appDir = QCoreApplication::applicationDirPath();
        _pluginsDir = appDir + "/extera_plugins";
        _settingsDir = appDir + "/extera_settings";
        _sdkDir = appDir + "/extera_sdk";
        _runtimeDir = appDir + "/extera_runtime";

        QDir().mkpath(_pluginsDir);
        QDir().mkpath(_settingsDir);

        if (!loadPythonDll(appDir)) {
            return false;
        }

        if (!resolvePythonFunctions()) {
            return false;
        }

        // Configure sys.path with standard library archive, runtime, and SDK
        QString pyPath =
            _runtimeDir + "/python311.zip;" +
            _runtimeDir + ";" +
            appDir + "/python311.zip;" +
            appDir + ";" +
            _runtimeDir + "/Lib;" +
            _runtimeDir + "/DLLs;" +
            _sdkDir + ";" +
            _pluginsDir;
        std::wstring wPyPath = pyPath.toStdWString();
        if (_Py_SetPath) {
            _Py_SetPath(wPyPath.c_str());
        }

        if (_Py_InitializeEx) {
            _Py_InitializeEx(0);
        }

        if (!_Py_IsInitialized || !_Py_IsInitialized()) {
            return false;
        }

        // Run Python bootstrap
        std::string sLogFile = (_pluginsDir + "/extera_plugins.log").toStdString();
        std::string sPluginsDir = _pluginsDir.toStdString();
        std::string sSettingsDir = _settingsDir.toStdString();

        std::string pyBootstrap =
            "import sys\n"
            "class _DesktopLog:\n"
            "    def write(self, s):\n"
            "        s = s.strip()\n"
            "        if s:\n"
            "            try:\n"
            "                with open(r'" + sLogFile + "', 'a', encoding='utf-8') as f:\n"
            "                    f.write('[Python] ' + s + '\\n')\n"
            "            except: pass\n"
            "    def flush(self): pass\n"
            "sys.stdout = _DesktopLog()\n"
            "sys.stderr = _DesktopLog()\n"
            "import loader\n"
            "mgr = loader.init_manager(r'" + sPluginsDir + "', r'" + sSettingsDir + "')\n";

        if (_PyRun_SimpleString && _PyRun_SimpleString(pyBootstrap.c_str()) != 0) {
            return false;
        }

        void *pLoaderModule = _PyImport_ImportModule("loader");
        if (pLoaderModule) {
            _hookFunc = _PyObject_GetAttrString(pLoaderModule, "hook_send_message");
            _hookIncomingFunc = _PyObject_GetAttrString(pLoaderModule, "hook_incoming_message");
            _Py_DECREF(pLoaderModule);
        }

        if (_PyEval_SaveThread) {
            _mainThreadState = _PyEval_SaveThread();
        }

        _initialized = true;
        return true;
    }

    bool isInitialized() const {
        return _initialized;
    }

    bool dispatchSendMessage(QString &text) {
        if (!_initialized || !_hookFunc) {
            return false; // Don't cancel, send as-is
        }

        // Catch built-in command
        const auto trimmed = text.trimmed();
        if (trimmed == u".plugins"_q || trimmed == u".settings"_q) {
            showToast(u"exteraGram"_q, u"Открываем настройки плагинов..."_q, 2500);
            return true; // Cancel message sending
        }

        void *gstate = nullptr;
        if (_PyGILState_Ensure) {
            gstate = _PyGILState_Ensure();
        }

        bool keepSending = true;
        std::string utf8Input = text.toStdString();
        void *pArgs = _PyTuple_Pack(1, _PyUnicode_FromString(utf8Input.c_str()));
        void *pResult = _PyObject_CallObject(_hookFunc, pArgs);
        _Py_DECREF(pArgs);

        if (pResult) {
            const char *utf8Res = _PyUnicode_AsUTF8(pResult);
            if (utf8Res) {
                std::string sRes(utf8Res);
                if (sRes == "__CANCEL__") {
                    keepSending = false;
                } else {
                    text = QString::fromUtf8(sRes.c_str());
                }
            }
            _Py_DECREF(pResult);
        }

        if (_PyGILState_Release && gstate) {
            _PyGILState_Release(gstate);
        }

        return !keepSending; // return true if cancelled
    }

    bool dispatchIncomingMessage(const QString &title, const QString &text, uint64 peerId) {
        if (!_initialized || !_hookIncomingFunc) {
            return false;
        }

        void *gstate = nullptr;
        if (_PyGILState_Ensure) {
            gstate = _PyGILState_Ensure();
        }

        bool handled = false;
        std::string sTitle = title.toStdString();
        std::string sText = text.toStdString();
        std::string sPeer = std::to_string(peerId);

        void *pArgs = _PyTuple_Pack(3,
            _PyUnicode_FromString(sTitle.c_str()),
            _PyUnicode_FromString(sText.c_str()),
            _PyUnicode_FromString(sPeer.c_str()));

        void *pResult = _PyObject_CallObject(_hookIncomingFunc, pArgs);
        _Py_DECREF(pArgs);

        if (pResult) {
            if (_PyObject_IsTrue && _PyObject_IsTrue(pResult)) {
                handled = true;
            }
            _Py_DECREF(pResult);
        }

        if (_PyGILState_Release && gstate) {
            _PyGILState_Release(gstate);
        }

        return handled;
    }

    std::vector<PluginUiData> getPlugins() {
        std::vector<PluginUiData> list;
        if (!_initialized) return list;

        void *gstate = nullptr;
        if (_PyGILState_Ensure) {
            gstate = _PyGILState_Ensure();
        }

        void *pModule = _PyImport_ImportModule("loader");
        if (pModule) {
            void *pGetJson = _PyObject_GetAttrString(pModule, "get_plugins_ui_json");
            if (pGetJson) {
                void *pRes = _PyObject_CallObject(pGetJson, nullptr);
                if (pRes) {
                    const char *utf8Str = _PyUnicode_AsUTF8(pRes);
                    if (utf8Str) {
                        const auto doc = QJsonDocument::fromJson(QByteArray(utf8Str));
                        if (doc.isArray()) {
                            const auto arr = doc.array();
                            for (const auto &val : arr) {
                                if (!val.isObject()) continue;
                                const auto obj = val.toObject();
                                PluginUiData p;
                                p.id = obj.value(u"id"_q).toString();
                                p.name = obj.value(u"name"_q).toString();
                                p.version = obj.value(u"version"_q).toString(u"1.0.0"_q);
                                p.author = obj.value(u"author"_q).toString();
                                p.description = obj.value(u"description"_q).toString();
                                p.enabled = obj.value(u"enabled"_q).toBool(true);

                                const auto sArr = obj.value(u"settings"_q).toArray();
                                for (const auto &sVal : sArr) {
                                    if (!sVal.isObject()) continue;
                                    const auto sObj = sVal.toObject();
                                    PluginSettingItem s;
                                    s.type = sObj.value(u"type"_q).toString(u"text"_q);
                                    s.key = sObj.value(u"key"_q).toString();
                                    s.text = sObj.value(u"text"_q).toString();
                                    s.subtext = sObj.value(u"subtext"_q).toString();
                                    s.value = sObj.value(u"value"_q).toString();
                                    s.boolValue = sObj.value(u"boolValue"_q).toBool(false);
                                    p.settings.push_back(s);
                                }
                                list.push_back(p);
                            }
                        }
                    }
                    _Py_DECREF(pRes);
                }
                _Py_DECREF(pGetJson);
            }
            _Py_DECREF(pModule);
        }

        if (_PyGILState_Release && gstate) {
            _PyGILState_Release(gstate);
        }
        return list;
    }

    bool saveSetting(const QString &pluginId, const QString &key, const QString &val) {
        if (!_initialized) return false;
        void *gstate = nullptr;
        if (_PyGILState_Ensure) gstate = _PyGILState_Ensure();

        bool ok = false;
        void *pModule = _PyImport_ImportModule("loader");
        if (pModule) {
            void *pFunc = _PyObject_GetAttrString(pModule, "save_plugin_setting");
            if (pFunc) {
                void *pArgs = _PyTuple_Pack(3,
                    _PyUnicode_FromString(pluginId.toUtf8().constData()),
                    _PyUnicode_FromString(key.toUtf8().constData()),
                    _PyUnicode_FromString(val.toUtf8().constData())
                );
                void *pRes = _PyObject_CallObject(pFunc, pArgs);
                if (pRes) {
                    ok = _PyObject_IsTrue(pRes);
                    _Py_DECREF(pRes);
                }
                _Py_DECREF(pArgs);
                _Py_DECREF(pFunc);
            }
            _Py_DECREF(pModule);
        }

        if (_PyGILState_Release && gstate) _PyGILState_Release(gstate);
        return ok;
    }

    bool installPlugin(const QString &filePath) {
        if (!_initialized) return false;
        void *gstate = nullptr;
        if (_PyGILState_Ensure) gstate = _PyGILState_Ensure();

        bool ok = false;
        void *pModule = _PyImport_ImportModule("loader");
        if (pModule) {
            void *pFunc = _PyObject_GetAttrString(pModule, "install_plugin");
            if (pFunc) {
                void *pArgs = _PyTuple_Pack(1, _PyUnicode_FromString(filePath.toUtf8().constData()));
                void *pRes = _PyObject_CallObject(pFunc, pArgs);
                if (pRes) {
                    ok = _PyObject_IsTrue(pRes);
                    _Py_DECREF(pRes);
                }
                _Py_DECREF(pArgs);
                _Py_DECREF(pFunc);
            }
            _Py_DECREF(pModule);
        }

        if (_PyGILState_Release && gstate) _PyGILState_Release(gstate);
        if (ok) {
            showToast(u"exteraGram"_q, u"Плагин успешно установлен!"_q, 3000);
        }
        return ok;
    }

    bool uninstallPlugin(const QString &pluginId) {
        const QDir dir(_pluginsDir);
        for (const auto &file : dir.entryList({ pluginId + ".*", pluginId }, QDir::Files)) {
            QFile::remove(dir.filePath(file));
        }
        showToast(u"exteraGram"_q, u"Плагин удалён."_q, 3000);
        return true;
    }

    void openPluginsFolder() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(_pluginsDir));
    }

    void showToast(const QString &title, const QString &subtitle, int durationMs = 3000) {
        crl::on_main([=] {
            Ui::Toast::Show({
                .title = title,
                .text = { subtitle },
                .duration = crl::time(durationMs)
            });
        });
    }

private:
    PluginsEngine() = default;
    ~PluginsEngine() = default;

    QString getPyStringAttr(void *obj, const char *attr, const QString &def = QString()) {
        void *pAttr = _PyObject_GetAttrString(obj, attr);
        if (!pAttr) return def;
        const char *s = _PyUnicode_AsUTF8(pAttr);
        QString res = s ? QString::fromUtf8(s) : def;
        _Py_DECREF(pAttr);
        return res;
    }

    bool loadPythonDll(const QString &appDir) {
        const QString candidates[] = {
            appDir + "/python311.dll",
            appDir + "/extera_runtime/python311.dll",
            appDir + "/python3.dll",
            "python311.dll"
        };
        for (const auto &path : candidates) {
            _hPyDll = LoadLibraryW(path.toStdWString().c_str());
            if (_hPyDll) return true;
        }
        return false;
    }

    bool resolvePythonFunctions() {
        if (!_hPyDll) return false;
        #define RESOLVE(name) _##name = (decltype(_##name))GetProcAddress(_hPyDll, #name); if (!_##name) return false;
        #define RESOLVE_OPT(name) _##name = (decltype(_##name))GetProcAddress(_hPyDll, #name);

        RESOLVE(Py_InitializeEx);
        RESOLVE(Py_Finalize);
        RESOLVE(Py_IsInitialized);
        RESOLVE_OPT(Py_SetPath);
        RESOLVE(PyRun_SimpleString);
        RESOLVE(PyImport_ImportModule);
        RESOLVE(PyObject_GetAttrString);
        RESOLVE(PyObject_CallObject);
        RESOLVE(PyObject_Str);
        RESOLVE(PyObject_IsTrue);
        RESOLVE(PyTuple_Pack);
        RESOLVE(PyUnicode_FromString);
        RESOLVE(PyUnicode_AsUTF8);
        RESOLVE(PyDict_Next);
        RESOLVE(PyList_Size);
        RESOLVE(PyList_GetItem);
        RESOLVE(PyGILState_Ensure);
        RESOLVE(PyGILState_Release);
        RESOLVE_OPT(PyEval_SaveThread);
        RESOLVE_OPT(PyEval_RestoreThread);

        // Optional helpers
        _Py_IncRef = (void(*)(void*))GetProcAddress(_hPyDll, "Py_IncRef");
        _Py_DecRef = (void(*)(void*))GetProcAddress(_hPyDll, "Py_DecRef");

        return true;
    }

    void _Py_DECREF(void *obj) {
        if (obj && _Py_DecRef) _Py_DecRef(obj);
    }

    HMODULE _hPyDll = nullptr;
    bool _initialized = false;
    std::mutex _initMutex;

    QString _pluginsDir;
    QString _settingsDir;
    QString _sdkDir;
    QString _runtimeDir;

    void *_hookFunc = nullptr;
    void *_hookIncomingFunc = nullptr;
    void *_mainThreadState = nullptr;

    // Python C-API Function Pointers
    void (*_Py_InitializeEx)(int) = nullptr;
    void (*_Py_Finalize)() = nullptr;
    int (*_Py_IsInitialized)() = nullptr;
    void (*_Py_SetPath)(const wchar_t*) = nullptr;
    int (*_PyRun_SimpleString)(const char*) = nullptr;
    void *(*_PyImport_ImportModule)(const char*) = nullptr;
    void *(*_PyObject_GetAttrString)(void*, const char*) = nullptr;
    void *(*_PyObject_CallObject)(void*, void*) = nullptr;
    void *(*_PyObject_Str)(void*) = nullptr;
    int (*_PyObject_IsTrue)(void*) = nullptr;
    void *(*_PyTuple_Pack)(intptr_t, ...) = nullptr;
    void *(*_PyUnicode_FromString)(const char*) = nullptr;
    const char *(*_PyUnicode_AsUTF8)(void*) = nullptr;
    int (*_PyDict_Next)(void*, intptr_t*, void**, void**) = nullptr;
    intptr_t (*_PyList_Size)(void*) = nullptr;
    void *(*_PyList_GetItem)(void*, intptr_t) = nullptr;
    int (*_PyBool_Check)(void*) = nullptr;
    void *(*_PyGILState_Ensure)() = nullptr;
    void (*_PyGILState_Release)(void*) = nullptr;
    void *(*_PyEval_SaveThread)() = nullptr;
    void (*_PyEval_RestoreThread)(void*) = nullptr;
    void (*_Py_IncRef)(void*) = nullptr;
    void (*_Py_DecRef)(void*) = nullptr;
};

} // namespace CustomFeatures
