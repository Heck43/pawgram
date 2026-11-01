#pragma once

#include <QString>
#include <QStringList>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QRandomGenerator>
#include <atomic>
#include <thread>
#include <chrono>

#include "custom_features/custom_settings.hpp"
#include "core/application.h"
#include "core/core_settings.h"
#include "core/core_settings_proxy.h"
#include "mtproto/mtproto_proxy_data.h"
#include <crl/crl_on_main.h>

#ifdef Q_OS_WIN
#include <windows.h>
#include <tlhelp32.h>
#endif

namespace CustomFeatures {

class WsProxyManager final {
public:
	static WsProxyManager &Instance() {
		static WsProxyManager instance;
		return instance;
	}

	[[nodiscard]] QString findProxyExecutable() const {
		const QStringList names = {
			u"tg-ws-proxy.exe"_q,
			u"TgWsProxy.exe"_q,
			u"TgWsProxy_windows.exe"_q,
		};

		const QStringList searchDirs = {
			QDir(cExeDir()).absolutePath(),
			QDir(cWorkingDir()).absolutePath(),
			QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation),
			QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + u"/Telegram Desktop"_q,
			QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + u"/TgWsProxy"_q,
			QDir::tempPath(),
		};

		for (const auto &dirPath : searchDirs) {
			if (dirPath.isEmpty()) {
				continue;
			}
			const QDir dir(dirPath);
			for (const auto &name : names) {
				const auto filePath = dir.filePath(name);
				if (QFile::exists(filePath)) {
					return QDir::toNativeSeparators(filePath);
				}
			}
		}
		return QString();
	}

	[[nodiscard]] bool hasExecutable() const {
		return !findProxyExecutable().isEmpty();
	}

	[[nodiscard]] bool isProxyRunning() const {
#ifdef Q_OS_WIN
		if (_processHandle != nullptr) {
			DWORD exitCode = 0;
			if (GetExitCodeProcess(_processHandle, &exitCode) && exitCode == STILL_ACTIVE) {
				return true;
			}
		}
#endif
		return _isRunning.load();
	}

	[[nodiscard]] QString statusString() const {
		if (!hasExecutable()) {
			return u"Файл tg-ws-proxy.exe не найден"_q;
		}
		if (isProxyRunning()) {
			return u"Работает (127.0.0.1:"_q + QString::number(GetConfig().wsProxyPort) + u")"_q;
		}
		return u"Отключен"_q;
	}

	static void terminateAllProxyProcesses() {
#ifdef Q_OS_WIN
		HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (hSnapshot == INVALID_HANDLE_VALUE) {
			return;
		}

		PROCESSENTRY32W pe;
		ZeroMemory(&pe, sizeof(pe));
		pe.dwSize = sizeof(pe);

		if (Process32FirstW(hSnapshot, &pe)) {
			do {
				const std::wstring exeName = pe.szExeFile;
				if (_wcsicmp(exeName.c_str(), L"tg-ws-proxy.exe") == 0
					|| _wcsicmp(exeName.c_str(), L"TgWsProxy.exe") == 0
					|| _wcsicmp(exeName.c_str(), L"TgWsProxy_windows.exe") == 0) {
					HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
					if (hProc) {
						TerminateProcess(hProc, 0);
						CloseHandle(hProc);
					}
				}
			} while (Process32NextW(hSnapshot, &pe));
		}
		CloseHandle(hSnapshot);
#endif
	}

