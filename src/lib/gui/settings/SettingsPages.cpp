/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "SettingsPages.h"

#include "SettingsBinder.h"
#include "SettingsPage.h"
#include "common/I18N.h"
#include "common/LogLevel.h"
#include "common/PlatformInfo.h"
#include "common/Settings.h"
#include "gui/TlsUtility.h"
#include "gui/core/NetworkMonitor.h"
#include "gui/style/Appearance.h"
#include "gui/style/LookFollower.h"
#include "gui/widgets/SettingCard.h"
#include "gui/widgets/SettingRow.h"
#include "gui/widgets/ToggleSwitch.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>

using namespace deskflow::gui::style;

namespace deskflow::gui::settings {

namespace {

using Text = std::function<QString()>;

// The texts that the dialogs of the system look have: the context is theirs, so that what is translated for them is
// translated here. lupdate finds the source in QT_TRANSLATE_NOOP, and the dialogs are what keeps it in the catalogue
Text sd(const char *source)
{
  return [source] { return QCoreApplication::translate("SettingsDialog", source); };
}

// A title of a tab or of a group box has a mnemonic ("&General", "일반(&G)"): a title of a page has none
Text sdPlain(const char *source)
{
  return [source] { return withoutMnemonic(QCoreApplication::translate("SettingsDialog", source)); };
}

// The texts of the pages themselves (English until the translation of this part is done)
Text tx(const char *source)
{
  return [source] { return QCoreApplication::translate("SettingsPages", source); };
}

struct Choice
{
  Text text;
  QVariant data;
};

ToggleSwitch *addSwitch(
    SettingsPage *page, SettingsBinder *binder, SettingCard *card, const QString &key, Text title, Text description = {}
)
{
  auto *row = page->addRow(card, std::move(title), std::move(description));
  auto *toggle = new ToggleSwitch;
  row->addControl(toggle);
  binder->bindSwitch(key, toggle);
  return toggle;
}

QComboBox *addChoice(
    SettingsPage *page, SettingsBinder *binder, SettingCard *card, const QString &key, Text title, Text description,
    const QList<Choice> &choices, SettingRow **rowOut = nullptr
)
{
  auto *row = page->addRow(card, std::move(title), std::move(description));
  auto *combo = new QComboBox;
  combo->setMinimumWidth(180);
  for (const auto &choice : choices)
    combo->addItem(QString(), choice.data);
  page->follow([combo, choices] {
    for (int i = 0; i < choices.size(); ++i)
      combo->setItemText(i, choices[i].text());
  });
  row->addControl(combo);
  binder->bindChoice(key, combo);
  if (rowOut)
    *rowOut = row;
  return combo;
}

QSpinBox *addNumber(
    SettingsPage *page, SettingsBinder *binder, SettingCard *card, const QString &key, Text title, int minimum,
    int maximum
)
{
  auto *row = page->addRow(card, std::move(title));
  auto *spin = new QSpinBox;
  spin->setRange(minimum, maximum);
  spin->setMinimumWidth(120);
  row->addControl(spin);
  binder->bindNumber(key, spin);
  return spin;
}

// A text field with the button that browses for a file next to it
struct PathRow
{
  SettingRow *row = nullptr;
  QLineEdit *edit = nullptr;
  QPushButton *browse = nullptr;
};

PathRow addPath(SettingsPage *page, SettingsBinder *binder, SettingCard *card, const QString &key, Text title)
{
  PathRow path;
  path.row = page->addRow(card, std::move(title));
  path.edit = new QLineEdit;
  path.edit->setMinimumWidth(240);
  path.browse = new QPushButton;
  // the field takes the room that is left, the button only what its text needs
  path.browse->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  page->follow([button = path.browse] { button->setText(QCoreApplication::translate("SettingsPages", "Browse...")); });
  path.row->addControl(path.edit);
  path.row->addControl(path.browse);
  path.row->setExpandingControls(true);
  binder->bindText(key, path.edit);
  return path;
}

// The folder of a file has to be there for the file to be written
QString missingFolder(const QString &path)
{
  if (path.trimmed().isEmpty())
    return {};
  const auto folder = QFileInfo(path).absoluteDir();
  if (folder.exists())
    return {};
  return QCoreApplication::translate("SettingsPages", "The folder of this file does not exist.");
}

bool isClientMode()
{
  return Settings::value(Settings::Core::CoreMode) == Settings::CoreMode::Client;
}

} // namespace

QString SettingsPages::generalId()
{
  return QStringLiteral("settings.general");
}

SettingsPages::SettingsPages(QObject *parent) : QObject(parent), m_binder(new SettingsBinder(this))
{
  connect(m_binder, &SettingsBinder::applied, this, &SettingsPages::applied);

  // the order of the sidebar
  m_entries = {
      {QStringLiteral("settings.security"), QStringLiteral("shield-check"), tx("Security & clipboard"),
       buildSecurity()},
      {QStringLiteral("settings.network"), QStringLiteral("network"), sdPlain("&Network"), buildNetwork()},
      {generalId(), QStringLiteral("gear"), sdPlain("&General"), buildGeneral()},
      {QStringLiteral("settings.advanced"), QStringLiteral("sliders-horizontal"), sdPlain("&Advanced"),
       buildAdvanced()},
  };
  m_binder->refresh();
}

QList<SettingsPages::Entry> SettingsPages::entries() const
{
  return m_entries;
}

SettingsPage *SettingsPages::page(const QString &id) const
{
  for (const auto &entry : m_entries) {
    if (entry.id == id)
      return entry.page;
  }
  return nullptr;
}

SettingsBinder *SettingsPages::binder() const
{
  return m_binder;
}

SettingsPage *SettingsPages::buildGeneral()
{
  auto *page = new SettingsPage(generalId(), sdPlain("&General"));

  // Language: the program is translated at once (I18N::setLanguage writes the setting as well)
  auto *language = page->addCard(sd("Language"));
  auto *languageRow = page->addRow(language, sd("Language"));
  auto *languages = new QComboBox;
  languages->setObjectName(QStringLiteral("comboLanguage"));
  languages->setMinimumWidth(180);
  I18N::reDetectLanguages();
  languages->addItems(I18N::detectedLanguages());
  languageRow->addControl(languages);
  connect(languages, &QComboBox::activated, this, [languages] {
    I18N::setLanguage(I18N::nativeTo639Name(languages->currentText()));
  });
  m_binder->bind(
      Settings::Core::Language, languages, languages, &QComboBox::activated,
      [languages] { return QVariant(I18N::nativeTo639Name(languages->currentText())); },
      [languages](const QVariant &stored) { languages->setCurrentText(I18N::toNativeName(stored.toString())); }
  );

  // The window: when it comes, when it goes, and how it is named
  auto *window = page->addCard(sdPlain("&Window"));
  addChoice(
      page, m_binder, window, Settings::Gui::Autohide, sd("When the application starts"), {},
      {{sd("Show the main window"), false}, {sd("Send to background"), true}}
  )->setObjectName(QStringLiteral("comboAutoHide"));
  addChoice(
      page, m_binder, window, Settings::Gui::CloseToTray, sd("When the main window is closed"), {},
      {{sd("Exit"), false}, {sd("Send to background"), true}}
  )->setObjectName(QStringLiteral("comboCloseToTray"));
  addSwitch(page, m_binder, window, Settings::Gui::ShowVersionInTitle, sd("Include version in the window title"))
      ->setObjectName(QStringLiteral("cbShowVersion"));
  addChoice(
      page, m_binder, window, Settings::Gui::SymbolicTrayIcon, sd("Tray icon style"), {},
      {{sd("Monocolor"), true}, {sd("Colorful"), false}}
  )->setObjectName(QStringLiteral("comboTrayIcon"));

  // The program: updates and sleep
  auto *program = page->addCard(tx("Updates and power"));
  addSwitch(page, m_binder, program, Settings::Gui::AutoUpdateCheck, sd("Check for updates on startup"))
      ->setObjectName(QStringLiteral("cbAutoUpdate"));
  addSwitch(page, m_binder, program, Settings::Core::PreventSleep, sd("Prevent this computer from going to sleep"))
      ->setObjectName(QStringLiteral("cbPreventSleep"));

  // The look of the application: chosen when the application starts
  auto *look = page->addCard(tx("Appearance"));
  addChoice(
      page, m_binder, look, Settings::Gui::Appearance, tx("Look of the application"),
      tx("Takes effect the next time the application starts."),
      {{tx("Deskflow"), QStringLiteral("deskflow")}, {tx("System"), QStringLiteral("system")}}
  )->setObjectName(QStringLiteral("comboAppearance"));
  return page;
}

SettingsPage *SettingsPages::buildSecurity()
{
  auto *page = new SettingsPage(QStringLiteral("settings.security"), tx("Security & clipboard"));

  auto *encryption = page->addCard(tx("Encryption"));
  encryption->setObjectName(QStringLiteral("cardEncryption"));
  auto *tls = addSwitch(
      page, m_binder, encryption, Settings::Security::TlsEnabled, sd("Enable TLS Encryption"),
      tx("Both computers must use the same TLS (encryption) setting.")
  );
  tls->setObjectName(QStringLiteral("cbTlsEnabled"));

  auto certificate = addPath(page, m_binder, encryption, Settings::Security::Certificate, sd("Certificate"));
  certificate.edit->setObjectName(QStringLiteral("lineTlsCertPath"));
  certificate.browse->setObjectName(QStringLiteral("btnTlsCertPath"));
  m_binder->setValidator(Settings::Security::Certificate, [](const QVariant &value) {
    const auto path = value.toString().trimmed();
    if (path.isEmpty())
      return QCoreApplication::translate("SettingsPages", "Enter the path of the certificate file.");
    return missingFolder(path);
  });
  connect(
      m_binder, &SettingsBinder::validated, certificate.row,
      [row = certificate.row](const QString &key, const QString &message) {
        if (key == Settings::Security::Certificate)
          row->setError(message);
      }
  );
  connect(certificate.browse, &QPushButton::clicked, page, [edit = certificate.edit, page] {
    const auto fileName = QFileDialog::getSaveFileName(
        page, QCoreApplication::translate("SettingsDialog", "Select a TLS certificate to use..."), edit->text(),
        QStringLiteral("Cert (*.pem)"), nullptr, QFileDialog::DontConfirmOverwrite
    );
    if (fileName.isEmpty())
      return;
    edit->setText(fileName);
    Q_EMIT edit->editingFinished();
  });

  // the key length and the button that makes a certificate with it
  auto *lengthRow = page->addRow(encryption, sd("Key length"));
  auto *keyLength = new QComboBox;
  keyLength->setObjectName(QStringLiteral("comboTlsKeyLength"));
  keyLength->addItem(QStringLiteral("2048"), 2048);
  keyLength->addItem(QStringLiteral("4096"), 4096);
  auto *regenerate = new QPushButton;
  regenerate->setObjectName(QStringLiteral("btnTlsRegenCert"));
  page->follow([regenerate] {
    regenerate->setText(QCoreApplication::translate("SettingsDialog", "Regenerate certificate"));
  });
  lengthRow->addControl(keyLength);
  lengthRow->addControl(regenerate);
  m_binder->bindChoice(Settings::Security::KeySize, keyLength);
  connect(regenerate, &QPushButton::clicked, page, [page] {
    if (!TlsUtility::generateCertificate())
      return;
    QMessageBox::information(
        page, QCoreApplication::translate("SettingsDialog", "TLS Certificate Regenerated"),
        QCoreApplication::translate("SettingsDialog", "TLS certificate regenerated successfully.")
    );
  });

  auto *peers = addSwitch(
      page, m_binder, encryption, Settings::Security::CheckPeers, sd("Require client certificates"),
      tx("The server accepts only the clients that have a certificate that it trusts.")
  );
  peers->setObjectName(QStringLiteral("cbRequireClientCert"));

  // the rest of the card is of use when the encryption is on, and what is asked of the clients only for a server
  m_binder->watch(Settings::Core::CoreMode);
  m_binder->onRefresh([=] {
    const bool writable = Settings::isWritable();
    const bool on = writable && tls->isChecked();
    certificate.edit->setEnabled(on);
    certificate.browse->setEnabled(on);
    keyLength->setEnabled(on);
    regenerate->setEnabled(on);
    peers->setEnabled(on && !isClientMode());
  });

  // the fingerprint to compare with the one of the other computer
  auto *trusted = page->addCard(tx("Trusted computers"));
  trusted->setObjectName(QStringLiteral("cardTrusted"));
  auto *fingerprintRow = page->addRow(
      trusted, tx("Local fingerprint"), tx("Compare it with the one shown on the other computer before you trust it.")
  );
  auto *fingerprint = new QPushButton;
  fingerprint->setObjectName(QStringLiteral("btnFingerprint"));
  page->follow([fingerprint] {
    fingerprint->setText(QCoreApplication::translate("SettingsPages", "View local fingerprint"));
  });
  fingerprintRow->addControl(fingerprint);
  connect(fingerprint, &QPushButton::clicked, this, &SettingsPages::fingerprintRequested);
  // the card hides itself when none of its rows is shown: the button is what is hidden while there is no encryption
  m_binder->onRefresh([=] { fingerprint->setVisible(tls->isChecked()); });
  return page;
}

void SettingsPages::fillAddresses(QComboBox *combo)
{
  const auto current = combo->currentData();
  combo->clear();
  combo->addItem(QCoreApplication::translate("SettingsDialog", "Automatic"), QVariant());
  for (const auto &address : NetworkMonitor::validAddresses()) {
    if (combo->findData(address) == -1)
      combo->addItem(address, address);
  }
  // an address that is set and is not one of this computer now (a VPN that is off) stays in the list
  if (const auto interface = Settings::value(Settings::Core::Interface).toString();
      !interface.isEmpty() && combo->findData(interface) == -1)
    combo->addItem(interface, interface);
  const int index = current.isValid() ? combo->findData(current) : 0;
  combo->setCurrentIndex(index < 0 ? 0 : index);
}

SettingsPage *SettingsPages::buildNetwork()
{
  auto *page = new SettingsPage(QStringLiteral("settings.network"), sdPlain("&Network"));
  auto *connection = page->addCard(tx("Connection"));

  auto *interfaceRow = page->addRow(connection, sd("Network IP"));
  auto *interfaces = new QComboBox;
  interfaces->setObjectName(QStringLiteral("comboInterface"));
  interfaces->setMinimumWidth(180);
  fillAddresses(interfaces);
  interfaceRow->addControl(interfaces);
  m_binder->bindChoice(Settings::Core::Interface, interfaces);
  page->follow([interfaces] { interfaces->setItemText(0, QCoreApplication::translate("SettingsDialog", "Automatic")); }
  );
  // the addresses of this computer are those of the time that the page is looked at
  connect(page, &SettingsPage::shown, this, [this, interfaces] {
    fillAddresses(interfaces);
    m_binder->reload(Settings::Core::Interface);
  });

  addNumber(page, m_binder, connection, Settings::Core::Port, sd("Port"), 1, 65535)
      ->setObjectName(QStringLiteral("sbPort"));
  return page;
}

SettingsPage *SettingsPages::buildAdvanced()
{
  auto *page = new SettingsPage(QStringLiteral("settings.advanced"), sdPlain("&Advanced"));

  // the background service of Windows: not where there is none, and not in the portable mode
  if (deskflow::platform::isWindows() && !Settings::isPortableMode()) {
    auto *service = page->addCard(tx("Background service"));
    service->setObjectName(QStringLiteral("cardService"));
    auto *row = page->addRow(service, sd("Use background service (daemon)"));
    auto *daemon = new ToggleSwitch;
    daemon->setObjectName(QStringLiteral("cbService"));
    row->addControl(daemon);
    m_binder->bind(
        Settings::Core::ProcessMode, daemon, daemon, &QAbstractButton::toggled,
        [daemon] {
          return QVariant::fromValue(
              daemon->isChecked() ? Settings::ProcessMode::Service : Settings::ProcessMode::Desktop
          );
        },
        [daemon](const QVariant &stored) {
          daemon->setChecked(stored.value<Settings::ProcessMode>() == Settings::ProcessMode::Service);
        }
    );
    auto *elevate = addSwitch(
        page, m_binder, service, Settings::Daemon::Elevate, sd("Always run as system (work at login screen and UAC)")
    );
    elevate->setObjectName(QStringLiteral("cbElevateDaemon"));
    m_binder->onRefresh([=] { elevate->setEnabled(Settings::isWritable() && daemon->isChecked()); });
  }

  // the commands: a switch and the command that it runs
  auto *commands = page->addCard(tx("Commands"));
  auto addCommand = [&](const QString &enableKey, const QString &commandKey, Text title, const char *name) {
    auto *row = page->addRow(commands, std::move(title));
    auto *enable = new ToggleSwitch;
    enable->setObjectName(QStringLiteral("cb%1").arg(QLatin1String(name)));
    auto *command = new QLineEdit;
    command->setObjectName(QStringLiteral("line%1").arg(QLatin1String(name)));
    command->setMinimumWidth(240);
    row->addControl(command);
    row->addControl(enable);
    row->setExpandingControls(true);
    m_binder->bindSwitch(enableKey, enable);
    m_binder->bindText(commandKey, command);
    m_binder->onRefresh([=] { command->setEnabled(Settings::isWritable() && enable->isChecked()); });
  };
  addCommand(
      Settings::Core::EnableEnterCommand, Settings::Core::ComputerEnterCommand, sd("Run command on enter"),
      "CommandEnter"
  );
  addCommand(
      Settings::Core::EnableExitCommand, Settings::Core::ComputerExitCommand, sd("Run command on exit"), "CommandExit"
  );

  // the log: how much, where, and the messages of the window
  auto *logging = page->addCard(sdPlain("&Logs"));
  auto *levelRow = page->addRow(logging, sd("Level"));
  auto *level = new QComboBox;
  level->setObjectName(QStringLiteral("comboLogLevel"));
  level->setMinimumWidth(180);
  const auto options = LogLevel::logLevelOptions();
  for (int i = 0; i < LogLevel::logLevelNames().count(); ++i)
    level->addItem(QString(), options.at(i));
  page->follow([level] {
    const QStringList toolTips = {
        QCoreApplication::translate("SettingsDialog", "Required messages"),
        QCoreApplication::translate("SettingsDialog", "Non-fatal errors"),
        QCoreApplication::translate("SettingsDialog", "General warnings"),
        QCoreApplication::translate("SettingsDialog", "General events [Default]"),
        QCoreApplication::translate("SettingsDialog", "Debug entries"),
        QCoreApplication::translate("SettingsDialog", "Verbose debug output"),
    };
    for (int i = 0; i < level->count(); ++i) {
      level->setItemText(i, LogLevel::toString(i));
      level->setItemData(i, toolTips.value(i), Qt::ToolTipRole);
    }
  });
  levelRow->addControl(level);
  m_binder->bind(
      Settings::Log::Level, level, level, &QComboBox::activated, [level] { return level->currentData(); },
      [level](const QVariant &stored) {
        // the stored level is not always written in the case of the options
        const auto index = level->findData(stored.toString(), Qt::UserRole, Qt::MatchFixedString);
        level->setCurrentIndex(index);
      }
  );
  m_binder->onRefresh([=] {
    const bool higher = level->currentIndex() > static_cast<int>(LogLevel::Level::Info);
    levelRow->setDescription(
        higher ? QCoreApplication::translate(
                     "SettingsDialog", "Using a log level higher than Info may affect performance."
                 )
               : QString()
    );
  });

  auto *toFile = addSwitch(page, m_binder, logging, Settings::Log::ToFile, sd("Log to file"));
  toFile->setObjectName(QStringLiteral("cbLogToFile"));
  auto logPath = addPath(page, m_binder, logging, Settings::Log::File, sd("Log path"));
  logPath.edit->setObjectName(QStringLiteral("lineLogFilename"));
  logPath.browse->setObjectName(QStringLiteral("btnBrowseLog"));
  m_binder->setValidator(Settings::Log::File, [](const QVariant &value) { return missingFolder(value.toString()); });
  connect(
      m_binder, &SettingsBinder::validated, logPath.row,
      [row = logPath.row](const QString &key, const QString &message) {
        if (key == Settings::Log::File)
          row->setError(message);
      }
  );
  connect(logPath.browse, &QPushButton::clicked, page, [edit = logPath.edit, page] {
    const auto fileName = QFileDialog::getSaveFileName(
        page, QCoreApplication::translate("SettingsDialog", "Save log file to..."), edit->text(),
        QStringLiteral("Logs (*.log *.txt)")
    );
    if (fileName.isEmpty())
      return;
    edit->setText(fileName);
    Q_EMIT edit->editingFinished();
  });
  m_binder->onRefresh([=] {
    const bool on = Settings::isWritable() && toFile->isChecked();
    logPath.edit->setEnabled(on);
    logPath.browse->setEnabled(on);
  });
  addSwitch(page, m_binder, logging, Settings::Log::GuiDebug, sd("Enable GUI debug messages"))
      ->setObjectName(QStringLiteral("cbGuiDebug"));

  // the way back to the settings that a new copy of the program has
  auto *reset = page->addCard(tx("Reset"));
  auto *resetRow =
      page->addRow(reset, sd("Remove all settings"), tx("Clears every setting and starts the application again."));
  auto *remove = new QPushButton;
  remove->setObjectName(QStringLiteral("btnClearAllSettings"));
  page->follow([remove] { remove->setText(QCoreApplication::translate("SettingsDialog", "Remove all settings")); });
  resetRow->addControl(remove);
  connect(remove, &QPushButton::clicked, page, [this, page] {
    const auto answer = QMessageBox::question(
        page, QCoreApplication::translate("SettingsDialog", "%1 Clear Settings").arg(kAppName),
        QCoreApplication::translate(
            "SettingsDialog",
            "<p>Are you sure you want to clear all settings and restart %1?</p> <p>This action cannot be undone.</p>"
        )
            .arg(kAppName)
    );
    if (answer == QMessageBox::Yes)
      Q_EMIT removeAllSettingsRequested();
  });
  m_binder->onRefresh([=] { remove->setEnabled(Settings::isWritable()); });
  return page;
}

} // namespace deskflow::gui::settings
