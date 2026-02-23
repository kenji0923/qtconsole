#include "time_series_widget.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGraphicsLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPainter>
#include <QPdfWriter>
#include <QPushButton>
#include <QSettings>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLegend>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

#include "measurement_model.h"

TimeSeriesWidget::TimeSeriesWidget(MeasurementModel* model, QWidget* parent)
    : QWidget(parent),
      model_(model),
      chart_(new QChart),
      chart_view_(new QChartView(chart_, this)),
      series_(new QLineSeries),
      axis_x_(new QValueAxis),
      axis_y_(new QValueAxis),
      auto_scale_check_(new QCheckBox("Auto scale Y", this)),
      min_spin_(new QDoubleSpinBox(this)),
      max_spin_(new QDoubleSpinBox(this)),
      duration_spin_(new QDoubleSpinBox(this)),
      reset_button_(new QPushButton("Reset history", this)),
      pause_button_(new QPushButton("Pause", this)),
      export_data_button_(new QPushButton("Export data", this)),
      export_image_button_(new QPushButton("Export image", this)),
      render_timer_(new QTimer(this)) {
  chart_->addSeries(series_);
  chart_->legend()->hide();
  chart_->setMargins(QMargins(0, 0, 0, 0));
  chart_->layout()->setContentsMargins(0, 0, 0, 0);
  chart_->setBackgroundRoundness(0);

  axis_x_->setTitleText("Time (s)");
  axis_y_->setTitleText("Value");
  axis_x_->setRange(0.0, static_cast<double>(window_ms_) / 1000.0);
  axis_y_->setRange(0.0, model_->referenceMax());

  chart_->addAxis(axis_x_, Qt::AlignBottom);
  chart_->addAxis(axis_y_, Qt::AlignLeft);
  series_->attachAxis(axis_x_);
  series_->attachAxis(axis_y_);

  chart_view_->setRenderHint(QPainter::Antialiasing, false);

  auto_scale_check_->setChecked(true);

  min_spin_->setRange(-1e9, 1e9);
  min_spin_->setDecimals(3);
  min_spin_->setValue(0.0);

  max_spin_->setRange(-1e9, 1e9);
  max_spin_->setDecimals(3);
  max_spin_->setValue(model_->referenceMax());

  duration_spin_->setRange(0.1, 3600.0);
  duration_spin_->setDecimals(2);
  duration_spin_->setSingleStep(0.5);
  duration_spin_->setValue(static_cast<double>(window_ms_) / 1000.0);

  auto* form = new QFormLayout;
  form->addRow("Y min", min_spin_);
  form->addRow("Y max", max_spin_);
  form->addRow("Duration (s)", duration_spin_);

  auto* controls_buttons = new QGridLayout;
  controls_buttons->addWidget(auto_scale_check_, 0, 0);
  controls_buttons->addWidget(pause_button_, 0, 1);
  controls_buttons->addWidget(export_data_button_, 1, 0);
  controls_buttons->addWidget(export_image_button_, 1, 1);
  controls_buttons->addWidget(reset_button_, 1, 2);

  auto* controls = new QHBoxLayout;
  controls->addLayout(form);
  controls->addLayout(controls_buttons);
  controls->addStretch(1);

  auto* layout = new QVBoxLayout(this);
  layout->addWidget(chart_view_, 1);
  layout->addLayout(controls);

  connect(model_, &MeasurementModel::sampleUpdated, this, &TimeSeriesWidget::onSampleUpdated);
  connect(model_, &MeasurementModel::historyReset, this, &TimeSeriesWidget::onHistoryReset);
  connect(auto_scale_check_, &QCheckBox::toggled, this, &TimeSeriesWidget::onAutoScaleToggled);
  connect(min_spin_, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
          &TimeSeriesWidget::onRangeEdited);
  connect(max_spin_, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
          &TimeSeriesWidget::onRangeEdited);
  connect(duration_spin_, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
          &TimeSeriesWidget::onDurationChanged);
  connect(reset_button_, &QPushButton::clicked, model_, &MeasurementModel::resetHistory);
  connect(pause_button_, &QPushButton::clicked, this, &TimeSeriesWidget::onPauseToggled);
  connect(export_data_button_, &QPushButton::clicked, this, &TimeSeriesWidget::onExportDataClicked);
  connect(export_image_button_, &QPushButton::clicked, this,
          &TimeSeriesWidget::onExportImageClicked);
  connect(render_timer_, &QTimer::timeout, this, &TimeSeriesWidget::renderFrame);

  render_timer_->setInterval(16);
  render_timer_->start();
}

