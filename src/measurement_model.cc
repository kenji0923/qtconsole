#include "measurement_model.h"

#include <QRegularExpression>
#include <QtMath>

#include <cmath>
#include <string>

#include "exprtk.hpp"

// Holds the compiled exprtk expression and the input variable it reads.
struct EquationEvaluator {
  double x = 0.0;
  exprtk::symbol_table<double> symbols;
  exprtk::expression<double> expression;
  exprtk::parser<double> parser;

  EquationEvaluator() {
    symbols.add_variable("x", x);
    symbols.add_constants();  // pi, epsilon, inf
    expression.register_symbol_table(symbols);
  }

  bool compile(const std::string& text, QString* error) {
    if (parser.compile(text, expression)) {
      return true;
    }
    if (error) {
      *error = QString::fromStdString(parser.error());
    }
    return false;
  }

  double eval(double value) {
    x = value;
    return expression.value();
  }
};

namespace {
// Accepts a printf format with exactly one floating conversion (e/E/f/F/g/G),
// optional flags/width/precision, and arbitrary surrounding literal text
// (with %% allowed). Rejects %n, %s, length modifiers, and multiple specifiers.
bool isValidFloatFormat(const QString& format) {
  static const QRegularExpression re(
      QStringLiteral("^(?:[^%]|%%)*%[-+ 0#]*[0-9]*(?:\\.[0-9]+)?[eEfFgG](?:[^%]|%%)*$"));
  return re.match(format).hasMatch();
}
}  // namespace

MeasurementModel::MeasurementModel(QObject* parent)
    : QObject(parent), evaluator_(std::make_unique<EquationEvaluator>()) {
  rate_timer_.start();
  evaluator_->compile("x", &equation_error_);
}

MeasurementModel::~MeasurementModel() = default;

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

void MeasurementModel::setEquation(const QString& equation) {
  equation_ = equation;
  equation_error_.clear();
  const QString trimmed = equation.trimmed();
  // An empty equation is treated as the identity transform.
  const std::string text = (trimmed.isEmpty() ? QStringLiteral("x") : trimmed).toStdString();
  equation_valid_ = evaluator_->compile(text, &equation_error_);
}

QString MeasurementModel::equation() const { return equation_; }

bool MeasurementModel::equationValid() const { return equation_valid_; }

QString MeasurementModel::equationError() const { return equation_error_; }

void MeasurementModel::setFormat(const QString& format) {
  format_ = format;
  format_valid_ = isValidFloatFormat(format);
}

QString MeasurementModel::format() const { return format_; }

bool MeasurementModel::formatValid() const { return format_valid_; }

QString MeasurementModel::formatValue(double value) const {
  if (format_valid_) {
    return QString::asprintf(format_.toUtf8().constData(), value);
  }
  return QString::number(value, 'g', 6);
}

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

  double processed_value = equation_valid_ ? evaluator_->eval(raw_value) : raw_value;
  if (!std::isfinite(processed_value)) {
    processed_value = raw_value;
  }
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
