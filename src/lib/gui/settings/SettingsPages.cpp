/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "SettingsPages.h"

#include "AwdlCard.h"
#include "SettingsBinder.h"
#include "SettingsPage.h"
#include "common/ConnectionDiagnosis.h"
#include "common/I18N.h"
#include "common/LogLevel.h"
#include "common/NetworkProtocol.h"
#include "common/PlatformInfo.h"
#include "common/Settings.h"
#include "gui/TlsUtility.h"
#include "gui/awdl/AwdlPlatform.h"
#include "gui/chrome/ChromePolicy.h"
#include "gui/core/NetworkMonitor.h"
#include "gui/style/Appearance.h"
#include "gui/style/LookFollower.h"
#include "gui/widgets/InlineHint.h"
#include "gui/widgets/SectionHeader.h"
#include "gui/widgets/SettingCard.h"
#include "gui/widgets/SettingRow.h"
#include "gui/widgets/StatusChip.h"
#include "gui/widgets/ToggleSwitch.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTextDocument>
#include <QVBoxLayout>

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
// The source of each text is marked at its call (QT_TRANSLATE_NOOP), which is what lets lupdate find it: the helper
// itself cannot be seen by it
Text tx(const char *source)
{
  return [source] { return QCoreApplication::translate("SettingsPages", source); };
}

// The texts that the dialog of the configuration of the client has: the same use as for the dialog of the preferences
Text cc(const char *source)
{
  return [source] { return QCoreApplication::translate("ClientConfigDialog", source); };
}

// The texts that the configuration of the server has (its context): the same use as for the dialog of the preferences
Text sv(const char *source)
{
  return [source] { return QCoreApplication::translate("ServerConfigDialog", source); };
}

// A label of the dialog has a mnemonic and often a colon: a title of a row has neither
QString plainLabel(QString text)
{
  text = withoutMnemonic(text);
  while (text.endsWith(QLatin1Char(':')) || text.endsWith(QLatin1Char(' ')))
    text.chop(1);
  return text;
}

Text svPlain(const char *source)
{
  return [source] { return plainLabel(QCoreApplication::translate("ServerConfigDialog", source)); };
}

Text ccPlain(const char *source)
{
  return [source] { return plainLabel(QCoreApplication::translate("ClientConfigDialog", source)); };
}

// "Experimental: multi-monitor aware cursor mapping" under a card called "Experimental" is "Multi-monitor aware cursor
// mapping"
QString withoutPrefix(const QString &title, const QString &prefix)
{
  const auto lead = prefix + QStringLiteral(": ");
  if (prefix.isEmpty() || !title.startsWith(lead))
    return title;
  auto rest = title.mid(lead.size());
  if (!rest.isEmpty())
    rest[0] = rest[0].toUpper();
  return rest;
}

// The help of a control that is written in rich text, as a line of text
QString plainText(const QString &html)
{
  QTextDocument document;
  document.setHtml(html);
  return document.toPlainText().simplified();
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

// Settings that only a server uses: where this computer is a client the card says so and cannot be changed (the server
// dialog of today is not there for a client either)
void markServerOnly(SettingsPage *page, SettingsBinder *binder, SettingCard *card)
{
  auto *note = new InlineHint;
  note->setObjectName(QStringLiteral("noteServerOnly"));
  page->follow([note] {
    note->setText(
        QCoreApplication::translate("SettingsPages", "These settings are used when this computer is the server.")
    );
  });
  card->addNote(note);
  binder->onRefresh([note, card] {
    const bool client = isClientMode();
    note->setVisible(client);
    card->setEnabled(!client);
  });
}

// Settings that only a client uses: where this computer is a server the cards say so and cannot be changed (the dialog
// of the client is not there for a server either)
void markClientOnly(SettingsPage *page, SettingsBinder *binder, SettingCard *card)
{
  auto *note = new InlineHint;
  note->setObjectName(QStringLiteral("noteClientOnly"));
  page->follow([note] {
    note->setText(
        QCoreApplication::translate("SettingsPages", "These settings are used when this computer is a client.")
    );
  });
  card->addNote(note);
  binder->onRefresh([note, card] {
    const bool client = isClientMode();
    note->setVisible(!client);
    card->setEnabled(client);
  });
}

// A number with decimals, written when the value is taken (and not at each digit that is typed)
QDoubleSpinBox *addDecimal(
    SettingsPage *page, SettingsBinder *binder, SettingCard *card, const QString &key, Text title, double minimum,
    double maximum, double step, int decimals, const QString &suffix, std::function<QString()> description = {},
    const std::function<QVariant(const QVariant &)> &stored = {}
)
{
  auto *row = page->addRow(card, std::move(title), std::move(description));
  auto *spin = new QDoubleSpinBox;
  spin->setRange(minimum, maximum);
  spin->setSingleStep(step);
  spin->setDecimals(decimals);
  spin->setSuffix(suffix);
  spin->setKeyboardTracking(false);
  spin->setMinimumWidth(120);
  row->addControl(spin);
  binder->bind(
      key, spin, spin, &QDoubleSpinBox::valueChanged, [spin] { return QVariant(spin->value()); },
      [spin, stored](const QVariant &value) { spin->setValue((stored ? stored(value) : value).toDouble()); }
  );
  return spin;
}

} // namespace