void TimeSeriesWidget::loadSettings(QSettings* settings) {
  settings->beginGroup("time_series");

  auto_scale_check_->setChecked(settings->value("auto_scale", true).toBool());
  min_spin_->setValue(settings->value("y_min", 0.0).toDouble());
  max_spin_->setValue(settings->value("y_max", model_->referenceMax()).toDouble());
  duration_spin_->setValue(settings->value("duration_sec", 10.0).toDouble());
  const bool paused = settings->value("paused", false).toBool();
  if (paused_ != paused) {
    onPauseToggled();
  }

  settings->endGroup();
}

void TimeSeriesWidget::saveSettings(QSettings* settings) const {
  settings->beginGroup("time_series");
  settings->setValue("auto_scale", auto_scale_check_->isChecked());
  settings->setValue("y_min", min_spin_->value());
  settings->setValue("y_max", max_spin_->value());
  settings->setValue("duration_sec", duration_spin_->value());
  settings->setValue("paused", paused_);
  settings->endGroup();
}

void TimeSeriesWidget::onSampleUpdated(double raw_value, double processed_value,
                                       double averaged_value, double, qint64 timestamp_ms) {
  if (paused_) {
    return;
  }

  const SamplePoint sample{timestamp_ms, raw_value, processed_value, averaged_value};
  samples_.append(sample);
  window_samples_.push_back(sample);

  const qint64 min_timestamp_ms = timestamp_ms - window_ms_;
  while (!window_samples_.empty() && window_samples_.front().timestamp_ms < min_timestamp_ms) {
    window_samples_.pop_front();
  }
}

void TimeSeriesWidget::onHistoryReset() {
  series_->clear();
  samples_.clear();
  window_samples_.clear();
  axis_x_->setRange(0.0, static_cast<double>(window_ms_) / 1000.0);
  applyAxisRange();
}

void TimeSeriesWidget::onAutoScaleToggled(bool checked) {
  min_spin_->setEnabled(!checked);
  max_spin_->setEnabled(!checked);

  if (!checked) {
    applyAxisRange();
  }
}

void TimeSeriesWidget::onRangeEdited() {
  if (!auto_scale_check_->isChecked()) {
    applyAxisRange();
  }
}

void TimeSeriesWidget::onDurationChanged(double duration_sec) {
  window_ms_ = static_cast<qint64>(qMax(0.1, duration_sec) * 1000.0);
  const qreal right = window_samples_.empty()
                          ? static_cast<qreal>(window_ms_) / 1000.0
                          : static_cast<qreal>(window_samples_.back().timestamp_ms) / 1000.0;
  const qint64 min_timestamp_ms =
      static_cast<qint64>(right * 1000.0) - static_cast<qint64>(window_ms_);
  while (!window_samples_.empty() && window_samples_.front().timestamp_ms < min_timestamp_ms) {
    window_samples_.pop_front();
  }
  updateXAxisRangeFor(right);
}

void TimeSeriesWidget::onPauseToggled() {
  paused_ = !paused_;
  pause_button_->setText(paused_ ? "Resume" : "Pause");
}

void TimeSeriesWidget::onExportDataClicked() {
  const QString path = QFileDialog::getSaveFileName(this, "Export time series data",
                                                    "time_series.csv", "CSV files (*.csv)");
  if (path.isEmpty()) {
    return;
  }

  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    QMessageBox::warning(this, "Export failed", "Unable to open file for writing.");
    return;
  }

  QTextStream stream(&file);
  stream << "timestamp_ms,raw_value,processed_value\n";
  for (const auto& sample : samples_) {
    stream << sample.timestamp_ms << ',' << sample.raw_value << ',' << sample.processed_value
           << '\n';
  }
}

