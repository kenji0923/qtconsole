#include "app_icon.h"

#include <QBrush>
#include <QColor>
#include <QFont>
#include <QFontMetricsF>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPixmap>
#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QString>

namespace {
// Draws the gauge icon in a 512x512 coordinate space, scaled to `size`.
// Coordinates mirror resource/icon.svg, the single design source that also
// feeds windows/app.ico (via tools/generate_icon.ps1). Keep the two in sync:
// any change to the geometry or colors here must be reflected in that SVG,
// and vice versa.
QPixmap makeIconPixmap(const QSize& size) {
  QPixmap pixmap(size);
  pixmap.fill(Qt::transparent);

  QPainter p(&pixmap);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.scale(size.width() / 512.0, size.height() / 512.0);

  // Rounded tile.
  QLinearGradient tile_grad(256, 40, 256, 472);
  tile_grad.setColorAt(0.0, QColor("#26375d"));
  tile_grad.setColorAt(1.0, QColor("#131d34"));
  p.setPen(Qt::NoPen);
  p.setBrush(tile_grad);
  p.drawRoundedRect(QRectF(40, 40, 432, 432), 100, 100);

  // Content sits slightly high so it reads as vertically centered in the tile.
  p.save();
  p.translate(0, -38);

  // Dial tick marks.
  {
    QPen major(QColor("#6f84b3"), 7);
    major.setCapStyle(Qt::RoundCap);
    p.setPen(major);
    p.drawLine(QPointF(114, 322), QPointF(136, 322));
    p.drawLine(QPointF(155.6, 221.6), QPointF(171.15, 237.15));
    p.drawLine(QPointF(256, 180), QPointF(256, 202));
    p.drawLine(QPointF(356.4, 221.6), QPointF(340.85, 237.15));
    p.drawLine(QPointF(398, 322), QPointF(376, 322));

    QColor minor_color("#6f84b3");
    minor_color.setAlphaF(0.8);
    QPen minor(minor_color, 5);
    minor.setCapStyle(Qt::RoundCap);
    p.setPen(minor);
    p.drawLine(QPointF(124.8, 267.7), QPointF(135.9, 272.25));
    p.drawLine(QPointF(201.66, 190.8), QPointF(206.25, 201.9));
    p.drawLine(QPointF(310.34, 190.8), QPointF(305.75, 201.9));
    p.drawLine(QPointF(387.2, 267.7), QPointF(376.1, 272.25));
  }

  // Dial bounding box: center (256, 322), radius 168.
  const QRectF dial(88, 154, 336, 336);

  // Unfilled track.
  {
    QPen track(QColor("#3b4e79"), 44);
    track.setCapStyle(Qt::RoundCap);
    p.setPen(track);
    p.drawArc(dial, 0 * 16, 180 * 16);
  }

  // Value arc (fills the dial from the left up to the needle).
  {
    QLinearGradient value_grad(88, 322, 334.9, 173.7);
    value_grad.setColorAt(0.0, QColor("#2bd4a7"));
    value_grad.setColorAt(1.0, QColor("#17b6dc"));
    QPen value(QBrush(value_grad), 44);
    value.setCapStyle(Qt::RoundCap);
    p.setPen(value);
    p.drawArc(dial, 180 * 16, -118 * 16);
  }

  // Needle.
  {
    QPen needle(QColor("#ffc247"), 26);
    needle.setCapStyle(Qt::RoundCap);
    p.setPen(needle);
    p.drawLine(QPointF(241.9, 348.5), QPointF(326.4, 189.6));
  }

  // Center hub.
  p.setPen(QPen(QColor("#ffc247"), 10));
  p.setBrush(QColor("#101a30"));
  p.drawEllipse(QPointF(256, 322), 30, 30);
  p.setPen(Qt::NoPen);
  p.setBrush(QColor("#ffe0a0"));
  p.drawEllipse(QPointF(256, 322), 10, 10);

  // qc monogram, drawn as vector outlines so it scales cleanly.
  {
    QFont font("Arial");
    font.setBold(true);
    font.setPixelSize(92);
    const QFontMetricsF metrics(font);
    const qreal text_width = metrics.horizontalAdvance(QStringLiteral("qc"));
    QPainterPath text_path;
    text_path.addText(256.0 - text_width / 2.0, 438.0, font, QStringLiteral("qc"));
    p.fillPath(text_path, QColor("#eef2fb"));
  }

  p.restore();
  return pixmap;
}
}  // namespace

QIcon createAppIcon() {
  QIcon icon;
  for (const int px : {16, 24, 32, 48, 64, 128, 256}) {
    icon.addPixmap(makeIconPixmap(QSize(px, px)));
  }
  return icon;
}
