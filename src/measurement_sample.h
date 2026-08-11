#pragma once

#include <QMetaType>
#include <QString>
#include <QtGlobal>

struct MeasurementSample {
  quint64 sequence = 0;
  qint64 timestamp_ms = 0;
  QString session_id;
  QString measurement_id;
  QString archive_root;
  double raw_value = 0.0;
  double processed_value = 0.0;
  double averaged_value = 0.0;
};

Q_DECLARE_METATYPE(MeasurementSample)
