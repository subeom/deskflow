/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "AwdlModel.h"

#include <QRegularExpression>

#include <algorithm>

namespace deskflow::awdl {

namespace {

// the first line of the interface: `awdl0: flags=8863<UP,BROADCAST,...> mtu 1500`
QString interfaceLine(const QString &text)
{
  const auto header = QStringLiteral("%1:").arg(QLatin1String(kInterface));
  const auto lines = text.split(QLatin1Char('\n'));
  for (const auto &line : lines) {
    if (line.startsWith(header))
      return line;
  }
  return {};
}

// "status: active" and "status: inactive" are the other way that ifconfig says it
LinkState stateFromStatusLine(const QString &text)
{
  static const QRegularExpression status(QStringLiteral(R"(^\s*status:\s*(\w+))"), QRegularExpression::MultilineOption);
  const auto match = status.match(text);
  if (!match.hasMatch())
    return LinkState::Unknown;
  const auto word = match.captured(1).toLower();
  if (word == QLatin1String("active"))
    return LinkState::Up;
  if (word == QLatin1String("inactive"))
    return LinkState::Down;
  return LinkState::Unknown;
}

} // namespace

LinkState parseIfconfig(int exitCode, const QString &standardOutput, const QString &standardError)
{
  if (exitCode != 0) {
    // a failed run is not trusted for what it wrote, except that the interface is not there
    const auto said = (standardOutput + QLatin1Char('\n') + standardError).toLower();
    return said.contains(QLatin1String("does not exist")) ? LinkState::Unavailable : LinkState::Unknown;
  }

  QString text = standardOutput;
  text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
  const auto line = interfaceLine(text);
  if (line.isEmpty())
    return LinkState::Unknown;

  static const QRegularExpression flags(QStringLiteral(R"(flags=[0-9a-fA-F]+<([^>]*)>)"));
  if (const auto match = flags.match(line); match.hasMatch())
    return match.captured(1).split(QLatin1Char(',')).contains(QLatin1String("UP")) ? LinkState::Up : LinkState::Down;
  return stateFromStatusLine(text);
}

QString linkStateId(LinkState state)
{
  switch (state) {
  case LinkState::Up:
    return QStringLiteral("up");
  case LinkState::Down:
    return QStringLiteral("down");
  case LinkState::Unavailable:
    return QStringLiteral("unavailable");
  case LinkState::Unknown:
    break;
  }
  return QStringLiteral("unknown");
}

AdminResult parseAdminResult(bool started, int exitCode, const QString &standardError)
{
  if (!started)
    return AdminResult::Failed;
  if (exitCode == 0)
    return AdminResult::Done;
  const auto said = standardError.toLower();
  // osascript: "execution error: User canceled. (-128)"
  if (said.contains(QLatin1String("(-128)")) || said.contains(QLatin1String("user canceled")) ||
      said.contains(QLatin1String("user cancelled")))
    return AdminResult::Cancelled;
  return AdminResult::Failed;
}

QString appleScriptString(const QString &text)
{
  QString literal;
  literal.reserve(text.size() + 2);
  literal += QLatin1Char('"');
  for (int i = 0; i < text.size(); ++i) {
    const QChar c = text.at(i);
    if (c == QLatin1Char('\r') && i + 1 < text.size() && text.at(i + 1) == QLatin1Char('\n'))
      continue;
    if (c == QLatin1Char('\\') || c == QLatin1Char('"'))
      literal += QLatin1Char('\\');
    if (c == QLatin1Char('\n') || c == QLatin1Char('\r') || c == QLatin1Char('\t') || c == QChar(0x2028) ||
        c == QChar(0x2029)) {
      literal += QLatin1Char(' ');
      continue;
    }
    if (c.category() == QChar::Other_Control)
      continue;
    literal += c;
  }
  literal += QLatin1Char('"');
  return literal;
}

QStringList adminArguments(Direction direction, const QString &prompt)
{
  // the command is a constant (the full path of the program, the interface, the direction): the only text that is not
  // is the sentence of the dialog, and it is a string of AppleScript that cannot end
  const auto command =
      QStringLiteral("do shell script \"%1 %2 %3\" with administrator privileges with prompt %4")
          .arg(
              QLatin1String(kIfconfigPath), QLatin1String(kInterface),
              direction == Direction::Down ? QStringLiteral("down") : QStringLiteral("up"), appleScriptString(prompt)
          );
  return {QStringLiteral("-e"), command};
}

QStringList readArguments()
{
  return {QLatin1String(kInterface)};
}

Route parseRoute(const QString &routeOutput)
{
  static const QRegularExpression gateway(
      QStringLiteral(R"(^\s*gateway:\s*(\S+))"), QRegularExpression::MultilineOption
  );
  static const QRegularExpression device(
      QStringLiteral(R"(^\s*interface:\s*(\S+))"), QRegularExpression::MultilineOption
  );
  Route route;
  if (const auto match = gateway.match(routeOutput); match.hasMatch())
    route.gateway = match.captured(1);
  if (const auto match = device.match(routeOutput); match.hasMatch())
    route.interfaceName = match.captured(1);
  return route;
}

bool isLocalGateway(const QString &address)
{
  // four numbers of one to three digits, none of them with a leading zero (010 could be octal): nothing else is an
  // address
  static const QRegularExpression dotted(QStringLiteral(R"(^(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})$)"));
  const auto match = dotted.match(address);
  if (!match.hasMatch())
    return false;
  int octet[4] = {};
  for (int i = 0; i < 4; ++i) {
    const auto text = match.captured(i + 1);
    if (text.size() > 1 && text.startsWith(QLatin1Char('0')))
      return false;
    octet[i] = text.toInt();
    if (octet[i] > 255)
      return false;
  }
  if (octet[0] == 10)
    return true;
  if (octet[0] == 172 && octet[1] >= 16 && octet[1] <= 31)
    return true;
  if (octet[0] == 192 && octet[1] == 168)
    return true;
  // shared address space (carrier grade NAT, and the networks that some phones and routers make)
  if (octet[0] == 100 && octet[1] >= 64 && octet[1] <= 127)
    return true;
  return octet[0] == 169 && octet[1] == 254;
}

bool canProbe(const Route &route)
{
  if (route.kind != LinkKind::WiFi)
    return false;
  // a device of the network (en0, en12): the wireless link itself (awdl0), the low latency one (llw0) and every name
  // that is not a plain device name are not measured
  static const QRegularExpression device(QStringLiteral(R"(^en\d{1,3}$)"));
  if (!device.match(route.interfaceName).hasMatch())
    return false;
  return isLocalGateway(route.gateway);
}

QStringList pingArguments(const Route &route, int count, int intervalMs)
{
  if (!canProbe(route) || count < 1 || count > 100 || intervalMs < 100 || intervalMs > 5000)
    return {};
  return {QStringLiteral("-c"),   QString::number(count),
          QStringLiteral("-i"),   QString::number(intervalMs / 1000.0, 'g', 6),
          QStringLiteral("-n"),   QStringLiteral("-W"),
          QStringLiteral("1000"), QStringLiteral("-b"),
          route.interfaceName,    route.gateway};
}

QList<Sample> parsePing(const QString &pingOutput)
{
  static const QRegularExpression answer(QStringLiteral(R"(icmp_seq=(\d+)\b.*?\btime=([0-9]+(?:\.[0-9]+)?)\s*ms)"));
  static const QRegularExpression loss(QStringLiteral(R"(^Request timeout for icmp_seq (\d+)\s*$)"));
  QList<Sample> samples;
  const auto lines = pingOutput.split(QLatin1Char('\n'));
  for (const auto &raw : lines) {
    const auto line = raw.trimmed();
    if (const auto match = answer.match(line); match.hasMatch()) {
      Sample sample;
      sample.sequence = match.captured(1).toInt();
      sample.rttMs = match.captured(2).toDouble();
      samples.append(sample);
    } else if (const auto lost = loss.match(line); lost.hasMatch()) {
      Sample sample;
      sample.sequence = lost.captured(1).toInt();
      sample.lost = true;
      samples.append(sample);
    }
  }
  return samples;
}

BurstStats analyzeBurst(const QList<Sample> &samples, int sent, int intervalMs)
{
  BurstStats stats;
  QList<double> answers;
  for (const auto &sample : samples) {
    if (!sample.lost)
      answers.append(sample.rttMs);
  }
  stats.received = static_cast<int>(answers.size());
  stats.lost = std::max(0, sent - stats.received);
  stats.pattern = Pattern::NoData;
  if (answers.isEmpty())
    return stats;

  auto sorted = answers;
  std::sort(sorted.begin(), sorted.end());
  const auto middle = sorted.size() / 2;
  stats.medianMs = sorted.size() % 2 ? sorted.at(middle) : (sorted.at(middle - 1) + sorted.at(middle)) / 2.0;
  stats.worstMs = sorted.last();
  // a network that is slow all the time has no spikes: the threshold goes up with the median
  stats.thresholdMs = std::max(20.0, 5.0 * stats.medianMs);

  QList<qint64> spikeTimes;
  for (const auto &sample : samples) {
    if (!sample.lost && sample.rttMs > stats.thresholdMs)
      spikeTimes.append(qint64(sample.sequence) * intervalMs);
  }
  stats.spikes = static_cast<int>(spikeTimes.size());

  // too few answers to say anything of the network
  if (stats.received * 2 < sent || stats.received < 8)
    return stats;

  stats.pattern = Pattern::Clean;
  if (stats.spikes >= 3) {
    stats.pattern = Pattern::Stutter;
    return stats;
  }
  // the spikes that are next to each other (a second at the most) are one event; two events 2.8 to 4.4 s apart are the
  // period of AWDL
  std::sort(spikeTimes.begin(), spikeTimes.end());
  QList<qint64> events;
  qint64 previous = 0;
  for (const auto time : std::as_const(spikeTimes)) {
    if (events.isEmpty() || time - previous > 1000)
      events.append(time);
    previous = time;
  }
  for (int i = 1; i < events.size(); ++i) {
    const auto apart = events.at(i) - events.at(i - 1);
    if (apart >= 2800 && apart <= 4400)
      stats.pattern = Pattern::Stutter;
  }
  return stats;
}

bool shouldProbe(const ProbeInput &input)
{
  if (!input.supported || !input.optionOn || !input.coreRunning || input.link != LinkState::Up)
    return false;
  if (input.askRunning || input.busy)
    return false;
  return input.sinceLastMs < 0 || input.sinceLastMs >= input.everyMs;
}

Summary summarize(LinkState link, LinkKind kind, Pattern latency)
{
  switch (link) {
  case LinkState::Unavailable:
    return Summary::NotAvailable;
  case LinkState::Unknown:
    return Summary::Unknown;
  case LinkState::Down:
    return Summary::Off;
  case LinkState::Up:
    break;
  }
  if (kind == LinkKind::Wired)
    return Summary::OnWired;
  if (kind == LinkKind::Other)
    return Summary::OnNotWiFi;
  switch (latency) {
  case Pattern::NoData:
    return Summary::OnNoData;
  case Pattern::Clean:
    return Summary::OnClean;
  case Pattern::Stutter:
    return Summary::OnStutter;
  case Pattern::NotMeasured:
    break;
  }
  return Summary::OnNotMeasured;
}

PolicyDecision decide(const PolicyInput &input)
{
  PolicyDecision decision;
  // nothing is done, and nothing of the run is kept, off a Mac, with the option off, and while the core is not running
  if (!input.supported || !input.optionOn || !input.coreRunning)
    return decision;

  decision.memory = input.memory;
  // the dialog is open: what is read meanwhile says nothing
  if (input.memory.askRunning)
    return decision;
  // the person turned the link on again: the program does not take it back
  if (input.memory.restoredByUser)
    return decision;
  if (input.link != LinkState::Up)
    return decision;

  if (!input.memory.asked) {
    decision.ask = true;
    decision.memory.asked = true;
    decision.memory.askRunning = true;
  } else {
    // the link being up is no problem by itself: the mouse stutters only while AWDL is in use, and the latency says it
    decision.hint = input.latency == Pattern::Stutter;
  }
  return decision;
}

} // namespace deskflow::awdl
