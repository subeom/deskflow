/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "SettingsBinder.h"

#include "SettingEffects.h"
#include "common/Settings.h"

#include <QAbstractButton>
#include <QComboBox>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QSpinBox>

namespace deskflow::gui::settings {

SettingsBinder::SettingsBinder(QObject *parent) : QObject(parent)
{
  connect(Settings::instance(), &Settings::settingsChanged, this, [this](const QString &key) {
    if (m_committing)
      return;
    reload(key);
    if (m_watched.contains(key))
      refresh();
  });
  connect(Settings::instance(), &Settings::settingsWritableChanged, this, [this] { refresh(); });
}

void SettingsBinder::watch(const QString &key)
{
  if (!m_watched.contains(key))
    m_watched << key;
}

bool SettingsBinder::sameValue(const QVariant &stored, const QVariant &value)
{
  if (!value.isValid())
    return !stored.isValid() || stored.toString().isEmpty();
  switch (value.typeId()) {
  case QMetaType::Bool:
    return stored.toBool() == value.toBool();
  case QMetaType::Int:
  case QMetaType::UInt:
  case QMetaType::LongLong:
    return stored.toLongLong() == value.toLongLong();
  case QMetaType::QString:
    return stored.toString() == value.toString();
  default: {
    // a type of the program (an enumeration): the stored text is converted to it
    auto converted = stored;
    return converted.convert(value.metaType()) && converted == value;
  }
  }
}

int SettingsBinder::add(const QString &key, QWidget *control, Reader read, Writer write)
{
  // a setting that a page binds has to say what it needs: a key without an effect is a mistake of the page
  Q_ASSERT_X(effectOf(key).has_value(), "SettingsBinder", qPrintable(key));
  m_bindings.append({key, control, std::move(read), std::move(write), {}});
  load(m_bindings.last());
  return static_cast<int>(m_bindings.size()) - 1;
}

void SettingsBinder::setValidator(const QString &key, Validator validate)
{
  for (auto &binding : m_bindings) {
    if (binding.key == key)
      binding.validate = validate;
  }
}

void SettingsBinder::load(const Binding &binding)
{
  if (!binding.control)
    return;
  {
    QSignalBlocker blocker(binding.control);
    binding.write(Settings::value(binding.key));
  }
  // what was said to be wrong about the value that was typed is not about this one
  if (binding.validate)
    Q_EMIT validated(binding.key, QString());
}

void SettingsBinder::commit(int index)
{
  const auto &binding = m_bindings.at(index);
  if (!binding.control)
    return;
  const auto value = binding.read();
  if (binding.validate) {
    const auto message = binding.validate(value);
    Q_EMIT validated(binding.key, message);
    if (!message.isEmpty())
      return;
  }
  if (sameValue(Settings::value(binding.key), value)) {
    refresh();
    return;
  }
  m_committing = true;
  Settings::setValue(binding.key, value);
  m_committing = false;
  Q_EMIT applied(binding.key);
  refresh();
}

void SettingsBinder::bindSwitch(const QString &key, QAbstractButton *button)
{
  bind(
      key, button, button, &QAbstractButton::toggled, [button] { return QVariant(button->isChecked()); },
      [button](const QVariant &stored) { button->setChecked(stored.toBool()); }
  );
}

void SettingsBinder::bindNumber(const QString &key, QSpinBox *spin)
{
  bind(
      key, spin, spin, &QSpinBox::valueChanged, [spin] { return QVariant(spin->value()); },
      [spin](const QVariant &stored) { spin->setValue(stored.toInt()); }
  );
}

void SettingsBinder::bindText(const QString &key, QLineEdit *edit)
{
  bind(
      key, edit, edit, &QLineEdit::editingFinished, [edit] { return QVariant(edit->text()); },
      [edit](const QVariant &stored) { edit->setText(stored.toString()); }
  );
}

void SettingsBinder::bindChoice(const QString &key, QComboBox *combo)
{
  bind(
      key, combo, combo, &QComboBox::activated, [combo] { return combo->currentData(); },
      [combo](const QVariant &stored) {
        for (int i = 0; i < combo->count(); ++i) {
          if (sameValue(stored, combo->itemData(i))) {
            combo->setCurrentIndex(i);
            return;
          }
        }
        combo->setCurrentIndex(-1);
      }
  );
}

bool SettingsBinder::isBound(const QString &key) const
{
  for (const auto &binding : m_bindings) {
    if (binding.key == key)
      return true;
  }
  return false;
}

QStringList SettingsBinder::keys() const
{
  QStringList keys;
  for (const auto &binding : m_bindings) {
    if (!keys.contains(binding.key))
      keys << binding.key;
  }
  return keys;
}

void SettingsBinder::reload()
{
  for (const auto &binding : std::as_const(m_bindings))
    load(binding);
  refresh();
}

void SettingsBinder::reload(const QString &key)
{
  bool found = false;
  for (const auto &binding : std::as_const(m_bindings)) {
    if (binding.key == key) {
      load(binding);
      found = true;
    }
  }
  if (found)
    refresh();
}

void SettingsBinder::onRefresh(std::function<void()> refresh)
{
  m_refresh.append(std::move(refresh));
  m_refresh.last()();
}

void SettingsBinder::refresh()
{
  const bool writable = Settings::isWritable();
  for (const auto &binding : std::as_const(m_bindings)) {
    if (binding.control)
      binding.control->setEnabled(writable);
  }
  for (const auto &rule : std::as_const(m_refresh))
    rule();
}

} // namespace deskflow::gui::settings
