#pragma once

#include <QString>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDir>
#include <QSet>
#include "custom_features/custom_settings.hpp"

namespace CustomFeatures {

class DownloadsRouter {
public:
    enum class FileCategory {
        Images,
        Audio,
        Video,
        Documents,
        Archives,
        Programs,
        Other
    };

    static FileCategory CategorizeFile(const QString &fileName) {
        const auto ext = QFileInfo(fileName).suffix().toLower();
        if (ext.isEmpty()) {
            return FileCategory::Other;
        }

        // 1. Изображения
        static const QSet<QString> kImages = {
            QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("png"),
            QStringLiteral("gif"), QStringLiteral("webp"), QStringLiteral("bmp"),
            QStringLiteral("svg"), QStringLiteral("heic"), QStringLiteral("heif"),
            QStringLiteral("avif"), QStringLiteral("jfif"), QStringLiteral("psd"),
            QStringLiteral("ico"), QStringLiteral("tiff"), QStringLiteral("tif"),
            QStringLiteral("ai"), QStringLiteral("dng"), QStringLiteral("raw"),
            QStringLiteral("cr2"), QStringLiteral("nef"), QStringLiteral("arw")
        };
        if (kImages.contains(ext)) {
            return FileCategory::Images;
        }

        // 2. Аудио и музыка
        static const QSet<QString> kAudio = {
            QStringLiteral("mp3"), QStringLiteral("flac"), QStringLiteral("wav"),
            QStringLiteral("ogg"), QStringLiteral("m4a"), QStringLiteral("m4b"),
            QStringLiteral("aac"), QStringLiteral("opus"), QStringLiteral("mid"),
            QStringLiteral("midi"), QStringLiteral("wma"), QStringLiteral("alac"),
            QStringLiteral("aiff"), QStringLiteral("aif"), QStringLiteral("amr")
        };
        if (kAudio.contains(ext)) {
            return FileCategory::Audio;
        }

        // 3. Видео
        static const QSet<QString> kVideo = {
            QStringLiteral("mp4"), QStringLiteral("mkv"), QStringLiteral("avi"),
            QStringLiteral("mov"), QStringLiteral("webm"), QStringLiteral("wmv"),
            QStringLiteral("flv"), QStringLiteral("m4v"), QStringLiteral("3gp"),
            QStringLiteral("ts"), QStringLiteral("m2ts"), QStringLiteral("mpg"),
            QStringLiteral("mpeg"), QStringLiteral("ogv")
        };
        if (kVideo.contains(ext)) {
            return FileCategory::Video;
        }

        // 4. Документы, книги и логи
        static const QSet<QString> kDocuments = {
            QStringLiteral("pdf"), QStringLiteral("docx"), QStringLiteral("doc"),
            QStringLiteral("xlsx"), QStringLiteral("xls"), QStringLiteral("pptx"),
            QStringLiteral("ppt"), QStringLiteral("txt"), QStringLiteral("log"),
            QStringLiteral("html"), QStringLiteral("htm"), QStringLiteral("csv"),
            QStringLiteral("epub"), QStringLiteral("fb2"), QStringLiteral("djvu"),
            QStringLiteral("mobi"), QStringLiteral("azw3"), QStringLiteral("json"),
            QStringLiteral("xml"), QStringLiteral("yaml"), QStringLiteral("yml"),
            QStringLiteral("toml"), QStringLiteral("sql"), QStringLiteral("ini"),
            QStringLiteral("cfg"), QStringLiteral("tex"), QStringLiteral("rtf"),
            QStringLiteral("md"), QStringLiteral("odt"), QStringLiteral("ods")
        };
        if (kDocuments.contains(ext)) {
            return FileCategory::Documents;
        }

        // 5. Архивы
        static const QSet<QString> kArchives = {
            QStringLiteral("zip"), QStringLiteral("rar"), QStringLiteral("7z"),
            QStringLiteral("tar"), QStringLiteral("gz"), QStringLiteral("bz2"),
            QStringLiteral("xz"), QStringLiteral("tgz"), QStringLiteral("zst"),
            QStringLiteral("lz4"), QStringLiteral("cab"), QStringLiteral("iso"),
            QStringLiteral("img")
        };
        if (kArchives.contains(ext)) {
            return FileCategory::Archives;
        }

        // 6. Программы и исполняемые файлы
        static const QSet<QString> kPrograms = {
            QStringLiteral("exe"), QStringLiteral("msi"), QStringLiteral("dll"),
            QStringLiteral("bat"), QStringLiteral("cmd"), QStringLiteral("ps1"),
            QStringLiteral("apk"), QStringLiteral("vbs"), QStringLiteral("jar"),
            QStringLiteral("msix"), QStringLiteral("appx"), QStringLiteral("dmg"),
            QStringLiteral("deb"), QStringLiteral("rpm"), QStringLiteral("appimage")
        };
        if (kPrograms.contains(ext)) {
            return FileCategory::Programs;
        }

        return FileCategory::Other;
    }

