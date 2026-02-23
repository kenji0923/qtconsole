#include "time_series_widget.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QPushButton>
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
      reset_button_(new QPushButton("Reset history", this)) {
  chart_->addSeries(series_);
  chart_->legend()->hide();
  chart_->setTitle("Time Series");

  axis_x_->setTitleText("Time (s)");
  axis_y_->setTitleText("Value");
  axis_x_->setRange(0.0, static_cast<double>(window_ms_) / 1000.0);
  axis_y_->setRange(0.0, model_->reference_max());

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
  max_spin_->setValue(model_->reference_max());

  auto* form = new QFormLayout;
  form->addRow("Y min", min_spin_);
  form->addRow("Y max", max_spin_);

  auto* controls = new QHBoxLayout;
  controls->addWidget(auto_scale_check_);
  controls->addLayout(form);
  controls->addWidget(reset_button_);
  controls->addStretch(1);

  auto* layout = new QVBoxLayout(this);
  layout->addWidget(chart_view_, 1);
  layout->addLayout(controls);

  connect(model_, &MeasurementModel::sample_updated, this, &TimeSeriesWidget::on_sample_updated);
  connect(model_, &MeasurementModel::history_reset, this, &TimeSeriesWidget::on_history_reset);
  connect(auto_scale_check_, &QCheckBox::toggled, this, &TimeSeriesWidget::on_auto_scale_toggled);
  connect(min_spin_, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
          &TimeSeriesWidget::on_range_edited);
  connect(max_spin_, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
          &TimeSeriesWidget::on_range_edited);
  connect(reset_button_, &QPushButton::clicked, model_, &MeasurementModel::reset_history);
}

void TimeSeriesWidget::on_sample_updated(double value, double, qint64 timestamp_ms) {
  series_->append(static_cast<qreal>(timestamp_ms) / 1000.0, value);

  const qreal right = static_cast<qreal>(timestamp_ms) / 1000.0;
  const qreal left = qMax<qreal>(0.0, right - static_cast<qreal>(window_ms_) / 1000.0);
  axis_x_->setRange(left, right);

  while (!series_->points().isEmpty() && series_->points().front().x() < left) {
    series_->remove(0);
  }

  if (auto_scale_check_->isChecked() && !series_->points().isEmpty()) {
    qreal ymin = series_->points().front().y();
    qreal ymax = ymin;
    const auto points = series_->points();
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

void TimeSeriesWidget::on_history_reset() {
  series_->clear();
  axis_x_->setRange(0.0, static_cast<double>(window_ms_) / 1000.0);
  apply_axis_range();
}

void TimeSeriesWidget::on_auto_scale_toggled(bool checked) {
  min_spin_->setEnabled(!checked);
  max_spin_->setEnabled(!checked);

  if (!checked) {
    apply_axis_range();
  }
}

void TimeSeriesWidget::on_range_edited() {
  if (!auto_scale_check_->isChecked()) {
    apply_axis_range();
  }
}

void TimeSeriesWidget::apply_axis_range() {
  double ymin = min_spin_->value();
  double ymax = max_spin_->value();
  if (ymin >= ymax) {
    ymax = ymin + 1.0;
    max_spin_->setValue(ymax);
  }
  axis_y_->setRange(ymin, ymax);
}