	bool startProxy() {
		if (isProxyRunning()) {
			applyTelegramProxy();
			return true;
		}

		const auto proxyExe = findProxyExecutable();
		if (proxyExe.isEmpty()) {
			return false;
		}

		ensureSecret();
		prepareProxyDataDir(proxyExe);

#ifdef Q_OS_WIN
		// Terminate any stale proxy instances before launching
		terminateAllProxyProcesses();

		if (!_jobHandle) {
			_jobHandle = CreateJobObjectW(nullptr, nullptr);
			if (_jobHandle) {
				JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = { 0 };
				jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
				SetInformationJobObject(
					_jobHandle,
					JobObjectExtendedLimitInformation,
					&jeli,
					sizeof(jeli));
			}
		}

		const auto exeDir = QFileInfo(proxyExe).dir().absolutePath();
		const std::wstring cmdLine = L"\"" + proxyExe.toStdWString() + L"\" --portable";
		std::vector<wchar_t> cmdLineBuf(cmdLine.begin(), cmdLine.end());
		cmdLineBuf.push_back(0);

		STARTUPINFOW si;
		ZeroMemory(&si, sizeof(si));
		si.cb = sizeof(si);
		si.dwFlags = STARTF_USESHOWWINDOW;
		si.wShowWindow = SW_HIDE;

		PROCESS_INFORMATION pi;
		ZeroMemory(&pi, sizeof(pi));

		const std::wstring workDirW = exeDir.toStdWString();

		if (CreateProcessW(
				nullptr,
				cmdLineBuf.data(),
				nullptr,
				nullptr,
				FALSE,
				CREATE_NO_WINDOW,
				nullptr,
				workDirW.c_str(),
				&si,
				&pi)) {
			_processHandle = pi.hProcess;
			_processPid = pi.dwProcessId;

			if (_jobHandle) {
				AssignProcessToJobObject(_jobHandle, pi.hProcess);
			}

			CloseHandle(pi.hThread);
			_isRunning.store(true);

			// Small grace sleep for the proxy to bind its listening port,
			// then configure Telegram proxy on the main thread safely
			std::thread([this]() {
				std::this_thread::sleep_for(std::chrono::milliseconds(600));
				applyTelegramProxy();
			}).detach();

			return true;
		}
#endif
		return false;
	}

	void stopProxy() {
#ifdef Q_OS_WIN
		if (_processHandle != nullptr) {
			TerminateProcess(_processHandle, 0);
			CloseHandle(_processHandle);
			_processHandle = nullptr;
			_processPid = 0;
		}
		if (_jobHandle != nullptr) {
			TerminateJobObject(_jobHandle, 0);
			CloseHandle(_jobHandle);
			_jobHandle = nullptr;
		}

		// Ensure all proxy instances are terminated cleanly via native Win32 API
		terminateAllProxyProcesses();
#endif
		_isRunning.store(false);

		// Revert Telegram proxy to Disabled if current was our local proxy
		// ONLY if the app is still active and not quitting
		if (Core::IsAppLaunched() && !Core::Quitting()) {
			crl::on_main([] {
				if (Core::IsAppLaunched() && !Core::Quitting()) {
					auto &settings = Core::App().settings().proxy();
					if (settings.isEnabled() && settings.selected().host == u"127.0.0.1"_q) {
						Core::App().setCurrentProxy(settings.selected(), MTP::ProxyData::Settings::Disabled);
					}
				}
			});
		}
	}

	void restartProxy() {
		stopProxy();
		std::thread([this]() {
			std::this_thread::sleep_for(std::chrono::milliseconds(300));
			crl::on_main([this] {
				startProxy();
			});
		}).detach();
	}

	void applyTelegramProxy() {
		crl::on_main([this] {
			if (!Core::IsAppLaunched() || Core::Quitting()) {
				return;
			}

			if (!isProxyRunning()) {
				return;
			}

			auto &cfg = GetConfig();
			if (!cfg.wsProxyAutoConfigTg) {
				return;
			}

			MTP::ProxyData proxy;
			proxy.type = MTP::ProxyData::Type::Mtproto;
			proxy.host = u"127.0.0.1"_q;
			proxy.port = cfg.wsProxyPort;
			proxy.password = cfg.wsProxySecret.startsWith(u"dd"_q)
				? cfg.wsProxySecret
				: (u"dd"_q + cfg.wsProxySecret);

			auto &settings = Core::App().settings().proxy();

			// Replace or remove any previous local proxy entry to prevent stale credentials
			for (const auto &item : settings.list()) {
				if (item.type == MTP::ProxyData::Type::Mtproto
					&& item.host == proxy.host
					&& item.port == proxy.port) {
					settings.removeFromList(item);
					break;
				}
			}
			settings.addToList(proxy);

			Core::App().setCurrentProxy(proxy, MTP::ProxyData::Settings::Enabled);
		});
	}

