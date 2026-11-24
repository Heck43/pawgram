#pragma once

#include <algorithm>
#include <atomic>
#include <QtCore/QCoreApplication>
#include <QtCore/QDateTime>
#include <QtGui/QPixmapCache>
#include "custom_features/custom_settings.hpp"

#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#endif // Q_OS_WIN

namespace CustomFeatures {

struct MemorySnapshot {
    qint64 workingSetKb = 0;
    qint64 privateBytesKb = 0;
    bool valid = false;
};

struct MemoryTrimResult {
    bool performed = false;
    bool skippedByCooldown = false;
    qint64 cooldownRemainingSec = 0;
    qint64 workingSetBeforeKb = 0;
    qint64 workingSetAfterKb = 0;
    qint64 privateBytesBeforeKb = 0;
    qint64 privateBytesAfterKb = 0;
};

class MemoryOptimizer {
public:
    static MemorySnapshot QueryMemoryUsage() {
        MemorySnapshot snap;
#ifdef Q_OS_WIN
        PROCESS_MEMORY_COUNTERS_EX pmc = {};
        pmc.cb = sizeof(pmc);
        if (GetProcessMemoryInfo(
                GetCurrentProcess(),
                reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc),
                sizeof(pmc))) {
            snap.workingSetKb = static_cast<qint64>(pmc.WorkingSetSize / 1024);
            snap.privateBytesKb = static_cast<qint64>(pmc.PrivateUsage / 1024);
            snap.valid = true;
        }
#endif // Q_OS_WIN
        return snap;
    }

    static void ApplyCacheLimit() {
        const auto &cfg = GetConfig();
        const int limitMb = std::clamp(cfg.pixmapCacheLimitMb, 8, 256);
        QPixmapCache::setCacheLimit(limitMb * 1024);
    }

    static void InitStartupLimits() {
        ApplyCacheLimit();
    }

    static void ClearGraphicsCacheOnly() {
        QPixmapCache::clear();
    }

    static MemoryTrimResult TrimWorkingSet(bool isAutoMinimize = false) {
        MemoryTrimResult result;
        static std::atomic<qint64> lastTrimMs{ 0 };
        constexpr qint64 kAutoCooldownMs = 15000;
        constexpr qint64 kManualCooldownMs = 3000;
        const qint64 cooldownMs = isAutoMinimize ? kAutoCooldownMs : kManualCooldownMs;

        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        qint64 prevMs = lastTrimMs.load(std::memory_order_relaxed);
        while (true) {
            const qint64 elapsed = nowMs - prevMs;
            if (prevMs > 0 && elapsed >= 0 && elapsed < cooldownMs) {
                result.skippedByCooldown = true;
                result.cooldownRemainingSec = std::max<qint64>(1, (cooldownMs - elapsed + 999) / 1000);
                return result;
            }
            if (lastTrimMs.compare_exchange_weak(
                    prevMs,
                    nowMs,
                    std::memory_order_acq_rel,
                    std::memory_order_relaxed)) {
                break;
            }
        }

        const auto before = QueryMemoryUsage();
        result.workingSetBeforeKb = before.workingSetKb;
        result.privateBytesBeforeKb = before.privateBytesKb;

        // 1. Очищаем графический кэш растровых изображений Qt
        QPixmapCache::clear();

#ifdef Q_OS_WIN
        // 2. Запрашиваем у Windows сброс неактивных страниц рабочего набора
        const auto process = GetCurrentProcess();
        if (process) {
            SetProcessWorkingSetSize(process, static_cast<SIZE_T>(-1), static_cast<SIZE_T>(-1));
            EmptyWorkingSet(process);
        }
#endif // Q_OS_WIN

        const auto after = QueryMemoryUsage();
        result.workingSetAfterKb = after.workingSetKb;
        result.privateBytesAfterKb = after.privateBytesKb;
        result.performed = true;
        return result;
    }

    static void OnMinimizedOrHidden() {
        if (GetConfig().trimMemoryOnMinimize) {
            TrimWorkingSet(true);
        }
    }
};

} // namespace CustomFeatures
