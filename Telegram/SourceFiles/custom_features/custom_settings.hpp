#pragma once

#include <QString>
#include <QStringList>
#include <QSettings>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QDir>

namespace CustomFeatures {

struct ClientConfig {
    // 1. Ссылки и приватность
    bool cleanTrackingUrls = true;        // Очистка UTM/si меток
    bool directExternalLinks = true;       // Переход без подтверждения
    bool enableDownloadsRouter = true;     // Умная сортировка файлов по папкам
    bool autoWipeOnLogout = false;         // Полная очистка tdata/логов при выходе (режим чужого ПК)

    // 2. Стикеры и медиа
    bool unlimitedRecentStickers = true;   // Расширенный список стикеров (300 шт.)
    int recentStickersMaxCount = 300;      // Лимит недавних стикеров (300)

    // 3. Гейминг и система
    bool enableInGameOverlay = true;       // Оверлей в играх (Shift + ~)
    bool overlayAllGames = false;          // false = только в выбранных играх, true = во всех
    QStringList overlayAllowedGames = { "javaw.exe", "cs2.exe", "dota2.exe", "RobloxPlayerBeta.exe", "GTA5.exe" };
    bool enableGameStatus = false;         // Авто-статус игры в профиле
    bool autoLockOnWindowsLock = false;    // Авто-блокировка при Win + L

    // 4. Интерфейс (UI)
    bool syncWindowsAccentColor = true;    // Цвет акцента из Windows (под обои / Wallpaper Engine)
    bool hideStoriesBar = false;           // Скрыть плашку историй
    bool hideSponsoredAds = true;          // Скрыть спонсорские посты и рекламные плашки вверху
    bool hidePremiumPromos = true;         // Скрыть промо Premium

    // 5. Производительность и Память
    bool fastStartup = true;               // Быстрый запуск (отложенная загрузка)
    bool delayStickersLoad = true;         // Отложить загрузку стикеров
    bool delayStoriesLoad = true;          // Отложить загрузку Stories
    bool trimMemoryOnMinimize = true;      // Авто-сброс RAM при сворачивании в трей/минимизации
    int pixmapCacheLimitMb = 32;           // Лимит кэша растровых изображений Qt в МБ (по умолч. 32)

    // 6. UI Customization
    int chatBorderRadius = 12;             // Скругление углов чатов (0-24px)
    int messageBorderRadius = 12;          // Скругление углов сообщений (0-24px)
    int buttonBorderRadius = 8;            // Скругление кнопок (0-16px)
    bool enableSoftShadows = true;         // Мягкие тени под элементами
    int uiDensity = 5;                     // Плотность UI (0=compact, 5=normal, 10=spacious)
    int animationSpeed = 5;                // Скорость анимаций (0=instant, 5=normal, 10=slow)
    bool smoothScrolling = true;           // Плавная прокрутка
    bool enableMotionBlur = true;          // Размытие в движении (Motion Blur)
    int motionBlurIntensity = 5;           // Интенсивность Motion Blur (1-10)
    int fontSize = 14;                     // Размер шрифта (10-20px)

    // 7. Встроенный tg-ws-proxy (Обход блокировок)
    bool enableWsProxy = false;            // Включить встроенный tg-ws-proxy
    int wsProxyPort = 1443;                // Порт MTProto прокси
    QString wsProxySecret;                 // 32-hex MTProto секрет
    bool wsProxyAutoConfigTg = true;       // Автоматически настраивать прокси в Telegram

