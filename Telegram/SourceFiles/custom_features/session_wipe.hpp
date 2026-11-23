#pragma once

#include <QString>
#include <QStringList>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QRandomGenerator>
#include <QTimer>
#include <atomic>
#include <vector>

#include "custom_features/custom_settings.hpp"
#include "custom_features/ws_proxy_manager.hpp"
#include "core/application.h"
#include "main/main_domain.h"
#include "main/main_account.h"
#include "logs.h"
#include "settings.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace CustomFeatures {

inline std::atomic<bool> IsWipingInProgress{ false };

namespace SessionWipeDetail {

[[nodiscard]] inline bool HasUncOrDevicePrefix(const QString &rawPath) {
	const auto trimmed = rawPath.trimmed();
	return trimmed.startsWith(u"\\\\"_q) || trimmed.startsWith(u"//"_q);
}

[[nodiscard]] inline QString NormalizeDirectoryPath(const QString &path) {
	const auto trimmed = path.trimmed();
	if (trimmed.isEmpty() || !QDir::isAbsolutePath(trimmed) || HasUncOrDevicePrefix(trimmed)) {
		return QString();
	}
	auto clean = QDir::cleanPath(trimmed);
	if (clean.isEmpty() || HasUncOrDevicePrefix(clean)) {
		return QString();
	}
	const auto isRoot = QDir(clean).isRoot();
	while (!isRoot && clean.endsWith('/')) {
		clean.chop(1);
	}
	return QDir::toNativeSeparators(clean);
}

[[nodiscard]] inline QString ResolveCanonicalLocalDirectory(const QString &rawPath) {
	const auto norm = NormalizeDirectoryPath(rawPath);
	if (norm.isEmpty()) {
		return QString();
	}
#ifdef Q_OS_WIN
	const std::wstring wPath = norm.toStdWString();
	const DWORD attrs = GetFileAttributesW(wPath.c_str());
	if (attrs == INVALID_FILE_ATTRIBUTES
		|| (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0
		|| (attrs & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
		return QString();
	}

	HANDLE hDir = CreateFileW(
		wPath.c_str(),
		0,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		nullptr,
		OPEN_EXISTING,
		FILE_FLAG_BACKUP_SEMANTICS,
		nullptr);
	if (hDir == INVALID_HANDLE_VALUE) {
		return QString();
	}

	std::vector<wchar_t> buf(32768, 0);
	const DWORD len = GetFinalPathNameByHandleW(
		hDir,
		buf.data(),
		static_cast<DWORD>(buf.size() - 1),
		FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
	CloseHandle(hDir);

	if (len == 0 || len >= buf.size()) {
		return QString();
	}

	QString resolved = QString::fromWCharArray(buf.data(), int(len));
	if (resolved.startsWith(u"\\\\?\\UNC\\"_q, Qt::CaseInsensitive)) {
		return QString();
	}
	if (resolved.startsWith(u"\\\\?\\"_q)) {
		resolved = resolved.mid(4);
	}
	if (resolved.size() < 3
		|| !resolved[0].isLetter()
		|| resolved[1] != u':'
		|| resolved[2] != u'\\') {
		return QString();
	}
	while (resolved.size() > 3 && resolved.endsWith(u'\\')) {
		resolved.chop(1);
	}
	return resolved;
#else
	const QFileInfo info(norm);
	if (!info.exists() || !info.isDir() || info.isSymLink()) {
		return QString();
	}
	const auto canon = info.canonicalFilePath();
	return canon.isEmpty() ? QString() : NormalizeDirectoryPath(canon);
#endif
}

[[nodiscard]] inline bool IsSameOrAncestorPath(
		const QString &candidateDir,
		const QString &targetDir) {
	if (candidateDir.isEmpty() || targetDir.isEmpty()) {
		return false;
	}
#ifdef Q_OS_WIN
	constexpr auto cs = Qt::CaseInsensitive;
	const QChar sep = u'\\';
#else
	constexpr auto cs = Qt::CaseSensitive;
	const QChar sep = u'/';
#endif
	if (candidateDir.compare(targetDir, cs) == 0) {
		return true;
	}
	const auto prefix = candidateDir.endsWith(sep) ? candidateDir : (candidateDir + sep);
	return targetDir.startsWith(prefix, cs);
}

[[nodiscard]] inline bool IsSafeWipeWorkDir(const QString &rawWorkDir) {
	const auto canonical = ResolveCanonicalLocalDirectory(rawWorkDir);
	if (canonical.isEmpty() || QDir(canonical).isRoot()) {
		return false;
	}
	if (canonical.size() <= 3 && canonical.endsWith(u'\\')) {
		return false;
	}

	const QStringList protectedRaw = {
		QStandardPaths::writableLocation(QStandardPaths::HomeLocation),
		qEnvironmentVariable("APPDATA"),
		qEnvironmentVariable("LOCALAPPDATA"),
		QDir::tempPath(),
		qEnvironmentVariable("WINDIR"),
		qEnvironmentVariable("SystemRoot"),
		qEnvironmentVariable("ProgramFiles"),
		qEnvironmentVariable("ProgramFiles(x86)"),
		qEnvironmentVariable("SystemDrive"),
		qEnvironmentVariable("PUBLIC"),
		qEnvironmentVariable("ALLUSERSPROFILE"),
	};

	for (const auto &rawProtected : protectedRaw) {
		auto resolvedProtected = ResolveCanonicalLocalDirectory(rawProtected);
		if (resolvedProtected.isEmpty()) {
			resolvedProtected = NormalizeDirectoryPath(rawProtected);
		}
		if (resolvedProtected.isEmpty()) {
			continue;
		}
		if (IsSameOrAncestorPath(canonical, resolvedProtected)) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] inline bool IsStandardAppDataWorkDir(const QString &canonicalWorkDir) {
	const auto appData = qEnvironmentVariable("APPDATA");
	if (appData.trimmed().isEmpty() || canonicalWorkDir.isEmpty()) {
		return false;
	}
	auto resolveOrNorm = [](const QString &p) {
		auto r = ResolveCanonicalLocalDirectory(p);
		return r.isEmpty() ? NormalizeDirectoryPath(p) : r;
	};
	const auto pawgramAppData = resolveOrNorm(appData + u"/Pawgram Desktop"_q);
	const auto telegramAppData = resolveOrNorm(appData + u"/Telegram Desktop"_q);
	return (!pawgramAppData.isEmpty()
			&& canonicalWorkDir.compare(pawgramAppData, Qt::CaseInsensitive) == 0)
		|| (!telegramAppData.isEmpty()
			&& canonicalWorkDir.compare(telegramAppData, Qt::CaseInsensitive) == 0);
}

[[nodiscard]] inline int CountAuthorizedAccounts() {
	if (!Core::IsAppLaunched()) {
		return 0;
	}
	int count = 0;
	for (const auto &item : Core::App().domain().accounts()) {
		if (item.account && item.account->sessionExists()) {
			++count;
		}
	}
	return count;
}

[[nodiscard]] inline QString EscapePsSingleQuoted(QString str) {
	str.remove(u'\r');
	str.remove(u'\n');
	return str.replace(u"'"_q, u"''"_q);
}

inline bool WriteUtf8BomFile(const QString &filePath, const QString &content) {
	QFile file(filePath);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		return false;
	}
	const unsigned char bom[3] = { 0xEF, 0xBB, 0xBF };
	if (file.write(reinterpret_cast<const char*>(bom), sizeof(bom)) != sizeof(bom)) {
		return false;
	}
	const QByteArray utf8 = content.toUtf8();
	if (!utf8.isEmpty() && file.write(utf8) != utf8.size()) {
		return false;
	}
	file.close();
	return true;
}

} // namespace SessionWipeDetail

inline void WipeSessionAndExit(bool relaunch = false) {
	bool expected = false;
	if (!IsWipingInProgress.compare_exchange_strong(expected, true)) {
		return;
	}

	const auto canonicalWork = SessionWipeDetail::ResolveCanonicalLocalDirectory(cWorkingDir());
	if (!SessionWipeDetail::IsSafeWipeWorkDir(canonicalWork)) {
		LOG(("Session Wipe Error: Refusing unsafe or ambiguous workDir '%1'").arg(cWorkingDir()));
		IsWipingInProgress.store(false);
		return;
	}

#ifdef Q_OS_WIN
	const auto pid = static_cast<DWORD>(QCoreApplication::applicationPid());
	quint64 creationTimeFt = 0;
	FILETIME ftCreate = {}, ftExit = {}, ftKernel = {}, ftUser = {};
	if (GetProcessTimes(GetCurrentProcess(), &ftCreate, &ftExit, &ftKernel, &ftUser)) {
		creationTimeFt = (static_cast<quint64>(ftCreate.dwHighDateTime) << 32)
			| static_cast<quint64>(ftCreate.dwLowDateTime);
	}

	const auto nativeExe = QDir::toNativeSeparators(cExeDir() + cExeName());
	const auto canonicalExeDir = SessionWipeDetail::ResolveCanonicalLocalDirectory(cExeDir());
	const auto customIni = QDir::toNativeSeparators(ClientConfig::getSettingsFilePath());
	const auto customIniDir = SessionWipeDetail::ResolveCanonicalLocalDirectory(
		QFileInfo(customIni).absolutePath());
	const bool isStandardProfile = SessionWipeDetail::IsStandardAppDataWorkDir(canonicalWork);
	const bool shouldDeleteCustomIni = isStandardProfile
		|| (!customIniDir.isEmpty()
			&& SessionWipeDetail::IsSameOrAncestorPath(canonicalWork, customIniDir));
	const bool shouldDeleteAppDataProxy = isStandardProfile;

	const auto randomToken = QString::number(QRandomGenerator::global()->generate64(), 16);
	const auto scriptPath = QDir::toNativeSeparators(
		QDir::tempPath()
		+ u"/tg_wipe_cleanup_"_q
		+ QString::number(pid)
		+ u"_"_q
		+ randomToken
		+ u".ps1"_q);
	const auto errorLogPath = QDir::toNativeSeparators(
		QDir::tempPath()
		+ u"/pawgram_wipe_error_"_q
		+ QString::number(pid)
		+ u"_"_q
		+ randomToken
		+ u".log"_q);

	const bool passCustomWorkDir = !isStandardProfile
		&& (canonicalExeDir.isEmpty()
			|| canonicalWork.compare(canonicalExeDir, Qt::CaseInsensitive) != 0);

	const QString scriptContent =
		u"$ErrorActionPreference = 'Stop'\r\n"
		"$targetPid = "_q + QString::number(pid) + u"\r\n"
		"$expectedCreationTime = [uint64]"_q + QString::number(creationTimeFt) + u"\r\n"
		"$workDir = '"_q + SessionWipeDetail::EscapePsSingleQuoted(canonicalWork) + u"'\r\n"
		"$exePath = '"_q + SessionWipeDetail::EscapePsSingleQuoted(nativeExe) + u"'\r\n"
		"$customIni = '"_q + SessionWipeDetail::EscapePsSingleQuoted(customIni) + u"'\r\n"
		"$errorLog = '"_q + SessionWipeDetail::EscapePsSingleQuoted(errorLogPath) + u"'\r\n"
		"$shouldDeleteCustomIni = $"_q + (shouldDeleteCustomIni ? u"true"_q : u"false"_q) + u"\r\n"
		"$shouldDeleteAppDataProxy = $"_q + (shouldDeleteAppDataProxy ? u"true"_q : u"false"_q) + u"\r\n"
		"$relaunch = $"_q + (relaunch ? u"true"_q : u"false"_q) + u"\r\n"
		"$passCustomWorkDir = $"_q + (passCustomWorkDir ? u"true"_q : u"false"_q) + u"\r\n\r\n"
		"$errors = New-Object System.Collections.Generic.List[string]\r\n"
		"function Write-ErrorLog {\r\n"
		"    if ($errors.Count -gt 0) {\r\n"
		"        try { [System.IO.File]::WriteAllLines($errorLog, $errors, [System.Text.Encoding]::UTF8) } catch {}\r\n"
		"    }\r\n"
		"}\r\n\r\n"
		"Add-Type -TypeDefinition @'\r\n"
		"using System;\r\n"
		"using System.Runtime.InteropServices;\r\n"
		"public static class PawgramWipeNative {\r\n"
		"    [DllImport(\"kernel32.dll\", SetLastError = true)]\r\n"
		"    public static extern IntPtr OpenProcess(uint access, bool inherit, uint pid);\r\n"
		"    [DllImport(\"kernel32.dll\", SetLastError = true)]\r\n"
		"    public static extern uint WaitForSingleObject(IntPtr handle, uint ms);\r\n"
		"    [DllImport(\"kernel32.dll\", SetLastError = true)]\r\n"
		"    public static extern bool CloseHandle(IntPtr handle);\r\n"
		"    [DllImport(\"kernel32.dll\", SetLastError = true)]\r\n"
		"    public static extern bool GetProcessTimes(IntPtr handle, out long ct, out long et, out long kt, out long ut);\r\n"
		"    [DllImport(\"kernel32.dll\", CharSet = CharSet.Unicode, SetLastError = true)]\r\n"
		"    public static extern uint GetFileAttributesW(string path);\r\n"
		"}\r\n"
		"'@\r\n\r\n"
		"$SYNCHRONIZE = [uint32]0x00100000\r\n"
		"$QUERY_LIMITED = [uint32]0x00001000\r\n"
		"$WAIT_OBJECT_0 = [uint32]0x00000000\r\n"
		"$INVALID_FILE_ATTRIBUTES = [uint32]::MaxValue\r\n"
		"$REPARSE_POINT = [uint32]0x00000400\r\n\r\n"
		"# 1. Wait for Pawgram process exit via Win32 OpenProcess(SYNCHRONIZE) + WaitForSingleObject (no WMI)\r\n"
		"$hProc = [PawgramWipeNative]::OpenProcess($SYNCHRONIZE -bor $QUERY_LIMITED, $false, [uint32]$targetPid)\r\n"
		"if ($hProc -eq [IntPtr]::Zero) {\r\n"
		"    $openErr = [System.Runtime.InteropServices.Marshal]::GetLastWin32Error()\r\n"
		"    if ($openErr -ne 87) {\r\n"
		"        $errors.Add(\"Abort: OpenProcess failed with Win32 error $openErr; cannot verify process exit.\")\r\n"
		"        Write-ErrorLog\r\n"
		"        Remove-Item -LiteralPath $PSCommandPath -Force -ErrorAction SilentlyContinue\r\n"
		"        exit 1\r\n"
		"    }\r\n"
		"} else {\r\n"
		"    $ct = [long]0; $et = [long]0; $kt = [long]0; $ut = [long]0\r\n"
		"    $sameProcess = $true\r\n"
		"    if ($expectedCreationTime -gt 0 -and [PawgramWipeNative]::GetProcessTimes($hProc, [ref]$ct, [ref]$et, [ref]$kt, [ref]$ut)) {\r\n"
		"        if ([uint64]$ct -ne $expectedCreationTime) {\r\n"
		"            $sameProcess = $false\r\n"
		"        }\r\n"
		"    }\r\n"
		"    if ($sameProcess) {\r\n"
		"        $waitRes = [PawgramWipeNative]::WaitForSingleObject($hProc, [uint32]10000)\r\n"
		"        [void][PawgramWipeNative]::CloseHandle($hProc)\r\n"
		"        if ($waitRes -ne $WAIT_OBJECT_0) {\r\n"
		"            $errors.Add(\"Abort: Target process $targetPid did not exit cleanly (wait result $waitRes).\")\r\n"
		"            Write-ErrorLog\r\n"
		"            Remove-Item -LiteralPath $PSCommandPath -Force -ErrorAction SilentlyContinue\r\n"
		"            exit 2\r\n"
		"        }\r\n"
		"    } else {\r\n"
		"        [void][PawgramWipeNative]::CloseHandle($hProc)\r\n"
		"    }\r\n"
		"}\r\n\r\n"
		"# 2. Safe deletion helper: refuse reparse points (junctions/symlinks) + bounded retries\r\n"
		"function Remove-SafeTarget([string]$targetPath, [bool]$isDir) {\r\n"
		"    if ([string]::IsNullOrWhiteSpace($targetPath)) { return }\r\n"
		"    $attrs = [PawgramWipeNative]::GetFileAttributesW($targetPath)\r\n"
		"    if ($attrs -eq $INVALID_FILE_ATTRIBUTES) { return }\r\n"
		"    if (($attrs -band $REPARSE_POINT) -ne 0) {\r\n"
		"        $errors.Add(\"Refused reparse point (junction/symlink): $targetPath\")\r\n"
		"        return\r\n"
		"    }\r\n"
		"    for ($attempt = 1; $attempt -le 5; $attempt++) {\r\n"
		"        try {\r\n"
		"            if ($isDir) {\r\n"
		"                Remove-Item -LiteralPath $targetPath -Recurse -Force -ErrorAction Stop\r\n"
		"            } else {\r\n"
		"                Remove-Item -LiteralPath $targetPath -Force -ErrorAction Stop\r\n"
		"            }\r\n"
		"            return\r\n"
		"        } catch {\r\n"
		"            if ($attempt -eq 5) {\r\n"
		"                $errors.Add(\"Failed to delete $targetPath after 5 attempts: $($_.Exception.Message)\")\r\n"
		"            } else {\r\n"
		"                Start-Sleep -Milliseconds 300\r\n"
		"            }\r\n"
		"        }\r\n"
		"    }\r\n"
		"}\r\n\r\n"
		"# Verify workDir itself is not a reparse point before touching children\r\n"
		"$workAttrs = [PawgramWipeNative]::GetFileAttributesW($workDir)\r\n"
		"if ($workAttrs -eq $INVALID_FILE_ATTRIBUTES -or ($workAttrs -band $REPARSE_POINT) -ne 0) {\r\n"
		"    $errors.Add(\"Abort: workDir missing or is a reparse point: $workDir\")\r\n"
		"    Write-ErrorLog\r\n"
		"    Remove-Item -LiteralPath $PSCommandPath -Force -ErrorAction SilentlyContinue\r\n"
		"    exit 3\r\n"
		"}\r\n\r\n"
		"Remove-SafeTarget (Join-Path $workDir 'tdata') $true\r\n"
		"Remove-SafeTarget (Join-Path $workDir 'DebugLogs') $true\r\n"
		"Remove-SafeTarget (Join-Path $workDir 'dumps') $true\r\n"
		"Remove-SafeTarget (Join-Path $workDir 'tupdates') $true\r\n"
		"Remove-SafeTarget (Join-Path $workDir 'TgWsProxy_data') $true\r\n"
		"Remove-SafeTarget (Join-Path $workDir 'log.txt') $false\r\n"
		"if ($shouldDeleteCustomIni) {\r\n"
		"    Remove-SafeTarget $customIni $false\r\n"
		"}\r\n\r\n"
		"try {\r\n"
		"    Get-ChildItem -LiteralPath $workDir -File -ErrorAction Stop | Where-Object {\r\n"
		"        $_.Name.ToLowerInvariant().StartsWith('log') -and $_.Name.ToLowerInvariant().EndsWith('.txt')\r\n"
		"    } | ForEach-Object { Remove-SafeTarget $_.FullName $false }\r\n"
		"} catch {\r\n"
		"    $errors.Add(\"Failed to enumerate logs in $workDir: $($_.Exception.Message)\")\r\n"
		"}\r\n\r\n"
		"if ($shouldDeleteAppDataProxy -and -not [string]::IsNullOrWhiteSpace($env:APPDATA)) {\r\n"
		"    Remove-SafeTarget (Join-Path $env:APPDATA 'TgWsProxy') $true\r\n"
		"}\r\n\r\n"
		"Write-ErrorLog\r\n\r\n"
		"if ($relaunch -and (Test-Path -LiteralPath $exePath)) {\r\n"
		"    if ($passCustomWorkDir) {\r\n"
		"        Start-Process -FilePath $exePath -ArgumentList @('-workdir', $workDir)\r\n"
		"    } else {\r\n"
		"        Start-Process -FilePath $exePath\r\n"
		"    }\r\n"
		"}\r\n\r\n"
		"Remove-Item -LiteralPath $PSCommandPath -Force -ErrorAction SilentlyContinue\r\n"_q;

	if (!SessionWipeDetail::WriteUtf8BomFile(scriptPath, scriptContent)) {
		LOG(("Session Wipe Error: Failed to write cleanup script '%1'").arg(scriptPath));
		IsWipingInProgress.store(false);
		return;
	}

	STARTUPINFOW si;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;

	PROCESS_INFORMATION pi;
	ZeroMemory(&pi, sizeof(pi));

	std::wstring cmdLine =
		L"powershell.exe -NoProfile -NonInteractive -WindowStyle Hidden -ExecutionPolicy Bypass -File \""
		+ scriptPath.toStdWString()
		+ L"\"";
	std::vector<wchar_t> cmdLineBuf(cmdLine.begin(), cmdLine.end());
	cmdLineBuf.push_back(0);

	if (!CreateProcessW(
			nullptr,
			cmdLineBuf.data(),
			nullptr,
			nullptr,
			FALSE,
			CREATE_NO_WINDOW | DETACHED_PROCESS,
			nullptr,
			nullptr,
			&si,
			&pi)) {
		const DWORD err = GetLastError();
		QFile::remove(scriptPath);
		LOG(("Session Wipe Error: Failed to launch cleanup helper (Win32 error %1)").arg(err));
		IsWipingInProgress.store(false);
		return;
	}
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
#endif

	// 1. Send logout requests while tg-ws-proxy is STILL active so MTProto auth.logOut can be sent.
	// Note: logOut() is asynchronous and does not guarantee server-side revocation if offline.
	if (Core::IsAppLaunched()) {
		for (const auto &item : Core::App().domain().accounts()) {
			if (item.account && item.account->sessionExists()) {
				item.account->logOut();
			}
		}
	}

#ifdef Q_OS_WIN
	// 2. Allow brief grace period (1000ms) for MTProto logout packet flush before Core::Quit()
	// triggers Application::~Application() -> WsProxyManager::stopProxy() -> stopOwnedProxy().
	QTimer::singleShot(1000, [] {
		Logs::closeMain();
		Core::Quit();
	});

	// 3. Fallback termination watchdog (5000ms) in case Core::Quit() hangs; Job Object kills owned proxy automatically.
	HANDLE hWatchdog = CreateThread(nullptr, 0, [](LPVOID) -> DWORD {
		Sleep(5000);
		ExitProcess(0);
		return 0;
	}, nullptr, 0, nullptr);
	if (hWatchdog != nullptr) {
		CloseHandle(hWatchdog);
	}
#else
	const QDir workDir(canonicalWork);
	QDir(workDir.filePath(u"tdata"_q)).removeRecursively();
	QDir(workDir.filePath(u"DebugLogs"_q)).removeRecursively();
	QDir(workDir.filePath(u"dumps"_q)).removeRecursively();
	QDir(workDir.filePath(u"tupdates"_q)).removeRecursively();
	QDir(workDir.filePath(u"TgWsProxy_data"_q)).removeRecursively();
	QFile::remove(workDir.filePath(u"log.txt"_q));
	const auto customIniDir = SessionWipeDetail::ResolveCanonicalLocalDirectory(
		QFileInfo(ClientConfig::getSettingsFilePath()).absolutePath());
	if (!customIniDir.isEmpty()
		&& SessionWipeDetail::IsSameOrAncestorPath(canonicalWork, customIniDir)) {
		QFile::remove(ClientConfig::getSettingsFilePath());
	}
	Logs::closeMain();
	Core::Quit();
#endif
}

} // namespace CustomFeatures
