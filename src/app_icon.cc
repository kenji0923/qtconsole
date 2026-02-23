#include "app_icon.h"

#include <QColor>
#include <QGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPixmap>
#include <QRectF>
#include <QSize>
#include <QtMath>

namespace {
QPixmap makeIconPixmap(const QSize& size) {
  QPixmap pixmap(size);
  pixmap.fill(Qt::transparent);

  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing, true);

  const qreal w = static_cast<qreal>(size.width());
  const qreal h = static_cast<qreal>(size.height());
  const QRectF rect(1.0, 1.0, w - 2.0, h - 2.0);
  const qreal radius = qMin(w, h) * 0.22;

  QLinearGradient bg(rect.topLeft(), rect.bottomRight());
  bg.setColorAt(0.0, QColor(12, 56, 88));
  bg.setColorAt(1.0, QColor(20, 132, 95));
  painter.setPen(Qt::NoPen);
  painter.setBrush(bg);
  painter.drawRoundedRect(rect, radius, radius);

  QPainterPath wave;
  const qreal left = rect.left() + w * 0.10;
  const qreal right = rect.right() - w * 0.10;
  const qreal center_y = rect.top() + h * 0.68;
  const qreal amp = h * 0.10;
  const qreal width = right - left;
  wave.moveTo(left, center_y);
  for (int i = 1; i <= 48; ++i) {
    const qreal t = static_cast<qreal>(i) / 48.0;
    const qreal x = left + width * t;
    const qreal y = center_y - qSin(t * 2.0 * M_PI * 1.6) * amp;
    wave.lineTo(x, y);
  }
  QPen wave_pen(QColor(223, 255, 244), qMax(2.0, w * 0.06));
  wave_pen.setCapStyle(Qt::RoundCap);
  painter.setPen(wave_pen);
  painter.drawPath(wave);

  const QColor mono_color(244, 255, 251);
  const qreal stroke = qMax(1.5, w * 0.06);
  QPen mono_pen(mono_color, stroke);
  mono_pen.setCapStyle(Qt::RoundCap);
  mono_pen.setJoinStyle(Qt::RoundJoin);
  painter.setPen(mono_pen);
  painter.setBrush(Qt::NoBrush);

  // Geometric monogram avoids DPI/font hinting shifts on taskbar rendering.
  const qreal q_cx = rect.left() + w * 0.37;
  const qreal q_cy = rect.top() + h * 0.39;
  const qreal q_r = h * 0.105;
  painter.drawEllipse(QPointF(q_cx, q_cy), q_r, q_r);
  painter.drawLine(QPointF(q_cx + q_r * 0.72, q_cy + q_r * 0.72),
                   QPointF(q_cx + q_r * 1.55, q_cy + q_r * 1.65));

  const qreal c_cx = rect.left() + w * 0.61;
  const qreal c_cy = rect.top() + h * 0.39;
  const qreal c_r = h * 0.10;
  QRectF c_rect(c_cx - c_r, c_cy - c_r, c_r * 2.0, c_r * 2.0);
  painter.drawArc(c_rect, 35 * 16, 290 * 16);

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
