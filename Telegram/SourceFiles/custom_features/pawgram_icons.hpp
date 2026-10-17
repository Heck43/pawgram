#pragma once

#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QColor>
#include <QtCore/QRectF>
#include <algorithm>
#include "custom_features/pawgram_theme.hpp"

namespace Pawgram::Icons {

inline void DrawOnlinePaw(
		QPainter &p,
		const QRectF &rect,
		const QColor &color) {
	if (rect.isEmpty()) {
		return;
	}
	p.save();
	p.setRenderHint(QPainter::Antialiasing, true);
	p.setCompositionMode(QPainter::CompositionMode_SourceOver);

	// 1. Отрисовка защитной подложки с DPI-масштабированием
	const auto margin = style::ConvertScale(1.0);
	const auto bgRect = rect.adjusted(-margin, -margin, margin, margin);
	p.setPen(Qt::NoPen);
	p.setBrush(Pawgram::Theme::Palette::SolidGlassBgDark);
	p.drawEllipse(bgRect);

	// 2. Векторная отрисовка подушечек лапки в локальных координатах rect
	p.translate(rect.topLeft());
	const auto w = rect.width();
	const auto h = rect.height();

	p.setPen(Qt::NoPen);
	p.setBrush(color);

	// Центральная подушечка лапки
	QPainterPath mainPad;
	mainPad.addEllipse(QRectF(w * 0.22, h * 0.42, w * 0.56, h * 0.48));
	p.fillPath(mainPad, color);

	// 4 пальчика (подушечки-бобы)
	QPainterPath toe1;
	toe1.addEllipse(QRectF(w * 0.10, h * 0.30, w * 0.20, h * 0.25));
	p.fillPath(toe1, color);

	QPainterPath toe2;
	toe2.addEllipse(QRectF(w * 0.29, h * 0.12, w * 0.19, h * 0.25));
	p.fillPath(toe2, color);

	QPainterPath toe3;
	toe3.addEllipse(QRectF(w * 0.52, h * 0.12, w * 0.19, h * 0.25));
	p.fillPath(toe3, color);

	QPainterPath toe4;
	toe4.addEllipse(QRectF(w * 0.70, h * 0.30, w * 0.20, h * 0.25));
	p.fillPath(toe4, color);

	p.restore();
}

inline void DrawStatusHeart(
		QPainter &p,
		const QRectF &rect,
		const QColor &color) {
	if (rect.isEmpty()) {
		return;
	}
	p.save();
	p.setRenderHint(QPainter::Antialiasing, true);
	p.setCompositionMode(QPainter::CompositionMode_SourceOver);
	p.translate(rect.topLeft());
	const auto w = rect.width();
	const auto h = rect.height();

	QPainterPath path;
	path.moveTo(w * 0.5, h * 0.3);
	// Левая половинка сердца
	path.cubicTo(
		w * 0.15, h * -0.05,
		w * -0.1, h * 0.45,
		w * 0.5, h * 0.92);
	// Правая половинка сердца
	path.cubicTo(
		w * 1.1, h * 0.45,
		w * 0.85, h * -0.05,
		w * 0.5, h * 0.3);
	path.closeSubpath();

	p.setPen(Qt::NoPen);
	p.setBrush(color);
	p.fillPath(path, color);

	p.restore();
}

inline void DrawSearchEars(
		QPainter &p,
		const QRectF &rect,
		const QColor &color) {
	if (rect.isEmpty()) {
		return;
	}
	p.save();
	p.setRenderHint(QPainter::Antialiasing, true);
	p.setCompositionMode(QPainter::CompositionMode_SourceOver);
	p.translate(rect.topLeft());
	const auto w = rect.width();
	const auto h = rect.height();

	const auto penWidth = std::max(1.5, style::ConvertScale(1.8));
	QPen pen(color, penWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
	p.setPen(pen);
	p.setBrush(Qt::NoBrush);

	// Ушки зверька над линзой
	QPainterPath ears;
	// Левое ушко
	ears.moveTo(w * 0.24, h * 0.36);
	ears.lineTo(w * 0.18, h * 0.08);
	ears.lineTo(w * 0.42, h * 0.24);
	// Правое ушко
	ears.moveTo(w * 0.48, h * 0.24);
	ears.lineTo(w * 0.72, h * 0.08);
	ears.lineTo(w * 0.66, h * 0.36);

	p.drawPath(ears);

	// Оправа лупы (линза)
	const auto lensRect = QRectF(w * 0.18, h * 0.24, w * 0.52, h * 0.52);
	p.drawEllipse(lensRect);

	// Ручка лупы
	p.drawLine(
		QPointF(w * 0.60, h * 0.66),
		QPointF(w * 0.88, h * 0.94));

	p.restore();
}

} // namespace Pawgram::Icons
