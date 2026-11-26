# Auto Updater Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace Deskflow's text-only version check with a fail-closed, fork-pinned update pipeline that stages and verifies direct macOS updates, requires explicit confirmation to install, and keeps Windows, Linux, and package-managed macOS manual.

**Architecture:** Build the feature as pure release/integrity/cache policies, an injectable bounded network layer, a `VersionChecker` state coordinator, a shared UI status model, and a macOS-only verifier/transaction helper. The helper remains dormant until one explicit confirmation and completed GUI/core/tunnel shutdown; every transaction has a verified rollback target and readiness handshake.

**Tech Stack:** C++20, Qt 6.12 Core/Network/Widgets/Test, QJson, QCryptographicHash, CMake/Ninja, CTest, macOS `hdiutil`, `ditto`, `codesign`, `plutil`, and `xattr` invoked with argument arrays only.

---

## Safety and execution contract

- Work only in the `feat/auto-updater` worktree branch rooted at the spec commit
  `cf77d2cf3bcba090b7fc20a4593fe881225fb2e7`.
- The source release base is `fork/master` at
  `bb309dfe9ceedab7af3523f2a49837e275b6e843`.
- Never run the updater helper, Deskflow GUI, `deskflow-core`, `cloudflared`, or any real client/server during
  implementation. Never mount or replace `/Applications/Deskflow.app`.
- Build outside the repository at
  `/Users/subeomlee/.copilot/session-state/3cb871c9-7e25-477c-9173-ee473b1d38f4/files/auto-updater-build`.
- Use scratch `HOME`, XDG, and `TMPDIR` directories below the same session folder.
- Build with Release, Qt 6.12.0, `CMAKE_COMPILE_WARNING_AS_ERROR=ON`, named targets only, and
  `SKIP_BUILD_TESTS=ON`.
- Run CTest offscreen with `-E "OSXClipboardTests|OSXKeyStateTests"`.
- Make one focused commit after each completed task.
- Do not push, open a PR, create a tag/release, or touch installed Deskflow.

## File map

| File or group | Responsibility |
| --- | --- |
| `src/lib/gui/UpdateRelease.cpp/.h` | bounded release JSON, stable versions, exact architecture plans, safe checksum manifest |
| `src/lib/gui/UpdateNetwork.cpp/.h` | fixed GitHub URL policy, redirects, response limits, timeout/cancel/rate-limit classification |
| `src/lib/gui/UpdateIntegrity.cpp/.h` | size and SHA-256 checks for API payloads and cache files |
| `src/lib/gui/UpdateCache.cpp/.h` | validated ETag/snapshot state, atomic stage/manifest, stale cleanup, safe path policy |
| `src/lib/gui/UpdateStatusModel.cpp/.h` | one presentation state consumed by all existing UI surfaces |
| `src/lib/gui/VersionChecker.cpp/.h` | release-to-stage state machine with injected network/cache/verifier |
| `src/lib/gui/MacUpdateVerifier.cpp/.h` | read-only DMG validation and pinned self-signed code requirement |
| `src/lib/gui/MacUpdateInstaller.cpp/.h` | direct-install preflight, helper-ready/handshake, bundle swap and rollback |
| `src/lib/gui/UpdateShutdown.cpp/.h` | core/tunnel stop gate that cannot launch the helper before acknowledged shutdown |
| `src/lib/gui/MainWindow.cpp/.h` | UI actions, confirmation, startup check, and shutdown coordinator wiring |
| `src/lib/gui/widgets/StatusBar.cpp/.h` | legacy surface renderer, no update policy |
| `src/lib/gui/shell/MainShell.cpp/.h` | existing bottom-rail update renderer |
| `src/lib/gui/tray/*` | existing badge/row renderer from shared state |
| `src/lib/gui/settings/*`, `dialogs/SettingsDialog.*` | startup switch and `Check now` action in both looks |
| `src/apps/deskflow-gui/deskflow-gui.cpp` | hidden helper/handshake command handling before normal single-instance GUI startup |
| `src/lib/common/{UrlConstants,Settings}.*` | pinned fork endpoint and retired URL override |
| `src/lib/gui/core/CoreProcess.*`, `tunnel/TunnelController.*` | acknowledged update shutdown |
| `.github/workflows/continuous-integration.yml` | exact `SHA256SUMS.txt` release artifact name |
| `docs/HelpMain.md`, `docs/dev/mac-stable-signing.md` | manual behavior and runtime signing policy |
| `translations/deskflow_ko.ts` | new Korean-ready source translations; no other translation files retained |
| `src/unittests/gui/*Update*Tests.*`, `UpdateShutdownTests.*` | pure policies, cache, verifier, transactions, failure injection, and UI state |

## Commit sequence

1. `docs: plan automatic updater`
2. `feat(update): add release integrity model`
3. `feat(update): add bounded GitHub network`
4. `feat(update): add verified staging cache`
5. `feat(update): coordinate update discovery`
6. `feat(update): verify macOS update bundles`
7. `feat(update): add transactional macOS installer`
8. `feat(update): wire update UI and shutdown`
9. `build(update): align release contract and docs`
10. `test(update): close integration failures` only if verification exposes implementation defects

### Task 1: Release model, stable plan, and checksum parser

**Files:**
- Create: `src/lib/gui/UpdateRelease.cpp`
- Create: `src/lib/gui/UpdateRelease.h`
- Create: `src/unittests/gui/UpdateReleaseTests.cpp`
- Create: `src/unittests/gui/UpdateReleaseTests.h`
- Modify: `src/lib/gui/CMakeLists.txt`
- Modify: `src/unittests/gui/CMakeLists.txt`

- [ ] **Step 1: Add failing release parser and planning tests**

Add the following test class and the exact failure-injection slots:

```cpp
class UpdateReleaseTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void latestStableRelease_parsesForkArm64Dmg();
  void latestStableRelease_rejectsDraftPrereleaseAndWrongRepository();
  void latestStableRelease_rejectsDuplicateUnsafeOrMismatchedAssets();
  void latestStableRelease_rejectsMissingOrInvalidDigest();
  void updatePlan_selectsExactArchitectureAndNeverFallbacks();
  void updatePlan_keepsWindowsLinuxAndPackageManagedMacManual();
  void versionComparison_rejectsPrereleaseAndNeverDowngrades();
  void checksumManifest_acceptsOneExactGithubLine();
  void checksumManifest_rejectsDuplicateMissingPathLikeAndMalformedLines();
};
```

The ARM64 fixture uses:

```cpp
QJsonObject{
    {"draft", false},
    {"prerelease", false},
    {"tag_name", "v2.0.6"},
    {"html_url", "https://github.com/subeom/deskflow/releases/tag/v2.0.6"},
    {"assets",
     QJsonArray{
         QJsonObject{
             {"name", "deskflow-2.0.6-macos-arm64.dmg"},
             {"browser_download_url",
              "https://github.com/subeom/deskflow/releases/download/v2.0.6/deskflow-2.0.6-macos-arm64.dmg"},
             {"size", 30437214},
             {"digest", "sha256:38749e16c5971bfefc10278fad8adf8ad57ae2ec3e2a11275be1774a1dfb2fdf"}},
         QJsonObject{
             {"name", "SHA256SUMS.txt"},
             {"browser_download_url",
              "https://github.com/subeom/deskflow/releases/download/v2.0.6/SHA256SUMS.txt"},
             {"size", 97},
             {"digest", "sha256:2a5cb53a2bd6b1d471ea757983c59c63fa65687b4fd09a69424d5ccf260763b6"}}
     }}}
```

Checksum mutation cases include a changed hash, duplicate selected name, `../name`, `subdir/name`, a leading
`./`, NUL, CR alone, a single separator, a duplicate line, and more than 256 lines.

Register the target:

```cmake
create_test(
  NAME UpdateReleaseTests
  DEPENDS gui
  SOURCE UpdateReleaseTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)
```

For this TDD step, do not add the production files yet.

- [ ] **Step 2: Configure a scratch Release build and verify the test target fails to compile**

Use the shared verification commands from Task 9. Configure with `SKIP_BUILD_TESTS=ON`, then run:

```bash
cmake --build "$BUILD" --config Release --parallel 8 --target UpdateReleaseTests
```

Expected: fail because `gui/UpdateRelease.h` and its symbols do not exist.