    void load() {
        const auto path = getSettingsFilePath();
        QSettings s(path, QSettings::IniFormat);

        cleanTrackingUrls = s.value("cleanTrackingUrls", cleanTrackingUrls).toBool();
        directExternalLinks = s.value("directExternalLinks", directExternalLinks).toBool();
        enableDownloadsRouter = s.value("enableDownloadsRouter", enableDownloadsRouter).toBool();
        autoWipeOnLogout = s.value("autoWipeOnLogout", autoWipeOnLogout).toBool();
        unlimitedRecentStickers = s.value("unlimitedRecentStickers", unlimitedRecentStickers).toBool();
        recentStickersMaxCount = s.value("recentStickersMaxCount", recentStickersMaxCount).toInt();
        enableInGameOverlay = s.value("enableInGameOverlay", enableInGameOverlay).toBool();
        overlayAllGames = s.value("overlayAllGames", overlayAllGames).toBool();
        overlayAllowedGames = s.value("overlayAllowedGames", overlayAllowedGames).toStringList();
        enableGameStatus = s.value("enableGameStatus", enableGameStatus).toBool();
        autoLockOnWindowsLock = s.value("autoLockOnWindowsLock", autoLockOnWindowsLock).toBool();
        syncWindowsAccentColor = s.value("syncWindowsAccentColor", syncWindowsAccentColor).toBool();
        hideStoriesBar = s.value("hideStoriesBar", hideStoriesBar).toBool();
        hideSponsoredAds = s.value("hideSponsoredAds", hideSponsoredAds).toBool();
        hidePremiumPromos = s.value("hidePremiumPromos", hidePremiumPromos).toBool();
        fastStartup = s.value("fastStartup", fastStartup).toBool();
        delayStickersLoad = s.value("delayStickersLoad", delayStickersLoad).toBool();
        delayStoriesLoad = s.value("delayStoriesLoad", delayStoriesLoad).toBool();
        trimMemoryOnMinimize = s.value("trimMemoryOnMinimize", trimMemoryOnMinimize).toBool();
        pixmapCacheLimitMb = s.value("pixmapCacheLimitMb", pixmapCacheLimitMb).toInt();
        if (pixmapCacheLimitMb < 8) pixmapCacheLimitMb = 8;
        if (pixmapCacheLimitMb > 256) pixmapCacheLimitMb = 256;
        
        chatBorderRadius = s.value("chatBorderRadius", chatBorderRadius).toInt();
        messageBorderRadius = s.value("messageBorderRadius", messageBorderRadius).toInt();
        buttonBorderRadius = s.value("buttonBorderRadius", buttonBorderRadius).toInt();
        enableSoftShadows = s.value("enableSoftShadows", enableSoftShadows).toBool();
        uiDensity = s.value("uiDensity", uiDensity).toInt();
        animationSpeed = s.value("animationSpeed", animationSpeed).toInt();
        smoothScrolling = s.value("smoothScrolling", smoothScrolling).toBool();
        enableMotionBlur = s.value("enableMotionBlur", enableMotionBlur).toBool();
        motionBlurIntensity = s.value("motionBlurIntensity", motionBlurIntensity).toInt();
        if (motionBlurIntensity < 1) motionBlurIntensity = 1;
        if (motionBlurIntensity > 10) motionBlurIntensity = 10;
        fontSize = s.value("fontSize", fontSize).toInt();

        enableWsProxy = s.value("enableWsProxy", enableWsProxy).toBool();
        wsProxyPort = s.value("wsProxyPort", wsProxyPort).toInt();
        if (wsProxyPort <= 0 || wsProxyPort > 65535) wsProxyPort = 1443;
        wsProxySecret = s.value("wsProxySecret", wsProxySecret).toString();
        wsProxyAutoConfigTg = s.value("wsProxyAutoConfigTg", wsProxyAutoConfigTg).toBool();
    }

    void save() const {
        const auto path = getSettingsFilePath();
        QSettings s(path, QSettings::IniFormat);

        s.setValue("cleanTrackingUrls", cleanTrackingUrls);
        s.setValue("directExternalLinks", directExternalLinks);
        s.setValue("enableDownloadsRouter", enableDownloadsRouter);
        s.setValue("autoWipeOnLogout", autoWipeOnLogout);
        s.setValue("unlimitedRecentStickers", unlimitedRecentStickers);
        s.setValue("recentStickersMaxCount", recentStickersMaxCount);
        s.setValue("enableInGameOverlay", enableInGameOverlay);
        s.setValue("overlayAllGames", overlayAllGames);
        s.setValue("overlayAllowedGames", overlayAllowedGames);
        s.setValue("enableGameStatus", enableGameStatus);
        s.setValue("autoLockOnWindowsLock", autoLockOnWindowsLock);
        s.setValue("syncWindowsAccentColor", syncWindowsAccentColor);
        s.setValue("hideStoriesBar", hideStoriesBar);
        s.setValue("hideSponsoredAds", hideSponsoredAds);
        s.setValue("hidePremiumPromos", hidePremiumPromos);
        s.setValue("fastStartup", fastStartup);
        s.setValue("delayStickersLoad", delayStickersLoad);
        s.setValue("delayStoriesLoad", delayStoriesLoad);
        s.setValue("trimMemoryOnMinimize", trimMemoryOnMinimize);
        s.setValue("pixmapCacheLimitMb", pixmapCacheLimitMb);
        
        s.setValue("chatBorderRadius", chatBorderRadius);
        s.setValue("messageBorderRadius", messageBorderRadius);
        s.setValue("buttonBorderRadius", buttonBorderRadius);
        s.setValue("enableSoftShadows", enableSoftShadows);
        s.setValue("uiDensity", uiDensity);
        s.setValue("animationSpeed", animationSpeed);
        s.setValue("smoothScrolling", smoothScrolling);
        s.setValue("enableMotionBlur", enableMotionBlur);
        s.setValue("motionBlurIntensity", motionBlurIntensity);
        s.setValue("fontSize", fontSize);

        s.setValue("enableWsProxy", enableWsProxy);
        s.setValue("wsProxyPort", wsProxyPort);
        s.setValue("wsProxySecret", wsProxySecret);
        s.setValue("wsProxyAutoConfigTg", wsProxyAutoConfigTg);
        s.sync();
    }

    static QString getSettingsFilePath() {
        const auto base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        QDir().mkpath(base);
        return base + "/custom_features.ini";
    }
};

// Глобальный синглтон настроек с автоматической загрузкой при старте
inline ClientConfig& GetConfig() {
    static ClientConfig config = [] {
        ClientConfig cfg;
        cfg.load();
        return cfg;
    }();
    return config;
}

} // namespace CustomFeatures
