#pragma once

#ifdef Q_OS_WIN

#include <windows.h>
#include <tlhelp32.h>
#include <QString>
#include <QVector>
#include <QPair>
#include <QTimer>
#include <QDateTime>
#include "custom_features/custom_settings.hpp"

namespace CustomFeatures {

struct DetectedGame {
    QString processName;
    QString displayName;
    QString emojiStatus;
};

class GameActivityDetector {
public:
    static GameActivityDetector& Instance() {
        static GameActivityDetector detector;
        return detector;
    }

    GameActivityDetector() {
        initGameDatabase();
    }

    ~GameActivityDetector() {
        stopMonitoring();
        delete _timer;
        _timer = nullptr;
    }

    void startMonitoring(std::function<void(const DetectedGame&, bool isPlaying)> onStatusChanged) {
        _callback = onStatusChanged;
        if (!_timer) {
            _timer = new QTimer();
            QObject::connect(_timer, &QTimer::timeout, [this]() {
                checkRunningGames();
            });
        }
        if (!_timer->isActive()) {
            _timer->start(10000); // Проверка каждые 10 секунд
        }
    }

    void stopMonitoring() {
        if (_timer && _timer->isActive()) {
            _timer->stop();
        }
    }

    [[nodiscard]] QString getCurrentGameName() const {
        return _currentGame.displayName;
    }

    [[nodiscard]] bool isPlayingGame() const {
        return !_currentGame.processName.isEmpty();
    }

private:
    void initGameDatabase() {
        _knownGames = {
            { "cs2.exe", "Counter-Strike 2", "🔫" },
            { "csgo.exe", "CS:GO", "💣" },
            { "dota2.exe", "Dota 2", "🛡️" },
            { "Cyberpunk2077.exe", "Cyberpunk 2077", "🦾" },
            { "GTA5.exe", "Grand Theft Auto V", "🚗" },
            { "VALORANT-Win64-Shipping.exe", "Valorant", "🎯" },
            { "RustClient.exe", "Rust", "🪵" },
            { "javaw.exe", "Minecraft", "⛏️" },
            { "Overwatch.exe", "Overwatch 2", "🤖" },
            { "ApexLegends.exe", "Apex Legends", "🏆" },
            { "r5apex.exe", "Apex Legends", "🏆" },
            { "GenshinImpact.exe", "Genshin Impact", "✨" },
            { "StarRail.exe", "Honkai: Star Rail", "🚂" },
            { "Wuthering Waves.exe", "Wuthering Waves", "🌊" },
            { "TslGame.exe", "PUBG: Battlegrounds", "🪂" },
            { "FortniteClient-Win64-Shipping.exe", "Fortnite", "⚡" },
            { "witcher3.exe", "The Witcher 3", "🐺" },
            { "EldenRing.exe", "Elden Ring", "💍" },
            { "BaldursGate3.exe", "Baldur's Gate 3", "🎲" },
            { "bg3.exe", "Baldur's Gate 3", "🎲" },
            { "Helldivers2.exe", "Helldivers 2", "🚀" }
        };
    }

    void checkRunningGames() {
        if (!GetConfig().enableGameStatus) {
            if (isPlayingGame()) {
                _currentGame = DetectedGame();
                if (_callback) _callback(_currentGame, false);
            }
            return;
        }

        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE) return;

        PROCESSENTRY32W pe;
        pe.dwSize = sizeof(PROCESSENTRY32W);

        DetectedGame foundGame;
        if (Process32FirstW(snapshot, &pe)) {
            do {
                const QString procName = QString::fromWCharArray(pe.szExeFile);
                for (const auto &g : _knownGames) {
                    if (procName.compare(g.processName, Qt::CaseInsensitive) == 0) {
                        foundGame = g;
                        break;
                    }
                }
                if (!foundGame.processName.isEmpty()) break;
            } while (Process32NextW(snapshot, &pe));
        }
        CloseHandle(snapshot);

        if (foundGame.processName != _currentGame.processName) {
            _currentGame = foundGame;
            const bool playing = !foundGame.processName.isEmpty();
            if (_callback) {
                _callback(_currentGame, playing);
            }
        }
    }

    QVector<DetectedGame> _knownGames;
    DetectedGame _currentGame;
    QTimer *_timer = nullptr;
    std::function<void(const DetectedGame&, bool isPlaying)> _callback = nullptr;
};

} // namespace CustomFeatures

#endif // Q_OS_WIN