- [ ] **Step 3: Implement the bounded pure release model**

Define the exact public surface in `UpdateRelease.h`:

```cpp
namespace deskflow::gui {

enum class UpdateInstallMode { Manual, AutomaticMac };

struct UpdateAsset
{
  QString name;
  QUrl downloadUrl;
  QString digest;
  qint64 size = 0;
};

struct UpdateRelease
{
  QString tag;
  QString version;
  QUrl pageUrl;
  QList<UpdateAsset> assets;
};

struct UpdateEnvironment
{
  QString platform;
  QString architecture;
  bool packageManaged = false;
  bool destinationWritable = false;
};

struct UpdatePlan
{
  QString releaseVersion;
  QString tag;
  QUrl releaseUrl;
  UpdateInstallMode mode = UpdateInstallMode::Manual;
  std::optional<UpdateAsset> installer;
  std::optional<UpdateAsset> checksums;
};

struct UpdateReleaseParseResult
{
  std::optional<UpdateRelease> release;
  QString error;
};

struct UpdateChecksumResult
{
  std::optional<QString> checksum;
  QString error;
};

UpdateReleaseParseResult parseGitHubRelease(const QByteArray &json);
std::optional<UpdatePlan> planUpdate(
    const UpdateRelease &release, const UpdateEnvironment &environment
);
bool isNewerUpdateVersion(const QString &current, const QString &candidate);
bool isValidSha256Digest(const QString &digest);
UpdateChecksumResult parseChecksumManifest(
    const QByteArray &payload, const QString &expectedAssetName
);

} // namespace deskflow::gui
```

Implementation rules:

```cpp
constexpr auto kMaximumReleaseBytes = 2LL * 1024 * 1024;
constexpr auto kMaximumChecksumBytes = 2LL * 1024 * 1024;
constexpr auto kMaximumAutomaticInstallerBytes = 512LL * 1024 * 1024;
constexpr qsizetype kMaximumAssets = 128;
constexpr qsizetype kMaximumChecksumLines = 256;
```

- Use `QJsonDocument::fromJson` and reject non-object root, drafts, prereleases, duplicate names, invalid URLs,
  invalid positive 64-bit sizes, and non-`sha256:` digests.
- Accept stable tags only:
  `^v(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$`.
- Use `quint64` components and lexicographic numeric version ordering, never signed `int` conversion.
- Select only
  `deskflow-<version>-macos-arm64.dmg` or `deskflow-<version>-macos-x86_64.dmg`.
- Require case-sensitive `SHA256SUMS.txt` only for `AutomaticMac`.
- Return a manual plan with no installer for package-managed macOS, Windows, Linux, and an unsupported manual
  CPU only; unsupported OS produces no plan.
- Parse every nonempty checksum line. Accept GNU `64hex + two spaces + optional * + safe name`, LF or CRLF, and
  exactly one line for `expectedAssetName`.
- Compare the release page and asset URL as exact `https://github.com/subeom/deskflow/...` URLs.

Add both production files to `src/lib/gui/CMakeLists.txt` beside `VersionChecker`. Add `Qt6::Network` to the
gui library's public link interface so the library that uses `QNetworkAccessManager` owns its dependency.

- [ ] **Step 4: Run the release tests and fix until they pass**

```bash
cmake --build "$BUILD" --config Release --parallel 8 --target UpdateReleaseTests
env QT_QPA_PLATFORM=offscreen HOME="$SCRATCH_HOME" XDG_CACHE_HOME="$XDG_CACHE_HOME" \
  XDG_CONFIG_HOME="$XDG_CONFIG_HOME" TMPDIR="$TMPDIR" \
  ctest --test-dir "$BUILD/src/unittests" -R '^UpdateReleaseTests$' --output-on-failure
```

Expected: 1 test executable, all nine slots pass. No network access occurs.

- [ ] **Step 5: Commit the release model**

```bash
git add src/lib/gui/UpdateRelease.cpp src/lib/gui/UpdateRelease.h \
  src/lib/gui/CMakeLists.txt src/unittests/gui/UpdateReleaseTests.cpp \
  src/unittests/gui/UpdateReleaseTests.h src/unittests/gui/CMakeLists.txt
git commit -m 'feat(update): add release integrity model' \
  -m 'Co-authored-by: Copilot App <223556219+Copilot@users.noreply.github.com>'
```

### Task 2: Fixed URL policy, bounded GitHub network, and ETag metadata

**Files:**
- Create: `src/lib/gui/UpdateNetwork.cpp`
- Create: `src/lib/gui/UpdateNetwork.h`
- Create: `src/unittests/gui/UpdateNetworkTests.cpp`
- Create: `src/unittests/gui/UpdateNetworkTests.h`
- Modify: `src/lib/gui/CMakeLists.txt`
- Modify: `src/unittests/gui/CMakeLists.txt`

- [ ] **Step 1: Add failing URL, redirect, rate-limit, and response-limit tests**

Define:

```cpp
class UpdateNetworkTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void metadataUrl_acceptsOnlyPinnedForkEndpoint();
  void redirectPolicy_rejectsApiRedirectsThirdPartyHostsAndUnsafeUrls();
  void redirectPolicy_acceptsOnlyTerminalGithubAssetHosts();
  void rateLimit_classifies429AndExhausted403ButNotOrdinary403();
  void retryPolicy_allowsOneMetadataOrChecksumTransientRetryOnly();
  void responseLimit_rejectsOversizeBeforeBodyPublication();
  void cancellation_generationInvalidatesLateCallbacks();
};
```

Pure fixtures cover `http`, user info, explicit port, IP literal, fragment, encoded slash/backslash, long URL,
`api.github.com` redirect, three CDN hosts, a fourth redirect, 200, 304, 403/429, 408, 5xx, `ETag`, and
`Retry-After`.

No test opens a socket. `QtUpdateNetwork` gets a fake request/reply adapter only if a later test requires
`QNetworkReply` event timing; policy and limits remain pure.

- [ ] **Step 2: Run the target to observe the expected missing-symbol failure**

```bash
cmake --build "$BUILD" --config Release --parallel 8 --target UpdateNetworkTests
```

Expected: compile fails on `gui/UpdateNetwork.h`.

- [ ] **Step 3: Implement the injectable bounded network layer**

Public API:

```cpp
enum class UpdateRequestKind { Metadata, Checksum, Installer };
enum class UpdateNetworkStatus
{
  Success, NotModified, Cancelled, Timeout, TooLarge, RateLimited,
  NetworkError, HttpError, PolicyError
};

struct UpdateNetworkOptions
{
  qint64 maximumBytes = 2LL * 1024 * 1024;
  int timeoutMs = 30'000;
  int maximumTotalMs = 30'000;
  QString etag;
  UpdateRequestKind kind = UpdateRequestKind::Metadata;
};

struct UpdateNetworkResult
{
  UpdateNetworkStatus status = UpdateNetworkStatus::HttpError;
  int httpStatus = 0;
  QByteArray data;
  QUrl finalUrl;
  QString error;
  QString etag;
  int retryAfterSeconds = 0;
};

using UpdateNetworkCallback = std::function<void(const UpdateNetworkResult &)>;

class IUpdateNetwork
{
public:
  virtual ~IUpdateNetwork() = default;
  virtual quint64 get(
      const QUrl &url, const UpdateNetworkOptions &options,
      UpdateNetworkCallback callback) = 0;
  virtual void cancel(quint64 requestId) = 0;
  virtual void cancelAll() = 0;
};

bool isAllowedUpdateUrl(const QUrl &url, UpdateRequestKind kind);
bool isAllowedUpdateRedirect(
    const QUrl &source, const QUrl &redirect, UpdateRequestKind kind,
    int redirectCount);
```

`QtUpdateNetwork`:

- creates `QNetworkAccessManager` as its only network implementation;
- uses manual redirect policy;
- fixes `Accept: application/vnd.github+json` and
  `X-GitHub-Api-Version: 2022-11-28`;
- sends only `User-Agent: Deskflow/<kVersion>`;
- sends `If-None-Match` only for Metadata;
- rejects URL length over 8 KiB, non-HTTPS, non-default ports, fragments, credentials, encoded separators,
  and normalized dot segments;
