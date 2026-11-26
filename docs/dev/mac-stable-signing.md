# Stable code signing of the macOS test builds

The macOS test builds that are published from this fork (the `macos-1.27.0.N` releases) are signed with **one fixed, self-signed
certificate** instead of an ad hoc signature. This page says why, what the certificate is, how it was made and how it is used, what
it does not do, and what to do once when you move from an ad hoc build to a stable one.

## Why

macOS keeps the privacy permissions that Deskflow needs (**Accessibility** and **Input Monitoring**) in its privacy database (TCC). With
each permission it stores a **code requirement** of the app that was allowed, and it takes a later program for that app only if the program
satisfies the requirement.

- An **ad hoc** signature (`codesign --sign -`) has no identity. The designated requirement of such a build is its own hash,
  `cdhash H"..."`, and that hash is different for every build. macOS therefore sees every new build as another app: after each install the
  permissions have to be reset and given again.
- A build that is signed with a **certificate** has the designated requirement
  `identifier "org.deskflow.deskflow" and certificate leaf = H"<SHA-1 of the certificate>"`. It does not depend on the contents of the build,
  so **every build that is signed with the same certificate satisfies it**, and a permission that was given to one build holds for the next.

## The identity

| | |
|---|---|
| Subject and issuer | `CN=Deskflow Local Signing` (self-signed; no other field) |
| Key and signature | RSA 2048, SHA-256 |
| Valid | 2026-11-16 23:43:59 GMT to 2036-11-13 23:43:59 GMT |
| Usage | key usage `digitalSignature`, extended key usage `codeSigning` (both critical), `CA:FALSE` |
| SHA-1 | `39b1c21df0ba75cdde058fcbb09a1ad38aef8055` |
| SHA-256 | `38f7620260214e9d1590949b45012366b003a61d8b1928d58da80876675f89e6` |

The designated requirement of the app, which is what macOS stores with a permission, is

```
identifier "org.deskflow.deskflow" and certificate leaf = H"39b1c21df0ba75cdde058fcbb09a1ad38aef8055"
```

`deskflow-core` has the identifier `deskflow-core` and every Qt framework and plug-in its own identifier, all with the same certificate.

The private key exists only on the machine that builds the releases, in a keychain file of its own. It is **not** in this repository, not in
the releases and not anywhere else, and it is never uploaded.

## What it does not do

- It is **not notarized** and it is not a Developer ID. Nothing in macOS trusts the certificate: `security find-identity -v -p codesigning` does not
  list it as valid (without `-v` it says `CSSMERR_TP_NOT_TRUSTED`), and `spctl --assess` says `rejected`. A copy that was downloaded (and so is
  quarantined) still gets the Gatekeeper warning for an unidentified developer when it is first opened: right-click the app and choose Open, or run
  `xattr -dr com.apple.quarantine /Applications/Deskflow.app`. The signature has no timestamp.
- It does not make a build safer. Whoever has the private key can sign an app that inherits the permissions that were given to Deskflow, which is
  why the key stays on the build machine and is never exported.
- It changes no code and no setting: the program is the same, only its signature is another.
- It does not use the hardened runtime and has no entitlements. A hardened runtime process only loads libraries that are signed by Apple or have its own Team ID, a
  self-signed identity has none, and the bundled Qt frameworks would be refused.

## How the identity is made (once, on the build machine)

The key and the PKCS#12 file are only in a private temporary directory while they are imported. The password of the keychain is a random value in a file
that only its owner can read.

```sh
umask 077
WORK="$(mktemp -d)"
PASSWORD_FILE=/path/of/a/file/with/mode/0600/that/holds/a/random/password
KEYCHAIN="$HOME/Library/Keychains/deskflow-signing.keychain-db"

openssl req -x509 -newkey rsa:2048 -noenc -sha256 -days 3650 \
  -keyout "$WORK/key.pem" -out "$WORK/cert.pem" -subj "/CN=Deskflow Local Signing" \
  -addext "basicConstraints=critical,CA:FALSE" \
  -addext "keyUsage=critical,digitalSignature" \
  -addext "extendedKeyUsage=critical,codeSigning" \
  -addext "subjectKeyIdentifier=hash"

# The LibreSSL that ships with macOS writes a PKCS#12 that `security import` reads; the files that OpenSSL 3 and
# later write by default may be refused by it.
/usr/bin/openssl pkcs12 -export -inkey "$WORK/key.pem" -in "$WORK/cert.pem" \
  -name "Deskflow Local Signing" -out "$WORK/id.p12" -passout "file:$PASSWORD_FILE"

# A keychain file of its own: not the login keychain and not the system keychain.
security create-keychain -p "$(cat "$PASSWORD_FILE")" "$KEYCHAIN"
security list-keychains -d user          # the new keychain must not be in the search list; if it is, put the list back as it was
security unlock-keychain -p "$(cat "$PASSWORD_FILE")" "$KEYCHAIN"
security set-keychain-settings "$KEYCHAIN"        # no timeout and no lock on sleep: it stays unlocked (see "Never let a command meet a locked keychain")
security import "$WORK/id.p12" -k "$KEYCHAIN" -P "$(cat "$PASSWORD_FILE")" -T /usr/bin/codesign
# so that codesign can use the key without a prompt that nobody can click
security set-key-partition-list -S apple-tool:,apple:,codesign: -s -k "$(cat "$PASSWORD_FILE")" "$KEYCHAIN"

rm -rf "$WORK"                    # from now on the key is only in the keychain
security find-identity -p codesigning "$KEYCHAIN"        # shows the SHA-1 of the certificate
```

Nothing here changes a trust setting of the user or of the system: no `security add-trusted-cert`, and `codesign` signs with the untrusted
identity when it is named by its SHA-1 and the keychain is given with `--keychain` (the keychain is not in the search list).

