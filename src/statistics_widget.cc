#include "statistics_widget.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "measurement_model.h"

namespace {
QLabel* make_value_label(QWidget* parent) {
  auto* label = new QLabel("0", parent);
  label->setMinimumWidth(140);
  return label;
}
}  // namespace

StatisticsWidget::StatisticsWidget(MeasurementModel* model, QWidget* parent)
    : QWidget(parent),
      model_(model),
      current_(make_value_label(this)),
      average_(make_value_label(this)),
      stddev_(make_value_label(this)),
      min_(make_value_label(this)),
      max_(make_value_label(this)),
      count_(make_value_label(this)),
      rate_(make_value_label(this)),
      start_button_(new QPushButton("Start", this)),
      stop_button_(new QPushButton("Stop", this)),
      reset_button_(new QPushButton("Reset", this)) {
  auto* form = new QFormLayout;
  form->addRow("Current", current_);
  form->addRow("Average", average_);
  form->addRow("Std dev", stddev_);
  form->addRow("Min", min_);
  form->addRow("Max", max_);
  form->addRow("Count", count_);
  form->addRow("Acquisition rate (Hz)", rate_);

  auto* controls = new QHBoxLayout;
  controls->addWidget(start_button_);
  controls->addWidget(stop_button_);
  controls->addWidget(reset_button_);
  controls->addStretch(1);

  auto* layout = new QVBoxLayout(this);
  layout->addLayout(form);
  layout->addLayout(controls);
  layout->addStretch(1);

  connect(model_, &MeasurementModel::statistics_updated, this,
          &StatisticsWidget::on_statistics_updated);
  connect(start_button_, &QPushButton::clicked, this, &StatisticsWidget::on_start);
  connect(stop_button_, &QPushButton::clicked, this, &StatisticsWidget::on_stop);
  connect(reset_button_, &QPushButton::clicked, this, &StatisticsWidget::on_reset);
}

void StatisticsWidget::on_statistics_updated(const MeasurementModel::Stats& stats) {
  current_->setText(QString::number(stats.current, 'f', 4));
  average_->setText(QString::number(stats.average, 'f', 4));
  stddev_->setText(QString::number(stats.stddev, 'f', 4));
  min_->setText(QString::number(stats.min, 'f', 4));
  max_->setText(QString::number(stats.max, 'f', 4));
  count_->setText(QString::number(stats.count));
  rate_->setText(QString::number(stats.rate_hz, 'f', 1));
}

void StatisticsWidget::on_start() { model_->start_statistics(); }

void StatisticsWidget::on_stop() { model_->stop_statistics(); }

void StatisticsWidget::on_reset() { model_->reset_statistics(); }