- allows no Metadata redirect;
- allows Checksum/Installer at most three transitions: exact `github.com/subeom/deskflow/releases/download/...`
  to one terminal `release-assets.githubusercontent.com`, `objects.githubusercontent.com`, or
  `github-releases.githubusercontent.com` URL;
- never follows a terminal-host redirect;
- cancels on `readyRead` when accumulated bytes exceed the option limit;
- classifies 304, 429, exhausted 403, 408/5xx, timeout, and other HTTP errors;
- parses and clamps rate retry to 24 hours;
- stores only host/path in errors and never logs or returns a signed CDN query in user-facing error text;
- increments and invalidates generation/request IDs on cancellation.

Expose pure helpers for URL and HTTP classification so tests do not start a socket.

- [ ] **Step 4: Run focused tests and inspect every URL/header assertion**

```bash
cmake --build "$BUILD" --config Release --parallel 8 --target UpdateNetworkTests
env QT_QPA_PLATFORM=offscreen HOME="$SCRATCH_HOME" XDG_CACHE_HOME="$XDG_CACHE_HOME" \
  XDG_CONFIG_HOME="$XDG_CONFIG_HOME" TMPDIR="$TMPDIR" \
  ctest --test-dir "$BUILD/src/unittests" -R '^UpdateNetworkTests$' --output-on-failure
```

Expected: 1 test executable, seven slots pass, including no query retention.

- [ ] **Step 5: Commit bounded networking**

```bash
git add src/lib/gui/UpdateNetwork.cpp src/lib/gui/UpdateNetwork.h \
  src/lib/gui/CMakeLists.txt src/unittests/gui/UpdateNetworkTests.cpp \
  src/unittests/gui/UpdateNetworkTests.h src/unittests/gui/CMakeLists.txt
git commit -m 'feat(update): add bounded GitHub network' \
  -m 'Co-authored-by: Copilot App <223556219+Copilot@users.noreply.github.com>'
```

### Task 3: Integrity, safe staging cache, ETag snapshot, and stale cleanup

**Files:**
- Create: `src/lib/gui/UpdateIntegrity.cpp`
- Create: `src/lib/gui/UpdateIntegrity.h`
- Create: `src/lib/gui/UpdateCache.cpp`
- Create: `src/lib/gui/UpdateCache.h`
- Create: `src/unittests/gui/UpdateIntegrityTests.cpp`
- Create: `src/unittests/gui/UpdateIntegrityTests.h`
- Create: `src/unittests/gui/UpdateCacheTests.cpp`
- Create: `src/unittests/gui/UpdateCacheTests.h`
- Modify: `src/lib/gui/CMakeLists.txt`
- Modify: `src/unittests/gui/CMakeLists.txt`

- [ ] **Step 1: Add failing integrity and cache mutation tests**

`UpdateIntegrityTests` slots:

```cpp
void payload_acceptsExactSizeDigestAndChecksum();
void payload_rejectsOneByteSizeDigestAndChecksumMutations();
void checksumAsset_matchesParsedManifestExactly();
```

`UpdateCacheTests` slots:

```cpp
void stage_publishesAtomicallyAndReloadReverifies();
void stage_rejectsCorruptOrUnexpectedPayload();
void find_rejectsWrongPlanManifestDigestSizeVersionAndRepository();
void load_rejectsSymlinkRootStageManifestInstallerAndChecksum();
void load_rejectsEscapedPathsHardlinksAndUnsafeModes();
void discard_removesOnlyAValidatedStageChild();
void state_roundTripsEtagSnapshotAndRateLimitWithoutRawBody();
void cleanup_removesExpiredInvalidIncomingAndOverQuotaStages();
```

Use `QTemporaryDir` only. Symlink tests are compiled on Unix/macOS. Hardlink tests use `QFile::link()` and
POSIX `lstat` behavior only where available. No test uses real QStandardPaths or a user's cache.

- [ ] **Step 2: Build both targets and confirm missing production symbols**

```bash
cmake --build "$BUILD" --config Release --parallel 8 --target \
  UpdateIntegrityTests UpdateCacheTests
```

Expected: fail on `UpdateIntegrity.h` or `UpdateCache.h`.

- [ ] **Step 3: Implement integrity verification and the cache**

`UpdateIntegrity.h`:

```cpp
struct UpdateIntegrityResult
{
  bool ok = false;
  QString error;
};

UpdateIntegrityResult verifyUpdatePayload(
    const UpdateAsset &asset, const QByteArray &payload,
    const std::optional<QByteArray> &checksums = std::nullopt);
```

`UpdateIntegrity.cpp` requires exact byte count, `sha256:` syntax, lowercase computed SHA-256, and
`parseChecksumManifest` equality.

`UpdateCache.h`:

```cpp
struct StagedUpdate
{
  QString version;
  QString installerName;
  QString installerPath;
  QString manifestPath;
};

struct UpdateCacheState
{
  int schemaVersion = 1;
  QString endpoint;
  QString etag;
  QByteArray sanitizedSnapshot;
  QString snapshotVersion;
  QDateTime retryAt;
  QDateTime lastSuccessfulCheck;
};

class UpdateCache
{
public:
  explicit UpdateCache(QString rootPath);
  std::optional<StagedUpdate> find(const UpdatePlan &plan) const;
  static std::optional<StagedUpdate> load(const QString &manifestPath);
  UpdateStageResult stage(
      const UpdatePlan &plan, const QByteArray &installer,
      const std::optional<QByteArray> &checksums);
  bool discard(const StagedUpdate &staged);
  std::optional<UpdateCacheState> loadState() const;
  bool saveState(const UpdateCacheState &state);
  bool cleanup(const QString &currentVersion);
};
```

The implementation:

- creates the injected cache root as mode 0700; each staged file is mode 0600;
- uses `.incoming-<uuid>` and `QSaveFile`, then one directory rename;
- writes compact manifest v1 with `repository=subeom/deskflow`, exact release fields, both asset hashes/sizes,
  selected checksum, stage ID, and UTC creation time;
- reconstructs paths from the root and version; it never trusts a manifest path field;
- rejects symbolic links at every component, nonregular files, hardlink count not equal to 1, canonical paths
  outside root, unsafe filename/path separators, wrong ownership, and group/world write access;
- rehashes DMG and `SHA256SUMS.txt` on every `find`, `load`, and installer preflight;
- retains at most one valid stage plus one incoming stage, removes invalid/over-30-day/superseded stages, and
  stops cache use on cleanup error;
- stores sanitized JSON from parser output, never the raw GitHub body;
- keeps ETag, retry time, and last successful check bounded and endpoint-bound.

Register `Q_DECLARE_METATYPE(deskflow::gui::StagedUpdate)`.

- [ ] **Step 4: Run the focused integrity/cache tests**

```bash
cmake --build "$BUILD" --config Release --parallel 8 --target \
  UpdateIntegrityTests UpdateCacheTests
env QT_QPA_PLATFORM=offscreen HOME="$SCRATCH_HOME" XDG_CACHE_HOME="$XDG_CACHE_HOME" \
  XDG_CONFIG_HOME="$XDG_CONFIG_HOME" TMPDIR="$TMPDIR" \
  ctest --test-dir "$BUILD/src/unittests" \
  -R '^(UpdateIntegrityTests|UpdateCacheTests)$' --output-on-failure
```

Expected: two executables, eleven integrity/cache slots total, all pass.

- [ ] **Step 5: Commit integrity and cache**

```bash
git add src/lib/gui/UpdateIntegrity.* src/lib/gui/UpdateCache.* \
  src/lib/gui/CMakeLists.txt src/unittests/gui/UpdateIntegrityTests.* \
  src/unittests/gui/UpdateCacheTests.* src/unittests/gui/CMakeLists.txt
git commit -m 'feat(update): add verified staging cache' \
  -m 'Co-authored-by: Copilot App <223556219+Copilot@users.noreply.github.com>'
```

### Task 4: `VersionChecker` state machine and shared presentation model

**Files:**
- Modify: `src/lib/gui/VersionChecker.cpp`
- Modify: `src/lib/gui/VersionChecker.h`
- Create: `src/lib/gui/UpdateStatusModel.cpp`
- Create: `src/lib/gui/UpdateStatusModel.h`
- Create: `src/lib/gui/UpdateVerifier.cpp`
- Create: `src/lib/gui/UpdateVerifier.h`
- Create: `src/unittests/gui/VersionCheckerTests.cpp`
- Create: `src/unittests/gui/VersionCheckerTests.h`
- Create: `src/unittests/gui/UpdateStatusModelTests.cpp`
- Create: `src/unittests/gui/UpdateStatusModelTests.h`
- Modify: `src/lib/gui/CMakeLists.txt`
- Modify: `src/unittests/gui/CMakeLists.txt`

