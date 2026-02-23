#include "measurement_model.h"

#include <QtMath>

MeasurementModel::MeasurementModel(QObject* parent) : QObject(parent) { rate_timer_.start(); }

void MeasurementModel::setReferenceMax(double max_value) {
  if (max_value > reference_min_) {
    reference_max_ = max_value;
  }
}

double MeasurementModel::referenceMax() const { return reference_max_; }

void MeasurementModel::setReferenceMin(double min_value) {
  if (min_value < reference_max_) {
    reference_min_ = min_value;
  }
}

double MeasurementModel::referenceMin() const { return reference_min_; }

void MeasurementModel::setScaleFactor(double scaleFactor) { scale_factor_ = scaleFactor; }

double MeasurementModel::scaleFactor() const { return scale_factor_; }

void MeasurementModel::setOffset(double offset) { offset_ = offset; }

double MeasurementModel::offset() const { return offset_; }

void MeasurementModel::setAveragingWindowLength(int averagingWindowLength) {
  averaging_window_length_ = qMax(1, averagingWindowLength);
  pruneAveragingQueue();
}

int MeasurementModel::averagingWindowLength() const { return averaging_window_length_; }

void MeasurementModel::setMeasurementTitle(const QString& measurementTitle) {
  if (measurement_title_ == measurementTitle) {
    return;
  }
  measurement_title_ = measurementTitle;
  emit measurementTitleChanged(measurement_title_);
}

QString MeasurementModel::measurementTitle() const { return measurement_title_; }

void MeasurementModel::pushRawSample(double raw_value) {
  const qint64 now_ms = rate_timer_.elapsed();
  last_timestamp_ms_ = now_ms;

  const double processed_value = scale_factor_ * raw_value + offset_;
  averaging_values_.enqueue(processed_value);
  averaging_sum_ += processed_value;
  pruneAveragingQueue();

  const double averaged_value =
      averaging_values_.isEmpty() ? processed_value
                                  : averaging_sum_ / static_cast<double>(averaging_values_.size());

  const double range = reference_max_ - reference_min_;
  const double ratio =
      range > 0.0 ? qBound(0.0, (averaged_value - reference_min_) / range, 1.0) : 0.0;
  emit sampleUpdated(raw_value, processed_value, averaged_value, ratio, now_ms);

  if (stats_running_) {
    updateStats(processed_value);
  }
}

void MeasurementModel::resetHistory() {
  averaging_values_.clear();
  averaging_sum_ = 0.0;
  emit historyReset();
}

void MeasurementModel::resetStatistics() {
  count_ = 0;
  mean_ = 0.0;
  m2_ = 0.0;
  min_ = 0.0;
  max_ = 0.0;
  started_timestamp_ms_ = -1;

  Stats stats;
  emit statisticsUpdated(stats);
}

bool MeasurementModel::statisticsRunning() const { return stats_running_; }

void MeasurementModel::startStatistics() {
  stats_running_ = true;
  if (count_ == 0) {
    started_timestamp_ms_ = -1;
  }
}

void MeasurementModel::stopStatistics() { stats_running_ = false; }

void MeasurementModel::updateStats(double value) {
  const qint64 now_ms = rate_timer_.elapsed();
  if (started_timestamp_ms_ < 0) {
    started_timestamp_ms_ = now_ms;
  }

  ++count_;

  if (count_ == 1) {
    mean_ = value;
    m2_ = 0.0;
    min_ = value;
    max_ = value;
  } else {
    const double delta = value - mean_;
    mean_ += delta / static_cast<double>(count_);
    const double delta2 = value - mean_;
    m2_ += delta * delta2;
    min_ = qMin(min_, value);
    max_ = qMax(max_, value);
  }

  Stats stats;
  stats.current = value;
  stats.average = mean_;
  stats.stddev = count_ > 1 ? qSqrt(m2_ / static_cast<double>(count_ - 1)) : 0.0;
  stats.min = min_;
  stats.max = max_;
  stats.count = count_;
  const qint64 elapsed_ms = now_ms - started_timestamp_ms_;
  stats.rate_hz =
      elapsed_ms > 0 ? static_cast<double>(count_) * 1000.0 / static_cast<double>(elapsed_ms) : 0.0;

  emit statisticsUpdated(stats);
}

void MeasurementModel::pruneAveragingQueue() {
  while (averaging_values_.size() > averaging_window_length_) {
    averaging_sum_ -= averaging_values_.dequeue();
  }
}