	void downloadProxyAsync(Fn<void(bool success, QString error)> callback) {
#ifdef Q_OS_WIN
		std::thread([callback = std::move(callback)]() {
			const auto targetDir = cWorkingDir();
			const auto targetFile = QDir::toNativeSeparators(targetDir + u"tg-ws-proxy.exe"_q);
			const auto downloadUrl = L"https://github.com/Flowseal/tg-ws-proxy/releases/download/v1.10.2/TgWsProxy_windows.exe";

			const std::wstring cmd = L"curl.exe -s -L -o \"" + targetFile.toStdWString() + L"\" " + downloadUrl;

			STARTUPINFOW si;
			ZeroMemory(&si, sizeof(si));
			si.cb = sizeof(si);
			si.dwFlags = STARTF_USESHOWWINDOW;
			si.wShowWindow = SW_HIDE;

			PROCESS_INFORMATION pi;
			ZeroMemory(&pi, sizeof(pi));

			std::vector<wchar_t> buf(cmd.begin(), cmd.end());
			buf.push_back(0);

			bool success = false;
			if (CreateProcessW(nullptr, buf.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
				WaitForSingleObject(pi.hProcess, 120000);
				DWORD exitCode = 1;
				GetExitCodeProcess(pi.hProcess, &exitCode);
				CloseHandle(pi.hProcess);
				CloseHandle(pi.hThread);
				success = (exitCode == 0) && QFile::exists(targetFile) && (QFileInfo(targetFile).size() > 1000000);
			}

			if (callback) {
				crl::on_main([callback, success]() {
					callback(success, success ? QString() : u"Не удалось загрузить tg-ws-proxy.exe"_q);
				});
			}
		}).detach();
#else
		if (callback) {
			callback(false, u"Поддерживается только на Windows"_q);
		}
#endif
	}

private:
	WsProxyManager() = default;
	~WsProxyManager() {
		stopProxy();
	}

	WsProxyManager(const WsProxyManager&) = delete;
	WsProxyManager &operator=(const WsProxyManager&) = delete;

	void ensureSecret() {
		auto &cfg = GetConfig();
		if (cfg.wsProxySecret.startsWith(u"dd"_q)) {
			cfg.wsProxySecret = cfg.wsProxySecret.mid(2);
		}
		if (cfg.wsProxySecret.length() != 32) {
			static const char hexChars[] = "0123456789abcdef";
			QString secret;
			secret.reserve(32);
			for (int i = 0; i < 32; ++i) {
				secret.append(hexChars[QRandomGenerator::global()->bounded(16)]);
			}
			cfg.wsProxySecret = secret;
			cfg.save();
		}
	}

	void prepareProxyDataDir(const QString &proxyExe) {
		const auto proxyDir = QFileInfo(proxyExe).dir();
		const QStringList targetDirs = {
			proxyDir.filePath(u"TgWsProxy_data"_q),
			QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + u"/TgWsProxy"_q,
		};

		const auto &cfg = GetConfig();

		for (const auto &dirPath : targetDirs) {
			QDir().mkpath(dirPath);

			// 1. First run marker to prevent TgWsProxy from popping up first-run GUI
			QFile marker(dirPath + u"/.first_run_done_mtproto"_q);
			if (marker.open(QIODevice::WriteOnly)) {
				marker.write("done\n");
				marker.close();
			}

			// 2. IPv6 warning marker to prevent TgWsProxy from popping up IPv6 dialog
			QFile ipv6Marker(dirPath + u"/.ipv6_warned"_q);
			if (ipv6Marker.open(QIODevice::WriteOnly)) {
				ipv6Marker.write("done\n");
				ipv6Marker.close();
			}

			// 3. config.json matching current port & secret
			QFile configFile(dirPath + u"/config.json"_q);
			if (configFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
				const QString json =
					u"{\n"
					"  \"port\": "_q + QString::number(cfg.wsProxyPort) + u",\n"
					"  \"host\": \"127.0.0.1\",\n"
					"  \"secret\": \""_q + cfg.wsProxySecret + u"\",\n"
					"  \"cfproxy\": true,\n"
					"  \"check_updates\": false,\n"
					"  \"autostart\": false\n"
					"}\n"_q;
				configFile.write(json.toUtf8());
				configFile.close();
			}
		}
	}

#ifdef Q_OS_WIN
	HANDLE _processHandle = nullptr;
	HANDLE _jobHandle = nullptr;
	DWORD _processPid = 0;
#endif

	std::atomic<bool> _isRunning{ false };
};

} // namespace CustomFeatures