- [ ] **Step 1: Add failing state-machine tests with fake network/cache/verifier**

`VersionCheckerTests` uses a fake network, a real `UpdateCache` rooted in `QTemporaryDir`, and a fake verifier.
Its slots:

```cpp
void checkLatest_deduplicatesInFlightAndUsesPinnedEndpoint();
void checkLatest_upToDateUsesOnlyBoundedCachedSnapshotOn304();
void checkLatest_rateLimitPersistsRetryAndMakesNoImmediateRetry();
void checkLatest_timeoutMakesOnlyOneMetadataRetry();
void checkLatest_manualPlatformsNeverDownload();
void checkLatest_directMacDownloadsChecksumsThenDmgThenVerifies();
void checkLatest_cancelInvalidatesLateNetworkCallbacks();
void checkLatest_discardsWrongIntegrityAndPlatformVerification();
void checkLatest_missingExactArchitectureNeverFallsBack();
void checkLatest_reusesOnlyAReverifiedStage();
```

Define fakes:

```cpp
class FakeUpdateNetwork final : public IUpdateNetwork
{
public:
  QList<QPair<QUrl, UpdateNetworkOptions>> calls;
  UpdateNetworkCallback callback;
  quint64 get(...) override;
  void complete(const UpdateNetworkResult &result);
  void cancel(quint64) override;
  void cancelAll() override;
};
```

`UpdateStatusModelTests` verifies that one snapshot produces Manual, Staged, Failed, Checking, Applying, and
UpToDate presentation, that failure clears any staged path, and that the Korean-visible source strings remain
translatable actions rather than preformatted error bodies.

- [ ] **Step 2: Verify missing coordinator/model symbols**

```bash
cmake --build "$BUILD" --config Release --parallel 8 --target \
  VersionCheckerTests UpdateStatusModelTests
```

Expected: fail because the old `VersionChecker` only exposes `replyFinished` and `updateFound`.

- [ ] **Step 3: Replace the checker with the exact state machine**

`VersionChecker.h` exposes:

```cpp
enum class UpdateCheckReason { Startup, Manual };

struct UpdateStatusSnapshot
{
  enum class Phase {
    Idle, CheckingRelease, UpToDate, ManualAvailable, DownloadingChecksums,
    DownloadingInstaller, Verifying, Staged, Confirming, Applying,
    RollingBack, Failed
  };
  Phase phase = Phase::Idle;
  QString version;
  QUrl releaseUrl;
  QString userError;
  QString retryText;
  bool installable = false;
};

class VersionChecker : public QObject
{
  Q_OBJECT
public:
  explicit VersionChecker(QObject *parent = nullptr);
  VersionChecker(
      std::unique_ptr<IUpdateNetwork> network,
      UpdateEnvironment environment,
      std::unique_ptr<UpdateCache> cache,
      std::unique_ptr<IUpdateVerifier> verifier,
      QObject *parent = nullptr);
  void checkLatest(UpdateCheckReason reason = UpdateCheckReason::Startup);
  void cancel();
  bool isChecking() const;
  const UpdateStatusSnapshot &status() const;
  const std::optional<StagedUpdate> &staged() const;
Q_SIGNALS:
  void statusChanged(const UpdateStatusSnapshot &status);
  void manualUpdateAvailable(const QUrl &releaseUrl, const QString &version);
  void stagedUpdateAvailable(const StagedUpdate &staged);
};
```

Flow:

1. startup/manual enters `CheckingRelease` and ignores duplicate calls;
2. metadata request uses compiled endpoint, ETag, 2 MiB cap, generation token, and 304 snapshot reparse;
3. parser/plan failure enters `Failed`;
4. candidate not newer enters `UpToDate`;
5. manual plan enters `ManualAvailable` and never starts payload requests;
6. automatic Mac checks/reverifies cache, otherwise downloads `SHA256SUMS.txt`, verifies it, downloads the
   exact DMG, verifies it, stages, and calls the platform verifier;
7. successful stage enters `Staged`; all errors clear staged metadata and enter `Failed`;
8. cancellation invalidates late callbacks;
9. retry eligibility comes from `UpdateCache::loadState`; 429/exhausted 403 never retry in-process.

The production constructor derives platform with `QSysInfo`, direct/package ownership, and
`QStandardPaths::CacheLocation`. It is allowed to return manual on every uncertain ownership result.

For this task, `UpdateVerifier.cpp` defines `createPlatformUpdateVerifier()` as an empty factory on every
platform. Task 5 replaces that stub with the production macOS verifier. This keeps Task 4 buildable while all
Task 4 automatic-mac tests inject their fake verifier.

`UpdateStatusModel` has no network/filesystem access. It converts `VersionChecker` snapshots plus installer
progress into immutable `UpdateStatusView` values and emits one `changed` signal:

```cpp
enum class UpdateAction { None, OpenRelease, ConfirmInstall, Retry };
struct UpdateStatusView
{
  QString version;
  QString label;
  QString toolTip;
  bool enabled = false;
  bool visible = false;
  bool installable = false;
  UpdateAction action = UpdateAction::None;
};
```

- [ ] **Step 4: Run coordinator/model tests**

```bash
cmake --build "$BUILD" --config Release --parallel 8 --target \
  VersionCheckerTests UpdateStatusModelTests
env QT_QPA_PLATFORM=offscreen HOME="$SCRATCH_HOME" XDG_CACHE_HOME="$XDG_CACHE_HOME" \
  XDG_CONFIG_HOME="$XDG_CONFIG_HOME" TMPDIR="$TMPDIR" \
  ctest --test-dir "$BUILD/src/unittests" \
  -R '^(VersionCheckerTests|UpdateStatusModelTests)$' --output-on-failure
```

Expected: ten `VersionChecker` slots and six model slots pass.

- [ ] **Step 5: Commit the discovery coordinator**

```bash
git add src/lib/gui/VersionChecker.* src/lib/gui/UpdateStatusModel.* \
  src/lib/gui/UpdateVerifier.cpp src/lib/gui/UpdateVerifier.h src/lib/gui/CMakeLists.txt \
  src/unittests/gui/VersionCheckerTests.* src/unittests/gui/UpdateStatusModelTests.* \
  src/unittests/gui/CMakeLists.txt
git commit -m 'feat(update): coordinate update discovery' \
  -m 'Co-authored-by: Copilot App <223556219+Copilot@users.noreply.github.com>'
```

### Task 5: macOS verifier for the pinned self-signed fork identity

**Files:**
- Create: `src/lib/gui/MacUpdateVerifier.cpp`
- Create: `src/lib/gui/MacUpdateVerifier.h`
- Create: `src/unittests/gui/UpdateVerifierTests.cpp`
- Create: `src/unittests/gui/UpdateVerifierTests.h`
- Create: `src/unittests/gui/MacUpdateIntegrationTests.cpp`
- Create: `src/unittests/gui/MacUpdateIntegrationTests.h`
- Modify: `src/lib/gui/UpdateVerifier.cpp`
- Modify: `src/lib/gui/UpdateVerifier.h`
- Modify: `src/lib/gui/CMakeLists.txt`
- Modify: `src/unittests/gui/CMakeLists.txt`

- [ ] **Step 1: Add failing signature-evidence and isolated DMG tests**

`UpdateVerifierTests`:

```cpp
void signatureEvidence_acceptsPinnedForkIdentity();
void signatureEvidence_rejectsWrongBundleWrongLeafAdHocAndDeveloperId();
void signatureEvidence_rejectsNotarizationOnlyAndMissingCore();
void verifier_mountsCopiesChecksVersionsAndAlwaysDetaches();
void verifier_failureStopsAfterOneBoundedDiagnostic();
```

The accepted fake evidence must contain:

```text
Identifier=org.deskflow.deskflow
certificate leaf = H"39b1c21df0ba75cdde058fcbb09a1ad38aef8055"
Executable=.../Deskflow.app/Contents/MacOS/Deskflow
```