## How a build is signed

The release build runs CMake's `package` target. Between the install into the staging directory and the creation of the disk image, a CPack
pre-build step signs the staged app, and stops the build when anything of the following is not as expected. That step belongs to the build tooling of
whoever publishes the releases and is not in this repository (the install rules of the repository sign ad hoc, with the hardened runtime, which the
bundled Qt frameworks do not survive without a Team ID):

```sh
# first, every time: unlock; when this fails, stop and do not run codesign (it would open a password window)
security unlock-keychain -p "$(cat "$PASSWORD_FILE")" "$KEYCHAIN"
codesign --force --deep --sign 39b1c21df0ba75cdde058fcbb09a1ad38aef8055 --keychain "$KEYCHAIN" --timestamp=none Deskflow.app

codesign --verify --deep --strict Deskflow.app
codesign -d -r- Deskflow.app        # designated => identifier "org.deskflow.deskflow" and certificate leaf = H"39b1..."
codesign -dvvv Deskflow.app         # flags=0x0(none): no hardened runtime; Authority=Deskflow Local Signing; no TeamIdentifier
```

`--deep` signs every nested Mach-O file (the Qt frameworks, the plug-ins and `deskflow-core`) first and the app last. Every one of those code objects has to
have the requirement `identifier "<its identifier>" and certificate leaf = H"39b1..."` and no `cdhash`.

## Never let a command meet a locked keychain

The password of the keychain is a random value in a file; nobody knows it by heart. A `security` or `codesign` call that finds the keychain **locked**
makes macOS open an "enter the keychain password" window on the screen of whoever is logged in, and that includes harmless calls such as
`security show-keychain-info`. Nobody can answer that window. Killing the call that opened it does not close the window, and in one case it was followed by
the system service refusing the *right* password for that keychain **file path** (the same file unlocked without a problem under another file name,
and renaming the file was the way out); the cause was not established.

So:

- Unlock first, every time, right before the keychain is used, with the password from the file, and go on only when the unlock succeeded. When it
  failed, do not call `security` or `codesign` on that keychain at all.
- Never lock it again by hand, and give it no timeout (`security set-keychain-settings "$KEYCHAIN"` without `-t` and without `-l`). It then stays
  unlocked until the user logs out or the Mac restarts, and the next build begins with the unlock again.
- The unlock can be done through the Security framework with `SecKeychainSetUserInteractionAllowed(false)` (a few lines of Python with `ctypes` are
  enough): a failed unlock then returns an error and cannot open a window. `SecKeychainGetStatus` tells whether the keychain is locked, which no `security`
  command tells without the risk of that window.
- Always name the keychain: a `security` command without a keychain argument acts on the login keychain of the user.

## Check a build

```sh
codesign -d -r- /Applications/Deskflow.app
codesign --verify --strict \
  --test-requirement='=identifier "org.deskflow.deskflow" and certificate leaf = H"39b1c21df0ba75cdde058fcbb09a1ad38aef8055"' \
  /Applications/Deskflow.app && echo "signed with the stable identity"
```

The second command is what macOS does with a stored requirement. Two different builds that are signed with the identity have different cdhashes
(`codesign -dvvv`), the same designated requirement, and each satisfies the requirement of the other. A build that is signed ad hoc never does.

## Updater verification

The updater uses the same exact requirement at runtime. It verifies the mounted `Deskflow.app`, the GUI
executable, and `deskflow-core` before accepting a staged update. The certificate is self-signed, so the updater
does not require or invoke Apple Developer ID, Team Identifier, notarization, `spctl`, or the real signing
keychain. GitHub asset sizes and `SHA256SUMS.txt` verify delivery integrity; the pinned bundle identifier and
certificate leaf bind the app to this fork.

This policy applies only to manually published fork releases. Standard repository CI keeps its upstream
`sums.txt` and notarized Developer ID path. Those artifacts are intentionally manual-only and are not accepted
by the fork updater's `SHA256SUMS.txt` plus pinned-leaf policy.

## The one-time re-grant

The first build that is signed with the identity replaces a build that is signed ad hoc. What macOS stored for that one is a cdhash, which the new build does not
satisfy, so the permissions have to be given **once more on every Mac**:

1. Quit Deskflow completely (also the `deskflow-core` process).
2. Reset what is stored for the app: `tccutil reset Accessibility org.deskflow.deskflow` and `tccutil reset ListenEvent org.deskflow.deskflow`
   (`ListenEvent` is Input Monitoring).
3. In System Settings > Privacy & Security, under Accessibility and under Input Monitoring, remove every Deskflow and `deskflow-core` entry that is still
   listed (select it and press the minus button). An entry of an earlier ad hoc build can look switched on and still not work.
4. Install the build, start Deskflow, and allow Accessibility and Input Monitoring when macOS asks (or add the app with the plus button).
5. Quit Deskflow and start it again; the permissions apply to a program that is started after they were given.

From then on an update that is signed with the same certificate should keep the permissions. `codesign -d -r- /Applications/Deskflow.app` before and after the
update shows the same requirement.

That the permissions survive an update follows from how macOS evaluates a stored requirement, and the requirement test above shows that a later build satisfies it. It had
**not been observed across two real installs** when this page was written; if the permissions are asked for again after an update, the requirement above is the first thing to compare.

## If the key is lost

A certificate cannot be made again with the same hash. A new certificate has another SHA-1, so another requirement, and the permissions have to be given once more on every Mac, as above.
That happens when the keychain file is deleted or damaged, when the build machine is replaced without the keychain file, and when the certificate expires (2036-11-13). The keychain file holds the private
key: if it is copied, keep the copy private, and never commit or upload it. A build that someone else makes from this source has another identity (or none) and does not share the permissions of the published builds.
