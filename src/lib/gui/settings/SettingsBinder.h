/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QVariant>

#include <functional>

class QAbstractButton;
class QComboBox;
class QLineEdit;
class QSpinBox;
class QWidget;

namespace deskflow::gui::settings {

/*!
 * \brief Ties the controls of the settings pages to the settings: a control that the user changes writes its setting at
 * once (Settings::setValue writes the file), and a setting that changes elsewhere is put back in its control.
 *
 * It says nothing about what a change needs (SettingEffects.h): it tells that a setting was written (applied()), and
 * whoever listens decides (the window shows the banner for a restart of the sharing). What a control may do when
 * another one is off (a path field that is off while its switch is) is the business of the page: it gives a function
 * to onRefresh(), which runs after every change and after the settings are loaded again, once the binder has made the
 * controls writable or not as the settings file is.
 *
 * A value that is the same as the stored one is not written (so nothing is applied, and the file is not touched).
 */
class SettingsBinder : public QObject
{
  Q_OBJECT

public:
  //! The value that the control has, in the form that is stored
  using Reader = std::function<QVariant()>;
  //! Puts the stored value in the control (its signals are blocked while this runs)
  using Writer = std::function<void(const QVariant &)>;

  explicit SettingsBinder(QObject *parent = nullptr);

  //! \p control is the widget that is enabled or not as the settings file is writable; \p signal tells that it changed
  template <typename Sender, typename Signal>
  void bind(const QString &key, QWidget *control, Sender *sender, Signal signal, Reader read, Writer write)
  {
    const int index = add(key, control, std::move(read), std::move(write));
    connect(sender, signal, this, [this, index] { commit(index); });
  }

  //! A switch or a check box: the setting is a boolean
  void bindSwitch(const QString &key, QAbstractButton *button);
  //! A number: the setting is an integer
  void bindNumber(const QString &key, QSpinBox *spin);
  //! A text, written when the field is left or Enter is pressed
  void bindText(const QString &key, QLineEdit *edit);
  /*!
   * A choice: the data of each item is the value that is stored (a boolean, a number, a text, or none, which removes
   * the setting).
   */
  void bindChoice(const QString &key, QComboBox *combo);

  bool isBound(const QString &key) const;
  QStringList keys() const;

  /*!
   * A value is checked before it is written: a function that gives what is wrong with it, or nothing. A value that is
   * not good is not written; validated() says so, and says it is good again (an empty message) when it is written or
   * when the control takes its value from the settings.
   */
  using Validator = std::function<QString(const QVariant &value)>;
  void setValidator(const QString &key, Validator validate);

  //! Every control takes the value of its setting
  void reload();
  //! The control of one setting takes its value
  void reload(const QString &key);

  //! Runs after the controls were loaded or changed: the rules of what is on while another control is off
  void onRefresh(std::function<void()> refresh);
  void refresh();
  //! A setting that the rules of the page depend on and that the page does not bind: when it changes elsewhere, the
  //! rules run again (the mode of the core decides whether a switch for the clients is of use)
  void watch(const QString &key);

  //! Whether two values are the same setting: the stored form is text, the control's form is typed
  static bool sameValue(const QVariant &stored, const QVariant &value);

Q_SIGNALS:
  //! A control changed the setting (and it was written)
  void applied(const QString &key);
  //! What is wrong with the value that was typed for a setting (it was not written), or nothing when it is good
  void validated(const QString &key, const QString &message);

private:
  struct Binding
  {
    QString key;
    QPointer<QWidget> control;
    Reader read;
    Writer write;
    Validator validate;
  };

  int add(const QString &key, QWidget *control, Reader read, Writer write);
  void commit(int index);
  void load(const Binding &binding);

  QList<Binding> m_bindings;
  QList<std::function<void()>> m_refresh;
  QStringList m_watched;
  bool m_committing = false;
};

} // namespace deskflow::gui::settings