QString SettingsPages::generalId()
{
  return QStringLiteral("settings.general");
}

QString SettingsPages::clientId()
{
  return QStringLiteral("settings.client");
}

SettingsPages::SettingsPages(QObject *parent, TunnelSettingsDependencies tunnel)
    : QObject(parent),
      m_binder(new SettingsBinder(this)),
      m_tunnel(std::move(tunnel))
{
  connect(m_binder, &SettingsBinder::applied, this, &SettingsPages::applied);

  // the order of the sidebar
  m_entries = {
      {QStringLiteral("settings.security"), QStringLiteral("shield-check"),
       tx(QT_TRANSLATE_NOOP("SettingsPages", "Security & clipboard")), buildSecurity()},
      {QStringLiteral("settings.display"), QStringLiteral("monitor"),
       tx(QT_TRANSLATE_NOOP("SettingsPages", "Display & monitors")), buildDisplay()},
      {QStringLiteral("settings.keyboard"), QStringLiteral("keyboard"),
       tx(QT_TRANSLATE_NOOP("SettingsPages", "Keyboard & mouse")), buildKeyboard()},
      {QStringLiteral("settings.network"), QStringLiteral("network"), sdPlain("&Network"), buildNetwork()},
      // the page of the client is reached from the page of the network and from the home page: one more entry in the
      // sidebar would make the window higher than a small screen has the room for
      {clientId(), QStringLiteral("desktop"), tx(QT_TRANSLATE_NOOP("SettingsPages", "Client")), buildClient(),
       QStringLiteral("settings.network")},
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

TunnelSettingsGroup *SettingsPages::tunnelGroup() const
{
  return m_tunnelGroup;
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
  auto *program = page->addCard(tx(QT_TRANSLATE_NOOP("SettingsPages", "Updates and power")));
  addSwitch(page, m_binder, program, Settings::Gui::AutoUpdateCheck, sd("Check for updates on startup"))
      ->setObjectName(QStringLiteral("cbAutoUpdate"));
  auto *checkRow =
      page->addRow(program, tx(QT_TRANSLATE_NOOP("SettingsPages", "Manual update check")));
  auto *checkNow = new QPushButton;
  const auto checkNowText = tx(QT_TRANSLATE_NOOP("SettingsPages", "Check now"));
  page->follow([checkNow, checkNowText] { checkNow->setText(checkNowText()); });
  checkNow->setObjectName(QStringLiteral("btnCheckForUpdates"));
  checkRow->addControl(checkNow);
  connect(checkNow, &QPushButton::clicked, this, &SettingsPages::checkForUpdatesRequested);
  addSwitch(page, m_binder, program, Settings::Core::PreventSleep, sd("Prevent this computer from going to sleep"))
      ->setObjectName(QStringLiteral("cbPreventSleep"));

  // The look of the application: chosen when the application starts
  auto *look = page->addCard(tx(QT_TRANSLATE_NOOP("SettingsPages", "Appearance")));
  addChoice(
      page, m_binder, look, Settings::Gui::Appearance,
      tx(QT_TRANSLATE_NOOP("SettingsPages", "Look of the application")),
      tx(QT_TRANSLATE_NOOP("SettingsPages", "Takes effect the next time the application starts.")),
      {{tx(QT_TRANSLATE_NOOP("SettingsPages", "Deskflow")), QStringLiteral("deskflow")},
       {tx(QT_TRANSLATE_NOOP("SettingsPages", "System")), QStringLiteral("system")}}
  )->setObjectName(QStringLiteral("comboAppearance"));
  // the frame of the window: the title and the buttons of the window are drawn by the application (on) or by the system
  // (off). The setting says it the other way round (true is the frame of the system), and the default is not the same
  // on every system, so the switch shows what the person prefers (see prefersDeskflowFrame)
  {
    auto *row = page->addRow(
        look, tx(QT_TRANSLATE_NOOP("SettingsPages", "Use the Deskflow window frame")),
        tx(QT_TRANSLATE_NOOP("SettingsPages", "Takes effect the next time the application starts."))
    );
    auto *frame = new ToggleSwitch;
    frame->setObjectName(QStringLiteral("cbDeskflowFrame"));
    row->addControl(frame);
    m_binder->bind(
        Settings::Gui::NativeWindowFrame, frame, frame, &QAbstractButton::toggled,
        [frame] { return QVariant(!frame->isChecked()); },
        [frame](const QVariant &) {
          frame->setChecked(
              deskflow::gui::chrome::prefersDeskflowFrame(deskflow::gui::chrome::ChromeEnvironment::current())
          );
        }
    );
  }
  // the size of text: Automatic is the size that the system gives, as large as the settings of the OS have it
  addChoice(
      page, m_binder, look, Settings::Gui::TextSize, tx(QT_TRANSLATE_NOOP("SettingsPages", "Text size")),
      [] {
        const auto key = [](Qt::Key key) { return QKeySequence(Qt::CTRL | key).toString(QKeySequence::NativeText); };
        return QCoreApplication::translate(
                   "SettingsPages",
                   "Automatic follows the text size of the system. %1 and %2 make the text larger and smaller, and %3 "
                   "goes back to Automatic."
        )
            .arg(key(Qt::Key_Equal), key(Qt::Key_Minus), key(Qt::Key_0));
      },
      {{tx(QT_TRANSLATE_NOOP("SettingsPages", "Smaller (90 %)")), 90},
       {tx(QT_TRANSLATE_NOOP("SettingsPages", "Automatic")), 100},
       {tx(QT_TRANSLATE_NOOP("SettingsPages", "Large (115 %)")), 115},
       {tx(QT_TRANSLATE_NOOP("SettingsPages", "Larger (130 %)")), 130},
       {tx(QT_TRANSLATE_NOOP("SettingsPages", "Largest (150 %)")), 150}}
  )
      ->setObjectName(QStringLiteral("comboTextSize"));
  return page;
}

SettingsPage *SettingsPages::buildSecurity()
{
  auto *page = new SettingsPage(
      QStringLiteral("settings.security"), tx(QT_TRANSLATE_NOOP("SettingsPages", "Security & clipboard"))
  );

  auto *encryption = page->addCard(tx(QT_TRANSLATE_NOOP("SettingsPages", "Encryption")));
  encryption->setObjectName(QStringLiteral("cardEncryption"));
  auto *tls = addSwitch(
      page, m_binder, encryption, Settings::Security::TlsEnabled, sd("Enable TLS Encryption"),
      tx(QT_TRANSLATE_NOOP("SettingsPages", "Both computers must use the same TLS (encryption) setting."))
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
      tx(QT_TRANSLATE_NOOP(
          "SettingsPages", "The server accepts only the clients that have a certificate that it trusts."
      ))
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
  auto *trusted = page->addCard(tx(QT_TRANSLATE_NOOP("SettingsPages", "Trusted computers")));
  trusted->setObjectName(QStringLiteral("cardTrusted"));
  auto *fingerprintRow = page->addRow(
      trusted, tx(QT_TRANSLATE_NOOP("SettingsPages", "Local fingerprint")),
      tx(QT_TRANSLATE_NOOP("SettingsPages", "Compare it with the one shown on the other computer before you trust it."))
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

  // the clipboard that the server shares: on or off, only text, and how large
  auto *clipboard = page->addCard(tx(QT_TRANSLATE_NOOP("SettingsPages", "Clipboard")));
  clipboard->setObjectName(QStringLiteral("cardClipboard"));
  auto *clipboardOn =
      addSwitch(page, m_binder, clipboard, Settings::Server::EnableClipboard, sv("Enable clipboard sharing"));
  clipboardOn->setObjectName(QStringLiteral("cbEnableClipboard"));
  auto *textOnly = addSwitch(
      page, m_binder, clipboard, Settings::Server::ClipboardTextOnly, sv("Share text only (max 10 KB)"),
      sv("Only plain text of up to 10 KB is shared. Images, formatted (HTML) text and longer text are not shared, and "
         "text is never cut short. The size limit above does not apply.")
  );
  textOnly->setObjectName(QStringLiteral("cbClipboardTextOnly"));
  auto *limitRow = page->addRow(clipboard, svPlain("Limit to:"));
  auto *limit = new QSpinBox;
  limit->setObjectName(QStringLiteral("sbClipboardSizeLimit"));
  limit->setRange(0, 10000);
  limit->setSuffix(QStringLiteral(" MB"));
  limitRow->addControl(limit);
  m_binder->bindNumber(Settings::Server::ClipboardSize, limit);
  // text only is a way of sharing clipboards, and it replaces the size limit
  m_binder->onRefresh([=] {
    const bool writable = Settings::isWritable();
    textOnly->setEnabled(writable && clipboardOn->isChecked());
    limit->setEnabled(writable && clipboardOn->isChecked() && !textOnly->isChecked());
  });
  // what the dialog of the server does when the clipboard is switched: a size that is nothing is the usual one, and the
  // permission that the portal of a desktop gave is asked for again
  connect(m_binder, &SettingsBinder::applied, this, [limit](const QString &key) {
    if (key != Settings::Server::EnableClipboard)
      return;
    Settings::setValue(Settings::Server::XdpRestoreToken, QString());
    Settings::setValue(Settings::Server::XdpClipboardRetried, false);
    if (Settings::value(Settings::Server::EnableClipboard).toBool() &&
        Settings::value(Settings::Server::ClipboardSize).toUInt() == 0) {
      const auto usual = Settings::defaultValue(Settings::Server::ClipboardSize).toUInt();
      limit->setValue(usual ? static_cast<int>(usual) : 1);
    }
  });
  markServerOnly(page, m_binder, clipboard);
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
  auto *connection = page->addCard(tx(QT_TRANSLATE_NOOP("SettingsPages", "Connection")));

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

  // the clients are asked now and then whether they are there: how often, and how they speak
  auto *keepAlive = page->addCard(tx(QT_TRANSLATE_NOOP("SettingsPages", "Server")));
  keepAlive->setObjectName(QStringLiteral("cardServerNetwork"));
  auto *heartbeatRow = page->addRow(keepAlive, svPlain("&Check clients every"), [] {
    return plainText(
        QCoreApplication::translate(
            "ServerConfigDialog",
            "<html><head/><body><p>Enables the network heartbeat to ping clients every <span style=\" "
            "font-style:italic;\">n</span> seconds. This may help to diagnose network problems by retrying the "
            "connection if the client becomes unresponsive.</p></body></html>"
        )
    );
  });
  auto *heartbeatNumber = new QSpinBox;
  heartbeatNumber->setObjectName(QStringLiteral("sbHeartbeat"));
  heartbeatNumber->setRange(1000, 30000);
  heartbeatNumber->setSingleStep(1000);
  heartbeatNumber->setSuffix(QStringLiteral(" ms"));
  auto *heartbeat = new ToggleSwitch;
  heartbeat->setObjectName(QStringLiteral("cbHeartbeat"));
  heartbeatRow->addControl(heartbeatNumber);
  heartbeatRow->addControl(heartbeat);
  m_binder->bindSwitch(Settings::Server::EnableHeartbeat, heartbeat);
  m_binder->bindNumber(Settings::Server::Heartbeat, heartbeatNumber);
  m_binder->onRefresh([=] { heartbeatNumber->setEnabled(Settings::isWritable() && heartbeat->isChecked()); });

  auto *protocolRow = page->addRow(keepAlive, svPlain("Network protocol"), [] {
    return plainText(
        QCoreApplication::translate(
            "ServerConfigDialog",
            "<html><head/><body><p>Enables compatibility with programs that use either the Synergy or Barrier "
            "protocols:</p><ul style=\"margin-top: 0px; margin-bottom: 0px; margin-left: 0px; margin-right: 0px; "
            "-qt-list-indent: 1;\"><li style=\" margin-top:12px; margin-bottom:0px; margin-left:0px; "
            "margin-right:0px; -qt-block-indent:0; text-indent:0px;\">Synergy 3 uses the Synergy protocol.</li><li "
            "style=\" margin-top:0px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; "
            "text-indent:0px;\">Barrier, Input-Leap and Synergy 1 uses the Barrier protocol.</li></ul><p>A Deskflow "
            "client will automatically use either the Synergy or Barrier protocol depending on the server "
            "protocol.</p></body></html>"
        )
    );
  });
  auto *protocol = new QComboBox;
  protocol->setObjectName(QStringLiteral("comboProtocol"));
  protocol->setMinimumWidth(180);
  protocol->addItem(QStringLiteral("Synergy"), networkProtocolToOption(NetworkProtocol::Synergy));
  protocol->addItem(QStringLiteral("Barrier"), networkProtocolToOption(NetworkProtocol::Barrier));
  protocolRow->addControl(protocol);
  m_binder->bind(
      Settings::Server::Protocol, protocol, protocol, &QComboBox::activated,
      [protocol] { return protocol->currentData(); },
      [protocol](const QVariant &stored) {
        // the stored word is read the way the server reads it
        protocol->setCurrentIndex(networkProtocolFromString(stored.toString()) == NetworkProtocol::Synergy ? 0 : 1);
      }
  );
  markServerOnly(page, m_binder, keepAlive);

  // the settings of this computer as a client are a page of their own, which has no entry in the sidebar: this is the
  // way to it from the settings (the home page has the other)
  auto *clientCard = page->addCard(tx(QT_TRANSLATE_NOOP("SettingsPages", "Client")));
  clientCard->setObjectName(QStringLiteral("cardClientLink"));
  auto *clientRow = page->addRow(
      clientCard, tx(QT_TRANSLATE_NOOP("SettingsPages", "Client settings")),
      tx(QT_TRANSLATE_NOOP(
          "SettingsPages",
          "Connection timeout, scrolling and the Cloudflare Tunnel, for when this computer is a client."
      ))
  );
  auto *openClient = new QPushButton;
  openClient->setObjectName(QStringLiteral("btnOpenClientSettings"));
  page->follow([openClient] { openClient->setText(QCoreApplication::translate("SettingsPages", "Open")); });
  clientRow->addControl(openClient);
  connect(openClient, &QPushButton::clicked, this, [this] { Q_EMIT pageRequested(clientId()); });

  // only a Mac has the wireless link of AirDrop: nowhere else is the card made, or the setting bound
  if (deskflow::awdl::platformSupported())
    m_awdlCard = new AwdlCard(page, m_binder);
  return page;
}

void SettingsPages::setAwdlController(deskflow::awdl::AwdlController *controller)
{
  if (m_awdlCard != nullptr)
    m_awdlCard->setController(controller);
}

SettingsPage *SettingsPages::buildClient()
{
  auto *page = new SettingsPage(clientId(), tx(QT_TRANSLATE_NOOP("SettingsPages", "Client")));

  // how the client reaches the server and keeps trying
  auto *connection = page->addCard(tx(QT_TRANSLATE_NOOP("SettingsPages", "Connection")));
  connection->setObjectName(QStringLiteral("cardClientConnection"));
  addSwitch(
      page, m_binder, connection, Settings::Client::DynamicConnectionRetry,
      cc("Use dynamic reconnection time based on connection attempts"),
      [] {
        return plainText(
            QCoreApplication::translate(
                "ClientConfigDialog",
                "<html><head/><body><p>Allow the client to slow the rate it attempts to reconnect to the server when "
                "connections attempts are failing. The delay between connection attempts will  start at 1 second "
                "intervals and can be a maxium of 5 minutes between connection attempts.</p></body></html>"
            )
        );
      }
  )->setObjectName(QStringLiteral("cbDynamicConnectTime"));
  addDecimal(
      page, m_binder, connection, Settings::Client::ConnectTimeout, ccPlain("Connect timeout:"), 1.0, 60.0, 0.5, 1,
      QStringLiteral(" s"),
      [] {
        return QCoreApplication::translate(
            "ClientConfigDialog",
            "How long one attempt to connect to the server may take: the connection, the secure handshake and the "
            "server's greeting together. Raise it on a slow or far away network."
        );
      },
      // what the client uses when the settings file holds something that is not a time limit
      [](const QVariant &value) { return QVariant(deskflow::diagnosis::connectTimeoutFromSetting(value).seconds); }
  )->setObjectName(QStringLiteral("sbConnectTimeout"));
  addSwitch(
      page, m_binder, connection, Settings::Client::LanguageSync, cc("Use server's keyboard language on this computer")
  )
      ->setObjectName(QStringLiteral("cbLanguageSync"));

  // the wheel of this computer: every wheel event, inverted and scaled, before what the server may add
  auto *scroll = page->addCard(cc("Local Scroll (this destination computer)"));
  scroll->setObjectName(QStringLiteral("cardClientScroll"));
  auto *scrollNote = new InlineHint;
  page->follow([scrollNote] {
    scrollNote->setText(
        QCoreApplication::translate(
            "ClientConfigDialog", "Applies to every wheel event; can compound with Cross-OS inversion."
        )
    );
  });
  scroll->addNote(scrollNote);
  const auto addScroll = [&](Text title, const QString &invertKey, const QString &scaleKey, const char *name) {
    auto *row = page->addRow(scroll, std::move(title));
    auto *scaleLabel = new QLabel;
    auto *scale = new QDoubleSpinBox;
    scale->setObjectName(QStringLiteral("sb%1ScrollScale").arg(QLatin1String(name)));
    scale->setRange(0.1, 10.0);
    scale->setSingleStep(0.1);
    scale->setKeyboardTracking(false);
    scale->setMinimumWidth(100);
    scaleLabel->setBuddy(scale);
    auto *invertLabel = new QLabel;
    auto *invert = new ToggleSwitch;
    invert->setObjectName(QStringLiteral("cb%1ScrollInvert").arg(QLatin1String(name)));
    page->follow([scaleLabel, invertLabel] {
      scaleLabel->setText(QCoreApplication::translate("ClientConfigDialog", "Scale"));
      invertLabel->setText(QCoreApplication::translate("ClientConfigDialog", "Invert"));
    });
    row->addControl(scaleLabel);
    row->addControl(scale);
    row->addControl(invertLabel);
    row->addControl(invert);
    m_binder->bindSwitch(invertKey, invert);
    m_binder->bind(
        scaleKey, scale, scale, &QDoubleSpinBox::valueChanged, [scale] { return QVariant(scale->value()); },
        [scale](const QVariant &stored) { scale->setValue(stored.toDouble()); }
    );
  };
  addScroll(cc("Vertical Scroll"), Settings::Client::InvertYScroll, Settings::Client::YScrollScale, "Y");
  addScroll(cc("Horizontal Scroll"), Settings::Client::InvertXScroll, Settings::Client::XScrollScale, "X");

  // the addresses that were typed in the field of the main window
  auto *history = page->addCard(tx(QT_TRANSLATE_NOOP("SettingsPages", "Recent servers")));
  history->setObjectName(QStringLiteral("cardClientHistory"));
  auto *historyRow = page->addRow(
      history, cc("Clear Server History"),
      tx(QT_TRANSLATE_NOOP(
          "SettingsPages", "Forget the addresses that were typed in the address field of the main window."
      ))
  );
  auto *clear = new QPushButton;
  clear->setObjectName(QStringLiteral("btnClearServerHistory"));
  page->follow([clear] { clear->setText(QCoreApplication::translate("ClientConfigDialog", "Clear Server History")); });
  historyRow->addControl(clear);
  connect(clear, &QPushButton::clicked, this, [this, clear] {
    Settings::setValue(Settings::Client::RecentRemoteHosts, QVariant());
    clear->setEnabled(false);
    Q_EMIT serverHistoryCleared();
  });
  m_binder->onRefresh([clear] {
    clear->setEnabled(
        Settings::isWritable() && !Settings::value(Settings::Client::RecentRemoteHosts).toStringList().isEmpty()
    );
  });

  markClientOnly(page, m_binder, connection);
  markClientOnly(page, m_binder, scroll);
  markClientOnly(page, m_binder, history);

  // The Cloudflare Tunnel: the group of the dialog of the client, as it is (the same settings, the same checks, the
  // same way that the Service Token is stored). What the person types is not written at each letter: the Secret is
  // stored when the button says so, as the dialog stores it when it is accepted. The group is made when the page is
  // first shown
  auto *tunnel = new SettingCard;
  tunnel->setObjectName(QStringLiteral("cardClientTunnel"));
  page->addCard(tunnel);
  auto *tunnelNote = new InlineHint;
  tunnelNote->setObjectName(QStringLiteral("noteTunnelSaved"));
  page->follow([tunnelNote] {
    tunnelNote->setText(
        QCoreApplication::translate(
            "SettingsPages",
            "The Service Token is kept in the secure store of the system, so the tunnel settings are saved with "
            "the buttons below."
        )
    );
  });
  tunnel->addNote(tunnelNote);
  auto *holder = new QWidget;
  holder->setObjectName(QStringLiteral("tunnelHolder"));
  auto *holderLayout = new QVBoxLayout(holder);
  holderLayout->setContentsMargins(0, 0, 0, 0);
  tunnel->addContent(holder);
  markClientOnly(page, m_binder, tunnel);

  connect(page, &SettingsPage::shown, this, [this, holder, holderLayout] {
    if (m_tunnelGroup)
      return;
    auto *made = new TunnelSettingsGroup(m_tunnel, holder);
    m_tunnelGroup = made;
    made->setFlat(true);
    made->setWritable(Settings::isWritable());
    made->load();
    holderLayout->addWidget(made);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Reset | QDialogButtonBox::RestoreDefaults, holder
    );
    buttons->setObjectName(QStringLiteral("tunnelButtons"));
    buttons->button(QDialogButtonBox::Reset)
        ->setToolTip(QCoreApplication::translate("SettingsDialogButtonBox", "Reset to stored values"));
    buttons->button(QDialogButtonBox::RestoreDefaults)
        ->setToolTip(QCoreApplication::translate("SettingsDialogButtonBox", "Reset to default values"));
    holderLayout->addWidget(buttons);

    const auto update = [made, buttons] {
      const bool writable = Settings::isWritable();
      const bool modified = made->isModified();
      buttons->button(QDialogButtonBox::Save)->setEnabled(writable && modified);
      buttons->button(QDialogButtonBox::Reset)->setEnabled(modified);
      buttons->button(QDialogButtonBox::RestoreDefaults)->setEnabled(writable && !made->isDefault());
    };
    connect(made, &TunnelSettingsGroup::changed, buttons, update);
    connect(Settings::instance(), &Settings::settingsWritableChanged, buttons, [made, update](bool writable) {
      made->setWritable(writable);
      update();
    });
    connect(buttons->button(QDialogButtonBox::Save), &QPushButton::clicked, this, [this, made, update] {
      // before anything is written: what cannot be saved is said, and nothing is written (as the dialog does)
      if (const auto problem = made->validationProblem(); !problem.isEmpty()) {
        made->showError(problem);
        return;
      }
      if (!made->save())
        return;
      made->load();
      update();
      Q_EMIT applied(Settings::Tunnel::Enabled);
      Q_EMIT tunnelSaved();
    });
    connect(buttons->button(QDialogButtonBox::Reset), &QPushButton::clicked, made, [made, update] {
      made->load();
      update();
    });
    connect(buttons->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, made, [made, update] {
      made->resetToDefault();
      update();
    });
    update();
  });
  return page;
}

SettingsPage *SettingsPages::buildDisplay()
{
  auto *page = new SettingsPage(
      QStringLiteral("settings.display"), tx(QT_TRANSLATE_NOOP("SettingsPages", "Display & monitors"))
  );

  // how a pointer that stays at the edge of a screen goes to the next one
  auto *switching = page->addCard(svPlain("&Switching"));
  auto addSwitching = [&](const QString &enableKey, const QString &delayKey, Text title, const char *name) {
    auto *row = page->addRow(switching, std::move(title));
    auto *enable = new ToggleSwitch;
    enable->setObjectName(QStringLiteral("cb%1").arg(QLatin1String(name)));
    auto *delay = new QSpinBox;
    delay->setObjectName(QStringLiteral("sb%1").arg(QLatin1String(name)));
    delay->setRange(10, 10000);
    delay->setSingleStep(10);
    delay->setSuffix(QStringLiteral(" ms"));
    row->addControl(delay);
    row->addControl(enable);
    m_binder->bindSwitch(enableKey, enable);
    m_binder->bindNumber(delayKey, delay);
    m_binder->onRefresh([=] { delay->setEnabled(Settings::isWritable() && enable->isChecked()); });
  };
  addSwitching(
      Settings::Server::EnableSwitchDelay, Settings::Server::SwitchDelay, svPlain("Switch &after waiting"),
      "SwitchDelay"
  );
  addSwitching(
      Settings::Server::EnableSwitchDoubleTap, Settings::Server::SwitchDoubleTap,
      svPlain("Switch on double &tap within"), "SwitchDoubleTap"
  );

  // the experiment says so with a chip as well as with its title
  auto *experimental = page->addCard(sv("Experimental"));
  experimental->setObjectName(QStringLiteral("cardExperimental"));
  auto *betaChip = new StatusChip(QString(), StatusChip::Kind::Warning);
  experimental->header()->setTrailingWidget(betaChip);
  page->follow([betaChip] { betaChip->setText(QCoreApplication::translate("ServerConfigAdvanced", "Beta")); });
  auto *row = page->addRow(
      experimental,
      [] {
        return withoutPrefix(
            plainLabel(
                QCoreApplication::translate("ServerConfigDialog", "Experimental: multi-monitor aware cursor mapping")
            ),
            plainLabel(QCoreApplication::translate("ServerConfigDialog", "Experimental"))
        );
      },
      sv("For computers whose monitors do not make up a rectangle: the pointer is put on a monitor and not in a hole, "
         "and leaves through any edge. It works for the computers that have a monitor layout (in their computer "
         "settings) that is of the screen size they report; nothing changes for the others. This is an experiment: off "
         "= Deskflow behaves exactly as before.")
  );
  auto *multiMonitor = new ToggleSwitch;
  multiMonitor->setObjectName(QStringLiteral("cbExperimentalMultiMonitor"));
  row->addControl(multiMonitor);
  m_binder->bindSwitch(Settings::Server::ExperimentalMultiMonitor, multiMonitor);
  markServerOnly(page, m_binder, switching);
  markServerOnly(page, m_binder, experimental);
  return page;
}

SettingsPage *SettingsPages::buildKeyboard()
{
  auto *page =
      new SettingsPage(QStringLiteral("settings.keyboard"), tx(QT_TRANSLATE_NOOP("SettingsPages", "Keyboard & mouse")));

  auto *lock = page->addCard(tx(QT_TRANSLATE_NOOP("SettingsPages", "Lock to computer")));
  addSwitch(
      page, m_binder, lock, Settings::Server::DisableLockToComputer, sv("Disable lock to computer (scroll lock key)")
  )
      ->setObjectName(QStringLiteral("cbDisableLockToComputer"));
  addSwitch(
      page, m_binder, lock, Settings::Server::DefaultLockToComputerState, sv("Enable lock to computer at startup")
  )
      ->setObjectName(QStringLiteral("cbDefaultLockToComputerState"));

  auto *pointer = page->addCard(tx(QT_TRANSLATE_NOOP("SettingsPages", "Pointer")));
  auto *relative = addSwitch(
      page, m_binder, pointer, Settings::Server::RelativeMouseMoves, svPlain("Use &relative mouse movements")
  );
  relative->setObjectName(QStringLiteral("cbRelativeMouseMoves"));
  // only Windows has a foreground window that a server can take
  if (deskflow::platform::isWindows()) {
    addSwitch(
        page, m_binder, pointer, Settings::Server::Win32KeepForeground,
        svPlain("Don't take &foreground window (Windows only)")
    )
        ->setObjectName(QStringLiteral("cbWin32KeepForeground"));
  }

#ifdef Q_OS_MACOS
  auto *inputIndicator = page->addCard(
      tx(QT_TRANSLATE_NOOP("SettingsPages", "Input source indicator"))
  );
  inputIndicator->setObjectName(QStringLiteral("cardInputIndicator"));
  auto *indicatorEnabled = addSwitch(
      page, m_binder, inputIndicator, Settings::Gui::InputIndicatorEnabled,
      tx(QT_TRANSLATE_NOOP("SettingsPages", "Input source indicator")),
      tx(QT_TRANSLATE_NOOP(
          "SettingsPages",
          "한/A always beside the mouse pointer while the pointer is local"
      ))
  );
  indicatorEnabled->setObjectName(QStringLiteral("cbInputIndicator"));
  auto *indicatorPosition = addChoice(
      page, m_binder, inputIndicator, Settings::Gui::InputIndicatorPosition,
      tx(QT_TRANSLATE_NOOP("SettingsPages", "Position from cursor")),
      {},
      {
          {tx(QT_TRANSLATE_NOOP("SettingsPages", "Bottom right")), QStringLiteral("cursorBottomRight")},
          {tx(QT_TRANSLATE_NOOP("SettingsPages", "Top right")), QStringLiteral("cursorTopRight")},
          {tx(QT_TRANSLATE_NOOP("SettingsPages", "Bottom left")), QStringLiteral("cursorBottomLeft")},
          {tx(QT_TRANSLATE_NOOP("SettingsPages", "Top left")), QStringLiteral("cursorTopLeft")},
      }
  );
  indicatorPosition->setObjectName(QStringLiteral("comboInputIndicatorPosition"));
  auto *indicatorOpacity = addNumber(
      page, m_binder, inputIndicator, Settings::Gui::InputIndicatorOpacityPercent,
      tx(QT_TRANSLATE_NOOP("SettingsPages", "Badge opacity")), 60, 100
  );
  indicatorOpacity->setSuffix(QStringLiteral("%"));
  indicatorOpacity->setObjectName(QStringLiteral("sbInputIndicatorOpacity"));
  auto *indicatorTone = addChoice(
      page, m_binder, inputIndicator, Settings::Gui::InputIndicatorTone,
      tx(QT_TRANSLATE_NOOP("SettingsPages", "Badge color")),
      {},
      {
          {tx(QT_TRANSLATE_NOOP("SettingsPages", "System")), QStringLiteral("automatic")},
          {tx(QT_TRANSLATE_NOOP("SettingsPages", "Light badge")), QStringLiteral("light")},
          {tx(QT_TRANSLATE_NOOP("SettingsPages", "Dark badge")), QStringLiteral("dark")},
      }
  );
  indicatorTone->setObjectName(QStringLiteral("comboInputIndicatorTone"));
  auto *indicatorNote = new InlineHint;
  indicatorNote->setObjectName(QStringLiteral("hintInputIndicatorSafety"));
  page->follow([indicatorNote] {
    indicatorNote->setText(
        QCoreApplication::translate(
            "SettingsPages",
            "Shows the current input-source badge beside the local mouse pointer. "
            "It reads only the input-source identifier and pointer position; it does not read Accessibility focus or "
            "text, key input, caret contents, clipboard contents, application titles, URLs, selections, values, or "
            "screenshots."
        )
    );
  });
  inputIndicator->addNote(indicatorNote);
  m_binder->onRefresh([indicatorEnabled, indicatorPosition, indicatorOpacity, indicatorTone] {
    const bool enabled = Settings::isWritable() && indicatorEnabled->isChecked();
    indicatorPosition->setEnabled(enabled);
    indicatorOpacity->setEnabled(enabled);
    indicatorTone->setEnabled(enabled);
  });
#endif

  markServerOnly(page, m_binder, lock);
  markServerOnly(page, m_binder, pointer);
  return page;
}

SettingsPage *SettingsPages::buildAdvanced()
{
  auto *page = new SettingsPage(QStringLiteral("settings.advanced"), sdPlain("&Advanced"));

  // the background service of Windows: not where there is none, and not in the portable mode
  if (deskflow::platform::isWindows() && !Settings::isPortableMode()) {
    auto *service = page->addCard(tx(QT_TRANSLATE_NOOP("SettingsPages", "Background service")));
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
  auto *commands = page->addCard(tx(QT_TRANSLATE_NOOP("SettingsPages", "Commands")));
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

  // the file that the server reads its configuration from, in place of the computers of the grid
  auto *configFile = page->addCard(tx(QT_TRANSLATE_NOOP("SettingsPages", "Server configuration file")));
  configFile->setObjectName(QStringLiteral("cardConfigFile"));
  auto *external =
      addSwitch(page, m_binder, configFile, Settings::Server::ExternalConfig, sv("Use a server config file"), [] {
        return QCoreApplication::translate(
                   "ServerConfigDialog",
                   "Use a server config file to create complex computer layouts that are not possible with the simple "
                   "grid-based computer layout editor.\n\nEnabling this setting will disable the server config GUI."
        )
            .simplified();
      });
  external->setObjectName(QStringLiteral("cbExternalConfig"));
  auto configPath =
      addPath(page, m_binder, configFile, Settings::Server::ExternalConfigFile, svPlain("Config file path"));
  configPath.edit->setObjectName(QStringLiteral("lineConfigFile"));
  configPath.browse->setObjectName(QStringLiteral("btnBrowseConfigFile"));
  m_binder->setValidator(Settings::Server::ExternalConfigFile, [](const QVariant &value) {
    const auto path = value.toString().trimmed();
    if (path.isEmpty() || QFileInfo::exists(path))
      return QString();
    return QCoreApplication::translate("SettingsPages", "This file does not exist.");
  });
  connect(
      m_binder, &SettingsBinder::validated, configPath.row,
      [row = configPath.row](const QString &key, const QString &message) {
        if (key == Settings::Server::ExternalConfigFile)
          row->setError(message);
      }
  );
  connect(configPath.browse, &QPushButton::clicked, page, [edit = configPath.edit, page] {
    //: %1 is replaced with the application names
    //: (*.conf) and (*.*) should not be translated
    const auto filter =
        QCoreApplication::translate("ServerConfigDialog", "%1 Configurations (*.conf);;All files (*.*)");
    const auto fileName = QFileDialog::getOpenFileName(
        page, QCoreApplication::translate("ServerConfigDialog", "Browse for a config file"), QString(),
        filter.arg(kAppName)
    );
    if (fileName.isEmpty())
      return;
    edit->setText(fileName);
    Q_EMIT edit->editingFinished();
  });
  m_binder->onRefresh([=] {
    const bool on = Settings::isWritable() && external->isChecked();
    configPath.edit->setEnabled(on);
    configPath.browse->setEnabled(on);
  });
  markServerOnly(page, m_binder, configFile);

  // the way back to the settings that a new copy of the program has
  auto *reset = page->addCard(tx(QT_TRANSLATE_NOOP("SettingsPages", "Reset")));
  auto *resetRow = page->addRow(
      reset, sd("Remove all settings"),
      tx(QT_TRANSLATE_NOOP("SettingsPages", "Clears every setting and starts the application again."))
  );
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
