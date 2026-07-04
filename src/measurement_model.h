#pragma once

#include <QElapsedTimer>
#include <QMetaType>
#include <QObject>
#include <QQueue>
#include <QString>

#include <memory>

struct EquationEvaluator;

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
  ~MeasurementModel();

  void setReferenceMax(double max_value);
  double referenceMax() const;
  void setReferenceMin(double min_value);
  double referenceMin() const;

  // Transform equation applied to each raw sample. The variable `x` is the raw
  // value (e.g. "sqrt(x)*2 + sin(x)"). Supports arithmetic and the usual
  // functions (exp, sin, cos, sqrt, pow, log, ...). An invalid equation falls
  // back to the identity transform (x).
  void setEquation(const QString& equation);
  QString equation() const;
  bool equationValid() const;
  QString equationError() const;

  // printf-style display format, e.g. "%.3f" or "%8.2e V". Must contain exactly
  // one floating conversion (e/E/f/F/g/G). Invalid formats fall back to a
  // general format.
  void setFormat(const QString& format);
  QString format() const;
  bool formatValid() const;

  // Formats a value using the current display format.
  QString formatValue(double value) const;

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
  QString equation_ = "x";
  bool equation_valid_ = true;
  QString equation_error_;
  std::unique_ptr<EquationEvaluator> evaluator_;
  QString format_ = "%.3f";
  bool format_valid_ = true;
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
