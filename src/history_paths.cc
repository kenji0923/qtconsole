#include "history_paths.h"

#include <QCryptographicHash>
#include <QDir>
#include <QRegularExpression>
#include <QStandardPaths>

QString defaultHistoryRoot() {
  return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation))
      .filePath(QStringLiteral("kshu/qtconsole/history"));
}

QString historyProfileKey(const QString& measurement_id) {
  return QString::fromLatin1(
      QCryptographicHash::hash(measurement_id.toUtf8(), QCryptographicHash::Sha256).toHex());
}

QString sanitizeHistoryComponent(const QString& text) {
  QString sanitized = text.trimmed();
  const QString original = sanitized;
  static const QRegularExpression invalid(QStringLiteral("[<>:\"/\\\\|?*\\x00-\\x1f]"));
  sanitized.replace(invalid, QStringLiteral("_"));
  while (sanitized.endsWith('.') || sanitized.endsWith(' ')) {
    sanitized.chop(1);
  }

  static const QRegularExpression reserved(
      QStringLiteral("^(con|prn|aux|nul|com[1-9]|lpt[1-9])(?:\\..*)?$"),
      QRegularExpression::CaseInsensitiveOption);
  if (reserved.match(sanitized).hasMatch()) {
    sanitized.prepend('_');
  }
  if (sanitized.isEmpty()) {
    sanitized = QStringLiteral("untitled");
  }
  if (sanitized != original) {
    const QByteArray hash =
        QCryptographicHash::hash(original.toUtf8(), QCryptographicHash::Sha256).toHex().left(8);
    sanitized += QStringLiteral("_") + QString::fromLatin1(hash);
  }
  return sanitized;
}

QString historyDirectoryPath(const QString& archive_root, const QString& measurement_id,
                             const QDate& date) {
  QDir root(archive_root);
  return root.filePath(sanitizeHistoryComponent(measurement_id) + '/' +
                       QString::number(date.year()));
}

QString historyFilePath(const QString& archive_root, const QString& measurement_id,
                        const QDate& date) {
  return QDir(historyDirectoryPath(archive_root, measurement_id, date))
      .filePath(date.toString(QStringLiteral("MMdd'.csv'")));
}