Reject upstream Team `5QCJCX8C37`, `source=Notarized Developer ID`, ad hoc, wrong leaf, wrong bundle ID, and
missing `deskflow-core`.

`MacUpdateIntegrationTests` creates only `QTemporaryDir` content:

```text
fixture/Deskflow.app/Contents/Info.plist
fixture/Deskflow.app/Contents/MacOS/Deskflow
fixture/Deskflow.app/Contents/MacOS/deskflow-core
fixture.dmg
mount/
copy/
```

It creates a synthetic DMG with `/usr/bin/hdiutil`, uses a command runner that executes real `hdiutil` and
`ditto` but fakes only signature evidence, and verifies mount/detach/copy/cleanup. It never signs with, reads,
or modifies any real keychain.

- [ ] **Step 2: Verify the tests fail before the verifier exists**

```bash
cmake --build "$BUILD" --config Release --parallel 8 --target \
  UpdateVerifierTests MacUpdateIntegrationTests
```

Expected: missing verifier headers/symbols.

- [ ] **Step 3: Implement `MacUpdateVerifier` without notarization assumptions**

`IUpdateCommandRunner`:

```cpp
struct UpdateCommandResult
{
  bool started = false;
  bool timedOut = false;
  int exitCode = -1;
  QByteArray standardOutput;
  QByteArray standardError;
};

class IUpdateCommandRunner
{
public:
  virtual ~IUpdateCommandRunner() = default;
  virtual UpdateCommandResult run(
      const QString &program, const QStringList &arguments, int timeoutMs) = 0;
};
```

Verifier rules:

- accept only a regular non-symlink staged `.dmg`;
- mount with `/usr/bin/hdiutil attach -readonly -nobrowse -noautoopen -mountpoint ...`;
- require `Deskflow.app` directly under the mount point, no symlink, and no second app;
- copy with `/usr/bin/ditto` to a private `QTemporaryDir`;
- read bundle ID/version using `/usr/bin/plutil`;
- run:

```text
/usr/bin/codesign --verify --deep --strict \
  --test-requirement='=identifier "org.deskflow.deskflow" and certificate leaf = H"39b1c21df0ba75cdde058fcbb09a1ad38aef8055"' \
  Deskflow.app
```

- verify both executable paths are strict signed nested code and pinned by the same leaf;
- reject Team ID/Developer ID evidence and never call `/usr/sbin/spctl`;
- bound command output at 64 KiB and user diagnostic at 240 characters;
- detach with `-force` on every exit path and fail if detach itself fails;
- verify `CFBundleShortVersionString` and `CFBundleVersion` exactly equal the selected stable version.

`createPlatformUpdateVerifier()` returns `MacUpdateVerifier` only with `Q_OS_MACOS`; all other OSes return
`nullptr`.

- [ ] **Step 4: Run pure and isolated integration tests**

```bash
cmake --build "$BUILD" --config Release --parallel 8 --target \
  UpdateVerifierTests MacUpdateIntegrationTests
env QT_QPA_PLATFORM=offscreen HOME="$SCRATCH_HOME" XDG_CACHE_HOME="$XDG_CACHE_HOME" \
  XDG_CONFIG_HOME="$XDG_CONFIG_HOME" TMPDIR="$TMPDIR" \
  ctest --test-dir "$BUILD/src/unittests" \
  -R '^(UpdateVerifierTests|MacUpdateIntegrationTests)$' --output-on-failure
```

Expected: all pure slots and the isolated DMG mount/copy test pass. No real keychain or installed app is used.

- [ ] **Step 5: Commit the macOS verifier**

```bash
git add src/lib/gui/MacUpdateVerifier.* src/lib/gui/UpdateVerifier.cpp src/lib/gui/UpdateVerifier.h \
  src/lib/gui/CMakeLists.txt src/unittests/gui/UpdateVerifierTests.* \
  src/unittests/gui/MacUpdateIntegrationTests.* src/unittests/gui/CMakeLists.txt
git commit -m 'feat(update): verify macOS update bundles' \
  -m 'Co-authored-by: Copilot App <223556219+Copilot@users.noreply.github.com>'
```

### Task 6: Detached macOS helper, transaction receipt, swap, rollback, and relaunch

**Files:**
- Create: `src/lib/gui/UpdateInstaller.h`
- Create: `src/lib/gui/MacUpdateInstaller.cpp`
- Create: `src/lib/gui/MacUpdateInstaller.h`
- Create: `src/unittests/gui/UpdateInstallerTests.cpp`
- Create: `src/unittests/gui/UpdateInstallerTests.h`
- Modify: `src/apps/deskflow-gui/deskflow-gui.cpp`
- Modify: `src/lib/gui/CMakeLists.txt`
- Modify: `src/unittests/gui/CMakeLists.txt`

- [ ] **Step 1: Add failing transaction and relaunch failure-injection tests**

`UpdateInstallerTests` slots:

```cpp
void launch_requiresExplicitConfirmationAndHelperReady();
void launch_refusesPackageManagedSymlinkMountedUnwritableAndWrongExecutable();
void launch_bindsManifestReceiptPidAndProcessStartIdentity();
void apply_waitsForParentExitAndShutdownReceipt();
void apply_reverifiesCopiesAndAtomicallySwaps();
void apply_removesQuarantineOnlyAfterCandidateVerification();
void apply_rollsBackForEveryCopyVerifyRenameLaunchHandshakeFailure();
void apply_keepsOldBackupWhenRollbackRenameOrRelaunchFails();
void apply_requiresMatchingPidAndHandshakeToken();
void commandLine_refusesMissingOrConflictingHelperArguments();
```

Use fake interfaces with operation logs:

```cpp
class FakeUpdateApplyOperations final : public IUpdateApplyOperations
{
public:
  QList<QString> operations;
  QSet<QString> failOperation;
  UpdateInstallResult waitForParent(...);
  UpdateInstallResult verifyDiskImage(...);
  UpdateInstallResult mountDiskImage(...);
  UpdateInstallResult copyApplication(...);
  UpdateInstallResult removeQuarantine(...);
  UpdateInstallResult verifyApplication(...);
  UpdateInstallResult detachDiskImage(...);
  UpdateInstallResult rename(...);
  UpdateInstallResult launchAndWait(...);
};
```

Assertions verify the old executable is the rollback target at every failure and that candidate launch
alone never marks success.

- [ ] **Step 2: Run the installer target and confirm missing transaction APIs**

```bash
cmake --build "$BUILD" --config Release --parallel 8 --target UpdateInstallerTests
```

Expected: fail on `gui/MacUpdateInstaller.h`.

- [ ] **Step 3: Implement preflight, helper mode, and transactional apply**

`UpdateInstaller.h`:

```cpp
struct UpdateInstallResult
{
  bool ok = false;
  QString error;
};

class IUpdateInstaller
{
public:
  virtual ~IUpdateInstaller() = default;
  virtual UpdateInstallResult preflight(const StagedUpdate &staged) = 0;
  virtual UpdateInstallResult start(
      const StagedUpdate &staged, bool explicitConfirmation) = 0;
};

std::unique_ptr<IUpdateInstaller> createPlatformUpdateInstaller();
UpdateInstallResult applyPlatformUpdate(
    const QString &manifestPath, qint64 parentPid,
    const QString &parentStartIdentity, const QString &receiptPath);
```

`MacUpdateInstaller::preflight` requires:

- explicit direct `Deskflow.app/Contents/MacOS/Deskflow` mapping;
- lexical/canonical path equality and no `/Volumes`;
- no Homebrew/Caskroom/Cellar/MacPorts/package receipt or unknown owner;
- writable bundle and parent directory without elevation;
- `ProcessMode::Desktop`;
- one valid stage/manifest;
- pinned candidate signature/version;
- one transaction lock;
- free space `2 * DMG size + 128 MiB`.

`start`:

- refuses `explicitConfirmation != true`;
- writes an initial mode-0600 receipt with random transaction token, parent PID, process start identity,
  canonical manifest path, and stage hashes;
- starts the same app absolute executable with:

```text
--apply-update=<manifest>
--update-parent-pid=<pid>
--update-parent-start=<identity>
--update-receipt=<receipt>
```

- waits no more than five seconds for `helper-ready` and returns failure if the PID/token do not match.

The helper:

- branches before shared memory, `MainWindow`, tray, accessibility check, network, or core startup;
- waits for matching `shutdown-complete`;
- waits for the captured parent PID/start identity to end;
- re-loads and re-verifies the stage;
- creates unique sibling `candidate`, `backup`, and `candidate-failed` paths under the same parent;
- copies and verifies the candidate before `/usr/bin/xattr -dr com.apple.quarantine`;
- mounts/detaches the DMG and always cleans its mount point;
- commits with two exact renames;
- launches the absolute new executable with `--update-receipt` and `--update-handshake`;
- waits 30 seconds for an atomic `ready/pid/token` handshake;
- restores old backup and relaunches old if any post-commit step fails;
- removes backup/stage only after new readiness;
- preserves the old backup and writes a blocking recovery status if old relaunch also fails.

No QProcess receives a shell command. All external operations use absolute program paths and argument lists.

- [ ] **Step 4: Run installer transaction tests**

```bash
cmake --build "$BUILD" --config Release --parallel 8 --target UpdateInstallerTests
env QT_QPA_PLATFORM=offscreen HOME="$SCRATCH_HOME" XDG_CACHE_HOME="$XDG_CACHE_HOME" \
  XDG_CONFIG_HOME="$XDG_CONFIG_HOME" TMPDIR="$TMPDIR" \
  ctest --test-dir "$BUILD/src/unittests" -R '^UpdateInstallerTests$' \
  --output-on-failure
```

Expected: ten slots pass for every mutation and rollback action.

- [ ] **Step 5: Commit the macOS transaction helper**

```bash
git add src/lib/gui/UpdateInstaller.h src/lib/gui/MacUpdateInstaller.* \
  src/apps/deskflow-gui/deskflow-gui.cpp src/lib/gui/CMakeLists.txt \
  src/unittests/gui/UpdateInstallerTests.* src/unittests/gui/CMakeLists.txt
git commit -m 'feat(update): add transactional macOS installer' \
  -m 'Co-authored-by: Copilot App <223556219+Copilot@users.noreply.github.com>'
```

### Task 7: Shared UI, Settings, shutdown gate, helper handoff, and Korean strings

**Files:**
- Create: `src/lib/gui/UpdateShutdown.cpp`
- Create: `src/lib/gui/UpdateShutdown.h`
- Create: `src/unittests/gui/UpdateShutdownTests.cpp`
- Create: `src/unittests/gui/UpdateShutdownTests.h`
- Create: `src/unittests/gui/widgets/StatusBarTests.cpp`
- Create: `src/unittests/gui/widgets/StatusBarTests.h`
- Modify: `src/lib/gui/MainWindow.cpp/.h`
- Modify: `src/lib/gui/core/CoreProcess.cpp/.h`
- Modify: `src/lib/gui/tunnel/TunnelController.cpp/.h`
- Modify: `src/lib/gui/widgets/StatusBar.cpp/.h`
- Modify: `src/lib/gui/shell/MainShell.cpp/.h`
- Modify: `src/lib/gui/tray/TrayMenuModel.cpp/.h`
- Modify: `src/lib/gui/settings/SettingsPages.cpp/.h`
- Modify: `src/lib/gui/dialogs/SettingsDialog.cpp`
- Modify: `src/lib/gui/dialogs/SettingsDialog.ui`
- Modify: `translations/deskflow_ko.ts`
- Modify existing GUI tests and CMake registrations

- [ ] **Step 1: Add failing shutdown/UI/settings/translation tests**

`UpdateShutdownTests` slots:

```cpp
void shutdown_waitsForCoreAndTunnelBeforeHelper();
void shutdown_timeoutCancelsApplyAndRestoresRunningState();
void shutdown_ignoresLateCoreAndTunnelSignalsAfterCancellation();
void shutdown_writesCompleteReceiptOnlyOnce();
```

Extend `StatusBarTests`/`MainShellTests` to use one `UpdateStatusView` and assert:

- Manual, Staged, Failed, Checking, and Applying labels/actions;
- no install action unless `installable`;
- one rail action and one tray fact row;
- no extra banner/card at minimum width and 150% text.

Register the new `StatusBarTests` in `src/unittests/gui/widgets/CMakeLists.txt` with:

```cmake
create_test(
  NAME StatusBarTests
  DEPENDS gui
  LIBS server ${gui_server_libs}
  SOURCE StatusBarTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)
target_include_directories(StatusBarTests PRIVATE "${CMAKE_SOURCE_DIR}/src/lib/gui")
```

Extend `SettingsPagesTests`:

- startup switch remains in General `Updates and power`;
- `Check now` emits one request;
- no update URL input exists.

Extend translation tests to assert every updater source key is present in Korean and no test source string uses
an em dash.

- [ ] **Step 2: Build the update/UI test set and observe failures**

```bash
cmake --build "$BUILD" --config Release --parallel 8 --target \
  UpdateShutdownTests StatusBarTests MainShellTests TrayMenuModelTests \
  SettingsPagesTests TranslationFileTests
```

Expected: missing `UpdateShutdown` and update model/render state slots.

- [ ] **Step 3: Implement the acknowledged shutdown coordinator**

`UpdateShutdown` uses injected callbacks and is not coupled to widgets:

```cpp
struct UpdateShutdownTargets
{
  std::function<void()> stopCore;
  std::function<void()> stopTunnel;
  std::function<void()> restoreCore;
  std::function<void()> restoreTunnel;
  std::function<bool()> coreStopped;
  std::function<bool()> tunnelStopped;
};

class UpdateShutdown : public QObject
{
public:
  void begin(const UpdateShutdownTargets &targets, const QString &receiptPath);
  void coreStateChanged();
  void tunnelStateChanged();
  void cancel();
Q_SIGNALS:
  void completed();
  void failed(const QString &boundedReason);
};
```

- Start a 15-second core timer and 10-second tunnel timer only after the helper is ready.
- Stop core first, then tunnel, preserving the current process mode and whether sharing was running.
- Emit `completed` only when `ProcessState::Stopped` and tunnel `Idle`.
- On timeout, cancel helper/transaction, restore the previous core/tunnel run state, and emit failure.
- Write `shutdown-complete` exactly once after settings/log flush, never from a timeout path.

`CoreProcess` adds an update stop method and completion signal; `TunnelController` exposes completion for its
existing shutdown. Normal close, quit, clear-settings, and regular restart behavior remains unchanged.

- [ ] **Step 4: Wire one shared UI status source**

`MainWindow`:

- starts the checker at `open()` only when `gui/enableUpdateCheck` is enabled, after the existing first-run
  consent;
- receives `Check now` from both settings implementations;
- owns `UpdateStatusModel`, checker, installer, and `UpdateShutdown`;
- opens the exact release page only from an explicit `OpenRelease` action;
- opens one confirmation dialog from `ConfirmInstall`, with Cancel as default and the only affirmative label
  `Install and restart`;
- preflights and starts helper, waits for `helper-ready`, then invokes the shutdown gate;
- never closes immediately on helper launch;
- reads a transaction receipt on relaunch and displays `previous version restored` once before a fresh check.

`StatusBar`, `MainShell`, and `TrayMenuModel` no longer infer updater state independently. They render
`UpdateStatusView`:

| View | label | action |
| --- | --- | --- |
| Manual | `View update` | `OpenRelease` |
| Staged | `Install update` | `ConfirmInstall` |
| Failed | `Update check failed` or `Update failed` | `Retry` |
| Checking | `Checking for updates...` | disabled |
| Applying | `Installing update...` | disabled |

The existing tray icon dot and row remain. No new card, page, banner, or OS notification is added.

- [ ] **Step 5: Add Settings actions and Korean translations**

In `SettingsPages::buildGeneral`, keep `cbAutoUpdate` and add an existing-style button row named
`btnCheckForUpdates` in the `Updates and power` card. The button emits
`SettingsPages::checkForUpdatesRequested`.

In the legacy `.ui`, add a `QPushButton` beside `cbAutoUpdate` with the same object name and connect it in
`SettingsDialog.cpp` to a `checkForUpdatesRequested` signal. `MainWindow` connects either surface to
`VersionChecker::checkLatest(Manual)`.

Keep source strings in English and add Korean entries to `translations/deskflow_ko.ts` for:

```text
Check now
Checking for updates...
Deskflow is up to date
View update
Update v%1 is verified and ready to install
Install update
Install Deskflow v%1?
Deskflow will stop sharing, replace the app, and restart.
Install and restart
Updating Deskflow...
Update failed
The previous Deskflow version was restored.
This installation is managed by a package manager. Update it from the release page or package manager.
```

Do not run lupdate in a mode that changes all translation catalogs. If a tool changes any `.ts` file other than
`deskflow_ko.ts`, restore only those unintended files to `HEAD`.

- [ ] **Step 6: Run focused UI/shutdown/translation tests**

```bash
cmake --build "$BUILD" --config Release --parallel 8 --target \
  UpdateShutdownTests StatusBarTests MainShellTests TrayMenuModelTests \
  SettingsPagesTests TranslationFileTests
env QT_QPA_PLATFORM=offscreen HOME="$SCRATCH_HOME" XDG_CACHE_HOME="$XDG_CACHE_HOME" \
  XDG_CONFIG_HOME="$XDG_CONFIG_HOME" TMPDIR="$TMPDIR" \
  ctest --test-dir "$BUILD/src/unittests" \
  -R '^(UpdateShutdownTests|StatusBarTests|MainShellTests|TrayMenuModelTests|SettingsPagesTests|TranslationFileTests)$' \
  --output-on-failure
```

Expected: all targeted executables pass and the redesigned window retains its minimum layout.

- [ ] **Step 7: Commit UI and shutdown integration**

```bash
git add src/lib/gui/UpdateShutdown.* src/lib/gui/MainWindow.* \
  src/lib/gui/core/CoreProcess.* src/lib/gui/tunnel/TunnelController.* \
  src/lib/gui/widgets/StatusBar.* src/lib/gui/shell/MainShell.* \
  src/lib/gui/tray/TrayMenuModel.* src/lib/gui/settings/SettingsPages.* \
  src/lib/gui/dialogs/SettingsDialog.cpp src/lib/gui/dialogs/SettingsDialog.ui \
  src/lib/gui/CMakeLists.txt src/unittests/gui/CMakeLists.txt \
  src/unittests/gui/UpdateShutdownTests.* \
  src/unittests/gui/widgets/StatusBarTests.* \
  src/unittests/gui/widgets/CMakeLists.txt \
  src/unittests/gui/shell/MainShellTests.* src/unittests/gui/tray/TrayMenuModelTests.* \
  src/unittests/gui/settings/SettingsPagesTests.* src/unittests/gui/shell/TranslationFileTests.* \
  translations/deskflow_ko.ts
git commit -m 'feat(update): wire update UI and shutdown' \
  -m 'Co-authored-by: Copilot App <223556219+Copilot@users.noreply.github.com>'
```

### Task 8: Pinned endpoint migration, release checksums, help, and signing docs

**Files:**
- Modify: `src/lib/common/UrlConstants.h`
- Modify: `src/lib/common/Settings.cpp`
- Modify: `src/lib/common/Settings.h`
- Modify: `.github/workflows/continuous-integration.yml`
- Modify: `docs/HelpMain.md`
- Modify: `docs/dev/mac-stable-signing.md`
- Modify: `src/unittests/common/SettingsTests.cpp`
- Modify: relevant translation/update tests

- [ ] **Step 1: Add failing source-policy tests before changing constants**

Extend existing tests:

- `SettingsTests` confirms no default update URL is read by the updater and existing `gui/enableUpdateCheck`
  remains writable.
- `UpdateNetworkTests` asserts `kUrlUpdateCheck` is the fork endpoint and that
  `api.deskflow.org/version` is absent from updater production code.
- A release workflow source test, if CMake exposes file tests, otherwise the final static review command,
  asserts no `sums.txt` deployment entry remains.

Run the focused targets and expect the pinned-endpoint assertion to fail.

- [ ] **Step 2: Replace endpoint and retire arbitrary URL input**

`UrlConstants.h`:

```cpp
const auto kUrlUpdateCheck =
    QStringLiteral("https://api.github.com/repos/subeom/deskflow/releases/latest");
```

Remove `Settings::Gui::UpdateCheckUrl` from valid updater use. A legacy persisted key can remain inert in the
store but is not returned as an endpoint, displayed in a prompt, or used by `VersionChecker`.

Change startup consent text to:

```cpp
QObject::tr(
    "<p>Would you like to check for updates when %1 starts?</p>"
    "<p>Release checks use the stable releases of subeom/deskflow on GitHub.</p>"
).arg(kAppName)
```

- [ ] **Step 3: Align release artifacts**

Change `.github/workflows/continuous-integration.yml`:

```diff
-echo "Build: ${{env.DESKFLOW_VERSION }}" > sums.txt
-sha256sum deskflow-* >> sums.txt
+echo "Build: ${{env.DESKFLOW_VERSION }}" > SHA256SUMS.txt
+sha256sum deskflow-* >> SHA256SUMS.txt
```

Change every release file list entry from `sums.txt` to `SHA256SUMS.txt`. Keep `changelog.txt` unchanged.

- [ ] **Step 4: Update only directly related docs**

`docs/HelpMain.md` explains:

- direct macOS checks/downloads/verifies/stages and asks once before `Install and restart`;
- Homebrew/package-managed macOS, Windows, and Linux remain manual;
- no unsigned installer is executed.

`docs/dev/mac-stable-signing.md` adds that the updater's runtime requirement is the existing bundle ID plus
leaf hash, intentionally without `spctl`, notarization, Team ID, or keychain use.

- [ ] **Step 5: Run focused source/settings tests and static scans**

```bash
cmake --build "$BUILD" --config Release --parallel 8 --target \
  SettingsTests UpdateNetworkTests
env QT_QPA_PLATFORM=offscreen HOME="$SCRATCH_HOME" XDG_CACHE_HOME="$XDG_CACHE_HOME" \
  XDG_CONFIG_HOME="$XDG_CONFIG_HOME" TMPDIR="$TMPDIR" \
  ctest --test-dir "$BUILD/src/unittests" \
  -R '^(SettingsTests|UpdateNetworkTests)$' --output-on-failure
rg -n 'api\\.deskflow\\.org/version|sums\\.txt|spctl|Developer ID|5QCJCX8C37' \
  src/lib/gui src/apps/deskflow-gui .github/workflows/continuous-integration.yml
```

Expected: tests pass; the scan returns `sums.txt` only in this design/spec prose, not production updater/workflow
code. No `spctl` or upstream team appears under updater production paths.

- [ ] **Step 6: Commit endpoint/release alignment**

```bash
git add src/lib/common/UrlConstants.h src/lib/common/Settings.cpp src/lib/common/Settings.h \
  src/unittests/common/SettingsTests.cpp .github/workflows/continuous-integration.yml \
  docs/HelpMain.md docs/dev/mac-stable-signing.md \
  src/lib/gui/VersionChecker.cpp src/lib/gui/VersionChecker.h \
  src/unittests/gui/UpdateNetworkTests.cpp src/unittests/gui/UpdateNetworkTests.h \
  src/unittests/gui/CMakeLists.txt
git commit -m 'build(update): align release contract and docs' \
  -m 'Co-authored-by: Copilot App <223556219+Copilot@users.noreply.github.com>'
```

### Task 9: Full mirrored Release verification and final review

**Files:**
- Create outside repo:
  `/Users/subeomlee/.copilot/session-state/3cb871c9-7e25-477c-9173-ee473b1d38f4/files/auto-updater.sandbox.sb`
- Modify no repository source unless a named check fails

- [ ] **Step 1: Create scratch directories and a restrictive macOS sandbox profile outside the repository**

Use:

```text
SESSION=/Users/subeomlee/.copilot/session-state/3cb871c9-7e25-477c-9173-ee473b1d38f4/files
BUILD=$SESSION/auto-updater-build
SCRATCH=$SESSION/auto-updater-scratch
SCRATCH_HOME=$SCRATCH/home
XDG_CACHE_HOME=$SCRATCH/cache
XDG_CONFIG_HOME=$SCRATCH/config
TMPDIR=$SCRATCH/tmp
```

Create the five directories. The sandbox profile at the file above is exactly:

