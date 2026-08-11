#include "history_settings_dialog.h"

#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QUrl>
#include <QVBoxLayout>

#include "history_manager.h"

HistorySettingsDialog::HistorySettingsDialog(const QString& measurement_id,
                                             const HistoryConfig& config,
                                             const HistoryManager* history, QWidget* parent)
    : QDialog(parent),
      archive_root_edit_(new QLineEdit(config.archive_root, this)),
      flush_interval_spin_(new QSpinBox(this)),
      max_samples_spin_(new QSpinBox(this)) {
  setWindowTitle(QStringLiteral("History Settings"));
  resize(700, sizeHint().height());

  flush_interval_spin_->setRange(1, 3600);
  flush_interval_spin_->setSuffix(QStringLiteral(" s"));
  flush_interval_spin_->setValue(config.flush_interval_sec);

  max_samples_spin_->setRange(10000, 10000000);
  max_samples_spin_->setSingleStep(10000);
  max_samples_spin_->setValue(config.max_samples);

  auto* browse_button = new QPushButton(QStringLiteral("Browse..."), this);
  auto* open_button = new QPushButton(QStringLiteral("Open"), this);
  auto* path_layout = new QHBoxLayout;
  path_layout->addWidget(archive_root_edit_, 1);
  path_layout->addWidget(browse_button);
  path_layout->addWidget(open_button);

  auto* form = new QFormLayout;
  form->addRow(QStringLiteral("Measurement ID"), new QLabel(measurement_id, this));
  form->addRow(QStringLiteral("Archive root"), path_layout);
  form->addRow(QStringLiteral("Append interval"), flush_interval_spin_);
  form->addRow(QStringLiteral("Maximum samples"), max_samples_spin_);

  QString current_file = history ? history->currentHistoryFile() : QString();
  if (current_file.isEmpty()) {
    current_file = QStringLiteral("Starts when reception begins");
  }
  auto* current_file_label = new QLabel(current_file, this);
  current_file_label->setTextInteractionFlags(Qt::TextSelectableByMouse);
  current_file_label->setWordWrap(true);
  form->addRow(QStringLiteral("Current daily file"), current_file_label);
  form->addRow(QStringLiteral("Session ID"),
               new QLabel(history && !history->sessionId().isEmpty()
                              ? history->sessionId()
                              : QStringLiteral("Not receiving"),
                          this));
  form->addRow(QStringLiteral("In-memory samples"),
               new QLabel(QString::number(history ? history->sampleCount() : 0), this));
  form->addRow(QStringLiteral("Pending samples"),
               new QLabel(QString::number(history ? history->pendingSampleCount() : 0), this));
  form->addRow(QStringLiteral("Dropped since clear"),
               new QLabel(QString::number(history ? history->droppedSampleCount() : 0), this));

  auto* buttons =
      new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  auto* layout = new QVBoxLayout(this);
  layout->addLayout(form);
  layout->addWidget(buttons);

  connect(browse_button, &QPushButton::clicked, this,
          &HistorySettingsDialog::browseForDirectory);
  connect(open_button, &QPushButton::clicked, this, &HistorySettingsDialog::openDirectory);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

HistoryConfig HistorySettingsDialog::historyConfig() const {
  HistoryConfig config;
  config.archive_root = archive_root_edit_->text().trimmed();
  config.flush_interval_sec = flush_interval_spin_->value();
  config.max_samples = max_samples_spin_->value();
  return config;
}

void HistorySettingsDialog::browseForDirectory() {
  const QString directory = QFileDialog::getExistingDirectory(
      this, QStringLiteral("Select history archive root"), archive_root_edit_->text());
  if (!directory.isEmpty()) {
    archive_root_edit_->setText(directory);
  }
}

void HistorySettingsDialog::openDirectory() {
  QDesktopServices::openUrl(QUrl::fromLocalFile(archive_root_edit_->text()));
}
