#pragma once

#include <QElapsedTimer>
#include <QMetaType>
#include <QObject>
#include <QQueue>
#include <QString>

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

  void setReferenceMax(double max_value);
  double referenceMax() const;
  void setReferenceMin(double min_value);
  double referenceMin() const;

  void setScaleFactor(double scaleFactor);
  double scaleFactor() const;

  void setOffset(double offset);
  double offset() const;

  void setAveragingWindowLength(int averagingWindowLength);
  int averagingWindowLength() const;

  void setMeasurementTitle(const QString& measurementTitle);
  QString measurementTitle() const;

  void pushRawSample(double raw_value);
  void resetHistory();
  void resetStatistics();

  bool statisticsRunning() const;
  void startStatistics();
  void stopStatistics();

 signals:
  void sampleUpdated(double raw_value, double processed_value, double averaged_value, double ratio,
                     qint64 timestamp_ms);
  void statisticsUpdated(const MeasurementModel::Stats& stats);
  void historyReset();
  void measurementTitleChanged(const QString& measurementTitle);

 private:
  void updateStats(double value);
  void pruneAveragingQueue();

  double reference_max_ = 100.0;
  double reference_min_ = 0.0;
  double scale_factor_ = 1.0;
  double offset_ = 0.0;
  int averaging_window_length_ = 1;
  bool stats_running_ = true;
  QString measurement_title_;

  QElapsedTimer rate_timer_;
  qint64 last_timestamp_ms_ = 0;

  qint64 count_ = 0;
  double mean_ = 0.0;
  double m2_ = 0.0;
  double min_ = 0.0;
  double max_ = 0.0;

  qint64 started_timestamp_ms_ = -1;
  QQueue<double> averaging_values_;
  double averaging_sum_ = 0.0;
};

Q_DECLARE_METATYPE(MeasurementModel::Stats)
