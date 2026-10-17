#pragma once

#include <QtGui/QColor>
#include <QtCore/QString>
#include "custom_features/custom_settings.hpp"
#include "ui/style/style_core.h"

namespace Pawgram::Theme {

// 🐾 Пастельная палитра темы (Pawgram Furry/Glass Aesthetic)
struct Palette {
	// Основные пастельные акценты
	static inline const QColor Mint = QColor(167, 243, 208);         // #A7F3D0 Мятный
	static inline const QColor MintDark = QColor(52, 211, 153);       // #34D399 Насыщенный мятный
	static inline const QColor Lavender = QColor(221, 214, 254);     // #DDD6FE Лавандовый
	static inline const QColor LavenderDark = QColor(167, 139, 250); // #A78BFA
	static inline const QColor Peach = QColor(254, 215, 170);        // #FED7AA Персиковый
	static inline const QColor PeachDark = QColor(251, 146, 60);      // #FB923C
	static inline const QColor Rose = QColor(254, 205, 211);         // #FECDD3 Розовый
	static inline const QColor RoseDark = QColor(244, 63, 94);        // #F43F5E

	// Полупрозрачные и сплошные стеклянные панели (Glass panels)
	static inline const QColor SolidGlassBgDark = QColor(15, 23, 42); // #0F172A Сплошной для OpaquePaintEvent
	static inline const QColor GlassBgDark = QColor(15, 23, 42, 215);
	static inline const QColor GlassBgSubtle = QColor(30, 41, 59, 140);
	static inline const QColor GlassBorder = QColor(255, 255, 255, 30);
	static inline const QColor GlassBorderHover = QColor(255, 255, 255, 60);

	// Цвета бабблов сообщений (Исходящие / Входящие)
	static inline const QColor OutgoingBubble = QColor(167, 139, 250, 230);
	static inline const QColor IncomingBubble = QColor(30, 41, 59, 200);
	static inline const QColor ActiveChatGlow = QColor(167, 243, 208, 120);
	static inline const QColor SelectedChatShadow = QColor(0, 0, 0, 45);
};

// Радиусы скругления и метрики колонок с поддержкой DPI через style::ConvertScale
struct Metrics {
	static inline constexpr int MinBorderRadius = 0;
	static inline constexpr int MaxBorderRadius = 24;
	static inline constexpr int DefaultChatRadius = 12;
	static inline constexpr int DefaultMessageRadius = 12;
	static inline constexpr int DefaultButtonRadius = 8;
	static inline constexpr int LargeBubbleOffset = 4;

	// Базовые размеры панелей для Discord-like 4-колоночного лейаута (в логических пикселях)
	static inline constexpr int BaseServerBarWidth = 64;   // Ширина левой панели папок
	static inline constexpr int BaseMembersBarWidth = 240; // Ширина правой панели участников

	// Логические значения из настроек
	static inline int DefaultBorderRadius() {
		const int r = CustomFeatures::GetConfig().chatBorderRadius;
		if (r >= MinBorderRadius && r <= MaxBorderRadius) {
			return r;
		}
		return DefaultChatRadius;
	}

	static inline int MessageBorderRadius() {
		const int r = CustomFeatures::GetConfig().messageBorderRadius;
		if (r >= MinBorderRadius && r <= MaxBorderRadius) {
			return r;
		}
		return DefaultMessageRadius;
	}

	static inline int ButtonBorderRadius() {
		const int r = CustomFeatures::GetConfig().buttonBorderRadius;
		if (r >= MinBorderRadius && r <= MaxBorderRadius) {
			return r;
		}
		return DefaultButtonRadius;
	}

	// DPI-масштабированные радиусы для отрисовки в QPainter
	static inline int ScaledChatRadius() {
		return style::ConvertScale(DefaultBorderRadius());
	}

	static inline int ScaledMessageRadius() {
		return style::ConvertScale(MessageBorderRadius());
	}

	static inline int ScaledButtonRadius() {
		return style::ConvertScale(ButtonBorderRadius());
	}

	// DPI-масштабированные размеры колонок
	static inline int ServerBarWidth() {
		return style::ConvertScale(BaseServerBarWidth);
	}

	static inline int MembersBarWidth() {
		return style::ConvertScale(BaseMembersBarWidth);
	}
};

// Проверка активности режима эстетики Pawgram
inline bool IsPawgramAestheticEnabled() {
	return CustomFeatures::GetConfig().enableSoftShadows
		|| (CustomFeatures::GetConfig().chatBorderRadius > 0);
}

} // namespace Pawgram::Theme

namespace Info {
class SectionWidget;
} // namespace Info

namespace Pawgram {
// Правая панель участников / инфо-бар Discord-like layout
using MembersBar = Info::SectionWidget;
} // namespace Pawgram
