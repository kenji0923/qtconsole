#include "measurement_model.h"

#include <QtMath>

MeasurementModel::MeasurementModel(QObject* parent) : QObject(parent) { rate_timer_.start(); }

void MeasurementModel::set_reference_max(double max_value) {
  if (max_value > 0.0) {
    reference_max_ = max_value;
  }
}

double MeasurementModel::reference_max() const { return reference_max_; }

void MeasurementModel::push_sample(double value) {
  const qint64 now_ms = rate_timer_.elapsed();
  last_timestamp_ms_ = now_ms;

  const double ratio = reference_max_ > 0.0 ? qBound(0.0, value / reference_max_, 1.0) : 0.0;
  emit sample_updated(value, ratio, now_ms);

  if (stats_running_) {
    update_stats(value);
  }
}

void MeasurementModel::reset_history() { emit history_reset(); }

void MeasurementModel::reset_statistics() {
  count_ = 0;
  mean_ = 0.0;
  m2_ = 0.0;
  min_ = 0.0;
  max_ = 0.0;
  recent_sample_timestamps_.clear();

  Stats stats;
  emit statistics_updated(stats);
}

bool MeasurementModel::statistics_running() const { return stats_running_; }

void MeasurementModel::start_statistics() { stats_running_ = true; }

void MeasurementModel::stop_statistics() { stats_running_ = false; }

void MeasurementModel::update_stats(double value) {
  const qint64 now_ms = last_timestamp_ms_;
  recent_sample_timestamps_.enqueue(now_ms);
  while (!recent_sample_timestamps_.isEmpty() && now_ms - recent_sample_timestamps_.head() > 1000) {
    recent_sample_timestamps_.dequeue();
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
  stats.rate_hz = static_cast<double>(recent_sample_timestamps_.size());

  emit statistics_updated(stats);
}
