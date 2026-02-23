#pragma once

#include <QElapsedTimer>
#include <QMetaType>
#include <QObject>
#include <QQueue>

class MeasurementModel : public QObject {
  Q_OBJECT
 public:
  struct Stats {
    double current = 0.0;
    double average = 0.0;
    double stddev = 0.0;
    double min = 0.0;
    double max = 0.0;
    qint64 count = 0;
    double rate_hz = 0.0;
  };

  explicit MeasurementModel(QObject* parent = nullptr);

  void set_reference_max(double max_value);
  double reference_max() const;

  void push_sample(double value);
  void reset_history();
  void reset_statistics();

  bool statistics_running() const;
  void start_statistics();
  void stop_statistics();

 signals:
  void sample_updated(double value, double ratio, qint64 timestamp_ms);
  void statistics_updated(const MeasurementModel::Stats& stats);
  void history_reset();

 private:
  void update_stats(double value);

  double reference_max_ = 100.0;
  bool stats_running_ = true;

  QElapsedTimer rate_timer_;
  qint64 last_timestamp_ms_ = 0;

  qint64 count_ = 0;
  double mean_ = 0.0;
  double m2_ = 0.0;
  double min_ = 0.0;
  double max_ = 0.0;

  QQueue<qint64> recent_sample_timestamps_;
};

Q_DECLARE_METATYPE(MeasurementModel::Stats)