```lisp
(version 1)
(allow default)

(deny file-write*
  (subpath "/Applications")
  (subpath "/Library")
  (subpath "/System")
  (subpath "/etc")
  (subpath "/Users/subeomlee/Library/Deskflow")
  (subpath "/Users/subeomlee/Library/Keychains"))

(deny process-exec
  (literal "/usr/bin/osascript")
  (literal "/usr/bin/sudo")
  (literal "/sbin/ping")
  (literal "/sbin/ifconfig")
  (literal "/usr/sbin/ifconfig")
  (literal "/usr/local/bin/cloudflared")
  (literal "/opt/homebrew/bin/cloudflared"))

(deny network-outbound)
```

The sandbox profile therefore:

- denies execution of `/usr/bin/osascript`, real `sudo`, `ifconfig`, `ping`, `cloudflared`, and the live updater
  helper;
- denies network connections initiated by tests;
- denies writes below `/Applications`, `/Library`, `/System`, `/etc`, the real
  `/Users/subeomlee/Library/Deskflow`, and the real keychains;
- allows reads of repository/compiler/Qt files and writes only below the repository source, session build,
  scratch, and CMake/Ninja temporary locations needed by the build.

Run the configure/build/test commands through `sandbox-exec -f` with the profile.

- [ ] **Step 2: Configure Release with Qt 6.12.0 and warnings as errors**

Resolve a real Qt 6.12.0 CMake prefix from the installed Qt location, then assert:

```bash
QT_ROOT="$HOME/Qt/6.12.0/macos"
test -f "$QT_ROOT/lib/cmake/Qt6/Qt6Config.cmake"
cmake -S "$REPO" -B "$BUILD" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$QT_ROOT" \
  -DCMAKE_COMPILE_WARNING_AS_ERROR=ON \
  -DBUILD_TESTS=ON \
  -DSKIP_BUILD_TESTS=ON \
  -DBUILD_INSTALLER=OFF \
  -DBUILD_DEV_DOCS=OFF
cmake --build "$BUILD" --target help
```

Record `Qt6_DIR`, CMake generator, compiler, and configured version from `CMakeCache.txt`; configuration is
successful only if Qt reports 6.12.x and `CMAKE_COMPILE_WARNING_AS_ERROR:BOOL=ON`.

- [ ] **Step 3: Build named production and test targets only**

First build production dependencies:

```bash
cmake --build "$BUILD" --config Release --parallel 8 --target \
  gui Deskflow
```

Then enumerate concrete test executables produced by CMake and build each named target. The command must not use
`all`, `run_tests`, or an empty aggregate target. At minimum include:

```text
UpdateReleaseTests
UpdateNetworkTests
UpdateIntegrityTests
UpdateCacheTests
UpdateStatusModelTests
VersionCheckerTests
UpdateVerifierTests
MacUpdateIntegrationTests
UpdateInstallerTests
UpdateShutdownTests
StatusBarTests
MainShellTests
TrayMenuModelTests
SettingsPagesTests
TranslationFileTests
SettingsTests
```

For the final `ctest -E` run, build every registered named CTest executable target with one explicit
space-separated `--target` list. Exclude only the CMake-only `GitVersionTests` entry, which has no executable
target.

- [ ] **Step 4: Run focused updater tests, then the required ctest set**

Focused:

```bash
env QT_QPA_PLATFORM=offscreen HOME="$SCRATCH_HOME" XDG_CACHE_HOME="$XDG_CACHE_HOME" \
  XDG_CONFIG_HOME="$XDG_CONFIG_HOME" TMPDIR="$TMPDIR" \
  ctest --test-dir "$BUILD/src/unittests" \
  -R '^(Update|VersionChecker|MacUpdateIntegration|StatusBar|MainShell|TrayMenuModel|SettingsPages|TranslationFile)' \
  --output-on-failure
```

Full required set, under the sandbox:

```bash
env QT_QPA_PLATFORM=offscreen HOME="$SCRATCH_HOME" XDG_CACHE_HOME="$XDG_CACHE_HOME" \
  XDG_CONFIG_HOME="$XDG_CONFIG_HOME" TMPDIR="$TMPDIR" \
  ctest --test-dir "$BUILD/src/unittests" \
  -E "OSXClipboardTests|OSXKeyStateTests" --output-on-failure
```

Record:

- configured test count from `ctest -N`;
- tests run/pass/skip/fail from focused and full runs;
- named target count built;
- any excluded tests and why.

Expected: all built tests pass, exactly the two requested macOS tests are excluded, and no updater test performs
network or real process access.

- [ ] **Step 5: Recompute live config hashes and verify no drift**

Re-run the exact read-only hash loop used before implementation for:

```text
~/Library/Deskflow/Deskflow.conf
~/Library/Deskflow/deskflow-server.conf
~/Library/Deskflow/tls/deskflow.pem
~/Library/Deskflow/tls/trusted-clients
~/Library/Preferences/org.deskflow.deskflow.plist
```

Expected hashes are the baseline recorded in the session checkpoint. `~/Library/Application Support/Deskflow`
must remain absent. Any difference is a stop/blocker requiring evidence; do not overwrite.

- [ ] **Step 6: Run final diff, scope, and security review**

```bash
git status --short --branch
git diff --check fork/master..HEAD
git --no-pager log --oneline --decorate fork/master..HEAD
git --no-pager diff --stat fork/master..HEAD
git --no-pager diff --name-status cf77d2cf3bcba090b7fc20a4593fe881225fb2e7..HEAD
rg -n 'TBD|TODO|FIXME|api\\.deskflow\\.org/version|spctl|--apply-update|sums\\.txt' \
  src/lib/gui src/apps/deskflow-gui .github/workflows/continuous-integration.yml
rg -n 'QProcess::start\\(|system\\(|popen\\(|/bin/sh|cmd\\.exe|powershell' \
  src/lib/gui/Update* src/lib/gui/MacUpdate* src/apps/deskflow-gui
```

Confirm:

- no source commit modifies the spec content;
- no translation file changed except Korean, if any;
- all updater URLs originate in `UrlConstants.h` and policy code;
- no production path calls `spctl`, `security`, shell commands, Windows package execution, or package-manager
  commands;
- no live helper argument can be executed during tests;
- no untracked build products remain in the repository.

- [ ] **Step 7: Commit only any final defect fix and report**

If Task 9 required a source correction, commit it as:

```bash
git diff --name-only
git add src/lib/gui/UpdateNetwork.cpp
git diff --cached --name-only
git commit -m 'test(update): close integration failures' \
  -m 'Co-authored-by: Copilot App <223556219+Copilot@users.noreply.github.com>'
```

The `git add src/lib/gui/UpdateNetwork.cpp` line is an example for a failure in that file. Replace it with one
`git add` line per exact path printed by `git diff --name-only`; stage no other file. If no correction was
needed, do not create an empty commit. No aggregate or `git add .` command is allowed.

The final handoff must list:

- branch;
- every commit from `cf77d2cf3..HEAD`;
- the plan path;
- implementation summary;
- named production/test target count;
- focused test count and exact full CTest count;
- excluded `OSXClipboardTests`/`OSXKeyStateTests`;
- manual-only TCC, Gatekeeper/quarantine, LaunchServices, real installed-app, daemon/service, and port checks;
- each spec deviation or `none`.

## Plan self-review checklist

- [x] Release JSON, stable version, exact repository/asset, architecture, and checksum parser are in Tasks 1 and
  2.
- [x] URL, redirects, ETag, rate limit, cancellation, timeout, size, and privacy constraints are in Tasks 2 and
  4.
- [x] Cache/path/symlink/quota/stale cleanup and re-verification are in Task 3.
- [x] Shared state/UI, startup setting, `Check now`, StatusBar, MainShell, tray, and Korean strings are in Tasks 4
  and 7.
- [x] Pinned self-signed bundle ID/leaf, no Team/notarization/`spctl`, and isolated DMG integration are in Task 5.
- [x] One confirmation, helper receipt/standby, core/tunnel shutdown, atomic swap, readiness, rollback, and
  relaunch are in Tasks 6 and 7.
- [x] Windows/Linux/package-managed macOS remain manual in Tasks 1, 4, 7, and 8.
- [x] Release workflow emits `SHA256SUMS.txt` in Task 8.
- [x] Every behavior change has a failing test before implementation and a focused pass command.
- [x] Full external Release/Qt 6.12/CTest execution, exclusions, sandboxing, and live hash comparison are in Task 9.