void TimeSeriesWidget::onExportImageClicked() {
  const QString path = QFileDialog::getSaveFileName(this, "Export chart image", "time_series.png",
                                                    "PNG image (*.png);;PDF document (*.pdf)");
  if (path.isEmpty()) {
    return;
  }

  const QString suffix = QFileInfo(path).suffix().toLower();
  if (suffix == "pdf") {
    QPdfWriter pdf_writer(path);
    pdf_writer.setResolution(300);
    const QSize content_px = chart_view_->size();
    const qreal width_pt = content_px.width() * 72.0 / pdf_writer.resolution();
    const qreal height_pt = content_px.height() * 72.0 / pdf_writer.resolution();
    QPageSize page_size(QSizeF(width_pt, height_pt), QPageSize::Point, "content_size");
    pdf_writer.setPageSize(page_size);
    pdf_writer.setPageMargins(QMarginsF(0, 0, 0, 0));
    QPainter painter(&pdf_writer);
    chart_view_->render(&painter);
    painter.end();
    return;
  }

  if (!chart_view_->grab().save(path, "PNG")) {
    QMessageBox::warning(this, "Export failed", "Unable to save PNG image.");
  }
}

void TimeSeriesWidget::applyAxisRange() {
  double ymin = min_spin_->value();
  double ymax = max_spin_->value();
  if (ymin >= ymax) {
    ymax = ymin + 1.0;
    max_spin_->setValue(ymax);
  }
  axis_y_->setRange(ymin, ymax);
}

void TimeSeriesWidget::updateXAxisRangeFor(qreal right_sec) {
  const qreal width_sec = static_cast<qreal>(window_ms_) / 1000.0;
  const qreal left_sec = qMax<qreal>(0.0, right_sec - width_sec);
  axis_x_->setRange(left_sec, right_sec);
}

void TimeSeriesWidget::renderFrame() {
  if (paused_) {
    return;
  }

  if (window_samples_.empty()) {
    return;
  }

  const qreal right_sec = static_cast<qreal>(window_samples_.back().timestamp_ms) / 1000.0;
  updateXAxisRangeFor(right_sec);

  const QVector<QPointF> points = buildDisplayPoints();
  if (points.isEmpty()) {
    return;
  }

  series_->replace(points);

  if (auto_scale_check_->isChecked()) {
    qreal ymin = points.front().y();
    qreal ymax = ymin;
    for (const auto& p : points) {
      ymin = qMin(ymin, p.y());
      ymax = qMax(ymax, p.y());
    }
    if (qFuzzyCompare(ymin, ymax)) {
      ymin -= 1.0;
      ymax += 1.0;
    }
    axis_y_->setRange(ymin, ymax);
  }
}

QVector<QPointF> TimeSeriesWidget::buildDisplayPoints() const {
  if (window_samples_.empty()) {
    return {};
  }

  const int viewport_width = qMax(64, chart_view_->viewport()->width());
  const int max_display_points = viewport_width * 2;
  const int sample_count = static_cast<int>(window_samples_.size());

  QVector<QPointF> points;
  if (sample_count <= max_display_points) {
    points.reserve(sample_count);
    for (const auto& sample : window_samples_) {
      points.append(
          QPointF(static_cast<qreal>(sample.timestamp_ms) / 1000.0, sample.averaged_value));
    }
    return points;
  }

  const int bucket_size = qMax(1, sample_count / max_display_points);
  points.reserve(max_display_points);

  for (int start = 0; start < sample_count; start += bucket_size) {
    const int end = qMin(sample_count, start + bucket_size);
    if (start >= end) {
      break;
    }

    int min_index = start;
    int max_index = start;
    for (int i = start + 1; i < end; ++i) {
      if (window_samples_[i].averaged_value < window_samples_[min_index].averaged_value) {
        min_index = i;
      }
      if (window_samples_[i].averaged_value > window_samples_[max_index].averaged_value) {
        max_index = i;
      }
    }

    auto append_point = [this, &points](int index) {
      const auto& sample = window_samples_[index];
      points.append(
          QPointF(static_cast<qreal>(sample.timestamp_ms) / 1000.0, sample.averaged_value));
    };

    if (min_index <= max_index) {
      append_point(min_index);
      if (max_index != min_index) {
        append_point(max_index);
      }
    } else {
      append_point(max_index);
      append_point(min_index);
    }
  }

  return points;
}