    static bool IsCategoryEnabled(FileCategory category) {
        const auto &cfg = GetConfig();
        switch (category) {
        case FileCategory::Images: return cfg.downloadsRouteImages;
        case FileCategory::Audio: return cfg.downloadsRouteAudio;
        case FileCategory::Video: return cfg.downloadsRouteVideo;
        case FileCategory::Documents: return cfg.downloadsRouteDocuments;
        case FileCategory::Archives: return cfg.downloadsRouteArchives;
        case FileCategory::Programs: return cfg.downloadsRoutePrograms;
        case FileCategory::Other:
        default:
            return cfg.downloadsRouteOther;
        }
    }

    static QString CategorySubfolder(FileCategory category) {
        switch (category) {
        case FileCategory::Images: return QStringLiteral("Изображения");
        case FileCategory::Audio: return QStringLiteral("Музыка");
        case FileCategory::Video: return QStringLiteral("Видео");
        case FileCategory::Documents: return QStringLiteral("Документы");
        case FileCategory::Archives: return QStringLiteral("Архивы");
        case FileCategory::Programs: return QStringLiteral("Программы");
        case FileCategory::Other:
        default:
            return QStringLiteral("Разное");
        }
    }

    static QString ResolveBasePath(const QString &defaultPath = QString()) {
        const auto &cfg = GetConfig();
        const auto custom = cfg.downloadsCustomBasePath.trimmed();
        if (!custom.isEmpty() && QDir::isAbsolutePath(custom)) {
            return QDir::cleanPath(custom);
        }
        const auto trimmedDefault = defaultPath.trimmed();
        if (!trimmedDefault.isEmpty() && QDir::isAbsolutePath(trimmedDefault)) {
            return QDir::cleanPath(trimmedDefault);
        }
        return QDir::cleanPath(
            QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)
            + QStringLiteral("/Telegram Desktop"));
    }

    static QString ResolveUniqueFilePath(const QString &directory, const QString &fileName) {
        const QDir dir(directory);
        const auto initial = dir.filePath(fileName);
        if (!QFileInfo::exists(initial)) {
            return initial;
        }
        const QFileInfo info(fileName);
        const auto base = info.completeBaseName();
        const auto suffix = info.suffix();
        for (int i = 1; i <= 9999; ++i) {
            const auto candidateName = suffix.isEmpty()
                ? QStringLiteral("%1 (%2)").arg(base).arg(i)
                : QStringLiteral("%1 (%2).%3").arg(base).arg(i).arg(suffix);
            const auto candidate = dir.filePath(candidateName);
            if (!QFileInfo::exists(candidate)) {
                return candidate;
            }
        }
        return initial;
    }

    static QString GetSuggestedDownloadPath(const QString &fileName, const QString &defaultPath = QString()) {
        const auto basePath = ResolveBasePath(defaultPath);
        if (!GetConfig().enableDownloadsRouter) {
            return defaultPath.trimmed().isEmpty() ? basePath : defaultPath;
        }

        const auto category = CategorizeFile(fileName);
        if (!IsCategoryEnabled(category)) {
            return defaultPath.trimmed().isEmpty() ? basePath : defaultPath;
        }

        const auto targetDir = QDir(basePath).filePath(CategorySubfolder(category));
        if (!QDir().mkpath(targetDir)) {
            return defaultPath.trimmed().isEmpty() ? basePath : defaultPath;
        }
        return targetDir;
    }
};

} // namespace CustomFeatures
