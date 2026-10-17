#pragma once

#include "custom_features/custom_settings.hpp"
#include "custom_features/pawgram_theme.hpp"
#include <QEasingCurve>
#include <QGraphicsDropShadowEffect>
#include <QPainter>
#include <QRect>
#include <QRadialGradient>
#include <QWidget>

namespace CustomFeatures {
namespace UI {

// Получить скругление для разных типов элементов
inline int GetChatBorderRadius() {
    return Pawgram::Theme::Metrics::DefaultBorderRadius();
}

inline int GetMessageBorderRadius() {
    return Pawgram::Theme::Metrics::MessageBorderRadius();
}

inline int GetButtonBorderRadius() {
    return Pawgram::Theme::Metrics::ButtonBorderRadius();
}

// Тени
inline bool AreSoftShadowsEnabled() {
    return GetConfig().enableSoftShadows;
}

// Создать эффект тени для виджета
inline QGraphicsDropShadowEffect* CreateSoftShadow(QWidget* widget) {
    if (!AreSoftShadowsEnabled()) {
        return nullptr;
    }
    
    auto* shadow = new QGraphicsDropShadowEffect(widget);
    shadow->setBlurRadius(8);           // Радиус размытия
    shadow->setXOffset(0);              // Смещение по X
    shadow->setYOffset(2);              // Смещение по Y (вниз)
    shadow->setColor(QColor(0, 0, 0, 25)); // Чёрный с прозрачностью 10%
    
    return shadow;
}

// Анимации
inline int GetAnimationDuration(int baseMs) {
    const int speed = GetConfig().animationSpeed;
    if (speed == 0) return 0; // Мгновенно
    
    // Масштабирование: 0=instant, 5=normal(100%), 10=slow(200%)
    const float multiplier = speed / 5.0f;
    return static_cast<int>(baseMs * multiplier);
}

inline QEasingCurve GetSmoothEasing() {
    // Используем кубическую кривую для плавности (UI/UX best practice)
    return QEasingCurve::OutCubic;
}

// Spacing/Density
inline int GetUISpacing(int baseSpacing) {
    const int density = GetConfig().uiDensity;
    // 0=compact(60%), 5=normal(100%), 10=spacious(140%)
    const float multiplier = 0.6f + (density * 0.08f);
    return static_cast<int>(baseSpacing * multiplier);
}

// Размер шрифта
inline int GetFontSize() {
    return GetConfig().fontSize;
}

// Плавная прокрутка
inline bool IsSmoothScrollingEnabled() {
    return GetConfig().smoothScrolling;
}

// Применить стиль тени (для использования в QSS или QPainter)
inline QString GetSoftShadowStyle() {
    if (!AreSoftShadowsEnabled()) {
        return QString();
    }
    // Мягкая тень: offset 0 2px, blur 8px, rgba(0,0,0,0.1)
    // Для Qt: используется через QGraphicsDropShadowEffect
    return "rgba(0, 0, 0, 0.1)";
}

// CSS border-radius для QSS
inline QString GetBorderRadiusStyle(int radius) {
    return QString("border-radius: %1px;").arg(radius);
}

// Нарисовать мягкую тень под прямоугольником (для QPainter)
inline void DrawSoftShadow(QPainter& p, const QRect& rect, int radius) {
    if (!AreSoftShadowsEnabled() || rect.isEmpty()) {
        return;
    }
    
    const int shadowSize = style::ConvertScale(4);
    const QRect shadowRect = rect.adjusted(-shadowSize, 0, shadowSize, shadowSize * 2);
    
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(Pawgram::Theme::Palette::SelectedChatShadow);
    p.drawRoundedRect(shadowRect, radius + shadowSize, radius + shadowSize);
    p.restore();
}

} // namespace UI
} // namespace CustomFeatures
