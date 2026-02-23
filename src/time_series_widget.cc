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
      export_image_button_(new QPushButton("Export image", this)) {
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
}

void TimeSeriesWidget::loadSettings(QSettings* settings) {
  settings->beginGroup("time_series");

  auto_scale_check_->setChecked(settings->value("auto_scale", true).toBool());
  min_spin_->setValue(settings->value("y_min", 0.0).toDouble());
  max_spin_->setValue(settings->value("y_max", model_->referenceMax()).toDouble());
  duration_spin_->setValue(settings->value("duration_sec", 10.0).toDouble());

  settings->endGroup();
}

void TimeSeriesWidget::saveSettings(QSettings* settings) const {
  settings->beginGroup("time_series");
  settings->setValue("auto_scale", auto_scale_check_->isChecked());
  settings->setValue("y_min", min_spin_->value());
  settings->setValue("y_max", max_spin_->value());
  settings->setValue("duration_sec", duration_spin_->value());
  settings->endGroup();
}

void TimeSeriesWidget::onSampleUpdated(double raw_value, double processed_value,
                                       double averaged_value, double, qint64 timestamp_ms) {
  if (paused_) {
    return;
  }
  samples_.append({timestamp_ms, raw_value, processed_value, averaged_value});
  series_->append(static_cast<qreal>(timestamp_ms) / 1000.0, averaged_value);

  const qreal right = static_cast<qreal>(timestamp_ms) / 1000.0;
  updateXAxisRangeFor(right);

  const qreal left = axis_x_->min();
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

void TimeSeriesWidget::onHistoryReset() {
  series_->clear();
  samples_.clear();
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
  const qreal right = series_->points().isEmpty() ? static_cast<qreal>(window_ms_) / 1000.0
                                                  : series_->points().back().x();
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
