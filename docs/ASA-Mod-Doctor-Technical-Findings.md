# ASA Mod Doctor: client mod-library validation findings

Prepared for a Studio Wildcard bug report, 18 November 2026. This is an independent technical investigation, not a description of proprietary ARK source code.

**Finding:** in the tested Windows client scenarios, the persisted mod library, files on disk, in-game presentation, and current distribution metadata can disagree. Treating any one of them as the complete installed state can conceal damaged files, strand interrupted installations, or misidentify a current build as outdated. ASA Mod Doctor reads these layers separately and only offers narrowly verified repairs.

**Important scope correction:** the ASR incident below involved stale persisted metadata and incorrect labels in earlier versions of this tool. The tester explicitly confirmed that ARK's own update display was correct throughout. It is evidence of inconsistent persisted records, not evidence that ARK showed an incorrect update warning or installed the wrong ASR version.

Related: [tool download](https://github.com/bushome/arkascendedpcsinglemodrepairguide/releases/tag/Tool), [manual repair guide](https://github.com/bushome/arkascendedpcsinglemodrepairguide/wiki/Ark-Ascended-Surgical-Guide-to-fixing-singular-mods-on-PC), [C++ startup-validation proposal](Startup-Validation-Proposal.md).

## Scope and evidence

Controlled client tests were performed on a Windows Steam installation on 11–12 November, followed by local diagnostics, API observations and independent tester feedback on 18 November. Exact ARK executable and bundled SDK build identifiers were not captured; these results must not be generalized to every release, console, platform, or initialization path. The standalone tool supports additional source types, but that does not establish equivalent game behavior on them.

| Evidence | What was established | Strength and limit |
|---|---|---|
| E1: removed file/folder and restored-file tests | Deliberately missing content could remain `Normal`; a missing folder became `Invalid`; restoring its files did not clear `Invalid` on the tested restart. | Controlled game launches and disk observations; exact internal validation calls unknown. |
| E2: interrupted first installation | A `Pending` entry recorded resumable ranges although its temporary archive was absent and no installed folder existed. | Captured library and filesystem state; does not identify when or why the partial archive disappeared. |
| E3: dynamic cosmetics and suppression | Dynamic cosmetics occupied disk without appearing as ordinary installed mods; main-menu removal of suppressed cosmetics also removed an owned, normally installed cosmetic. | Controlled sessions and before/after library/folder observations. Ownership was independently known, not inferred from JSON. |
| E4: ASR public-build reconciliation | Installed contents matched public Windows file `9642577`; persisted publication/latest-known/status fields disagreed. A backed-up correction persisted after a real ARK launch/shutdown. | Supplied library, independent live API observations, tester live-file verification and repair/restart screenshots. No fresh post-restart fingerprint is shown. |
| E5: genuine Update All | Tester confirmed all seven other yellow entries genuinely needed game updates. After Update All, two were `Pending`, and the tester reported them absent from the in-game installed list. | Field report and screenshots; new interrupted-entry JSON and temporary archives were not captured. |
| E6: unpublished skin reversion | A developer recalls unpublished skins reverting to the public version and later appearing to uninstall. | Historical report with incomplete details; cause and actual before/after file IDs unconfirmed. |
| E7: server API outage | A long-term server operator reports startup failure when CurseForge is unreachable. | Operator report; no controlled outage reproduction, startup logs or internal call trace supplied for this report. |

Raw user libraries, account information, sign-in material and private machine paths are not published here. The ASR excerpt below contains only the fields necessary to understand that case. Synthetic regression counts are reported separately from game tests.

## On-disk model inspected

The tested client layout is:

```text
<GameRoot>/ShooterGame/Binaries/Win64/ShooterGame/
  ModsUserData/83374/library.json
  ModsUserData/83374/local/user_info.json
  Mods/83374/<projectId>_<fileId>/
  Mods/83374/.temp/<archive>.zip.tmp
```

`pathOnDisk`, for example `83374/931874_9642577`, is relative to `Mods`, not to `Mods/83374`. Applying the game-ID directory twice was an earlier tool bug, now corrected. A record's local entry ID, CurseForge project ID and distribution file ID have different meanings. Comparing or sorting numeric file IDs does not establish intended branch or update order.

`library.json` contains status, installed-file metadata, latest-known file metadata, cached project details, paths, download bookkeeping and dynamic-content state. The paired per-user record contains identity/preferences and sensitive sign-in data; it is not needed for diagnostic scans or the new metadata-only reconciliation. Existing full-removal operations maintain the paired entries with the game closed.

## What the tool validates

| Check | Reference and decision | What a passing result does not establish |
|---|---|---|
| Parse and identity | Valid installed-mod array; project/file identity; nonzero installed file where required; known versus unresolved installation state. | Current release, ownership, or engine compatibility. |
| Path containment | Resolve against the configured root; reject traversal and unsafe linked/reparse paths; compare recorded folder/file identity. | A safe root alone does not establish complete contents. |
| Physical presence | Check recorded mod directory and every referenced module; distinguish missing from inaccessible/uninspected. | Existing old files do not prove that an update completed. |
| Unpacked size | Sum actual files and compare with installed metadata; expose missing or extra content. | Equal size cannot detect all corruption or a same-size different build. |
| Installed-record fingerprints | Compare module fingerprints with `installedFile.modules`. Recheck library and file observations for changes during the scan. | Public freshness, cryptographic authenticity, licensing, or a fully adversarial consistency guarantee. |
| Cached consistency | Compare same-file installed/project records and latest/main relationships. An `OutOfDate` flag with matching cached IDs is a candidate for investigation. | Mutually agreeing cached records can all be stale. |
| Explicit live public target | Authenticated metadata lookup for the exact project and client/server platform; verify the selected main-file record with an exact-file lookup. | Access to unpublished builds or a tester's entitlements. |
| Live-reference integrity | Match installed identity to that target, then verify complete module coverage, total size and fingerprints against fresh reference data. Recheck the API around hashing. | Future target stability, automatic repair authorization or engine ABI compatibility. |
| Download and dynamic state | Report `Pending`, recorded ranges, dynamic flags and visible disk consumption separately from completeness. | Recorded `canResume` or byte ranges alone do not prove usable partial data. |
| History | Compare library records and folder inventories across scans: additions, removals, status/file-ID changes and missing folders. | It cannot reconstruct an event that predates the saved snapshots. |

Ordinary **Check files** uses the cached installed-file reference. **Check all files** in the completed client candidate reloads the library first. Neither ordinary action calls CurseForge. Live public checking is a separate explicit action requiring a user-provided Core API key; optional local storage uses Windows Credential Manager. No application key or game sign-in token is distributed with the tool.

The platform-aware API observations were important: ASR's Windows target was `9642577`, while Windows Server selected `9642580`. Those files had different sizes and module fingerprints. The WindowsGdk query returned the Windows client archive in the tested response. Treat platform selection as a request/result validation problem rather than inferring it from the largest file ID. See [platform documentation](https://docs.curseforge.com/docs/game-integration/unreal/game-info-and-platforms/) and the [REST schemas](https://docs.curseforge.com/rest-api/).

## Fingerprint calculation

The module recipe was reconstructed and checked against 55 installed mods and deliberately changed content in the original tests:

1. For each file, omit bytes `09`, `0A`, `0D`, and `20` hexadecimal: tab, LF, CR and space.
2. Apply 32-bit MurmurHash2 with seed `1` to those remaining bytes.
3. Sort a module's unsigned per-file hashes numerically.
4. Concatenate their invariant decimal representations without separators.
5. Apply the same seed-1 hash to that ASCII string and compare with the module fingerprint.

This fingerprint does **not** certify byte identity: it omits specified whitespace and is noncryptographic. A matching cached fingerprint cannot establish which release is public, and a reference must cover every installed file before a metadata repair can claim a complete match. Original-byte backups and concurrency checks use separate byte comparisons/hashes.

## Reproducible client findings

### Damaged files can remain recorded as Normal

In E1, a `.ucas` file was moved out of an installed creature mod. Its directory dropped from approximately 316 MB to 2.6 MB. After the tested game launch, the library still recorded `Normal`, and the mod browser still presented the mod as installed. This establishes an absent/incomplete startup integrity check for this tested path, not that every ARK validation path omits content checks.

Reproduction should use an isolated installation and disposable save: record a clean module fingerprint, move a required data file out, launch/close, then compare stored status, directory inventory and browser presentation. The expected correction is to identify incomplete contents before loading or presenting a healthy installation. Permission errors and temporary file locks should remain distinct from confirmed content mismatches.

### Restored files can retain a stale Invalid status

Removing a recorded mod folder caused `Invalid` in E1. Restoring the intact folder and relaunching did not clear that state in the tested flow. Existing tool recovery verifies contents before proposing `Normal` or an update-needed state. A separate original field test confirmed that the reset persisted after game startup without replacing the mod's contents.

Suggested client behavior: validate the recorded installed revision and intended target before deciding whether a recovered installation is usable. Do not require a replacement download solely because a previously missing but now verified folder retains a stale flag.

### Interrupted state can outlive usable download data

E2 recorded `Pending`, `canResume: true`, approximately 100 MB of downloaded ranges, no installed folder and an empty `.temp` directory. E5 involved updates over existing installations: two entries became `Pending` and were reported missing from the installed menu. These are different forms of interrupted state and must not be reduced to one assumption that a full previous folder means the new version is complete.

The initial tests separately observed removal of an **empty** `.temp` directory on startup. They do not prove that every partial archive is always deleted on every launch. A robust recovery decision must reconcile the actual archive, validated ranges, extraction/staging state and queue state before offering resume or discarding records. Pending metadata must not be force-reset to `Normal` based only on old installed files.

### Same-file publication and latest-known records can disagree

Sanitized E4 example:

```json
{
  "projectId": 931874,
  "status": "OutOfDate",
  "installedFile": { "iD": 9642577, "fileStatus": "ReadyForReview" },
  "latestUpdatedFile": { "iD": 9555651, "fileStatus": "Approved" },
  "cachedProjectMainFileId": 9642577,
  "cachedSameFilePublication": "Approved"
}
```

The two same-ID records agreed on the relevant content metadata; publication state was the differing field. The installed archive was `arkitect-structures-remastered-windows-168.zip`, expected unpacked size `1,765,616,367` bytes, module `ArkitectStructures`, fingerprint `4178414037`. Live Windows metadata and the tester's actual installed files agreed on this reference. The original three-field correction was:

| Field | Before | After |
|---|---|---|
| Installed publication | ReadyForReview | Approved |
| Latest-known file | 9555651 | 9642577, with its associated known file metadata |
| Stored game status | OutOfDate | Normal |

The tester applied that correction with ARK closed. After launching and closing ARK, the tool still showed `Valid`, `Normal`, `Approved`, installed file `9642577`, and the same recorded identity/path/install date/size. Seven other notices were left intact and were independently confirmed as genuine game updates. This supports narrowly scoped reconciliation rather than blanket flag clearing. It does not demonstrate an in-game ASR update-display bug, because the tester said the game had been correct all along.

### Cosmetic visibility is not proof of uninstallation

E3 found installed dynamic cosmetic files absent from the ordinary installed-mod list. A later session completed several pending dynamic downloads. Main-menu **Uninstall suppressed cosmetics** removed every suppressed cosmetic in that test, including an owned, explicitly installed one; suppressing during an active session did not stop already queued downloads. These observations explain why browser visibility, suppression and physical installation need separate reporting. The local library did not establish ownership.

E6's unpublished-to-public reversion remains unconfirmed. Sharing a project ID between revisions is normal. Capture installed file IDs, dynamic state, folder existence and the triggering menu/session/server action before diagnosing branch replacement or deletion. The tool's intentional-test preference changes diagnostics; it does not pin the game's revision. Wildcard documents unpublished access through My Mods and server `-dev` selection in its [testing guide](https://devkit.studiowildcard.com/getting-started/testing). Dynamic-content visibility is also described in the [installation-method overview](https://docs.curseforge.com/docs/capabilities/mod-installation-methods/overview/).

### Unreferenced-folder cleanup can amplify lost metadata

An original test placed a folder for a project absent from the library; that folder was deleted during the tested startup. Cleanup of old-version folders belonging to projects still installed was not established. The tool therefore reports unreferenced folders for investigation and does not promise they will all disappear on next launch. Losing a library entry can make a previously valid folder appear unclaimed; preserve evidence and reconcile records before destructive cleanup.

## Repair boundaries

The local metadata-only operation requires an unambiguous complete installed record, explicit public intent, matching platform/file identity, fresh API reference and complete actual-file verification. It refuses Pending/Invalid/unmanaged/intentional-test entries, ambiguous content records and genuine different-target installs. It locks existing content against writes/deletion during repair, checks library bytes and the public target again, verifies a durable original-byte backup, stages beside the destination, replaces the library and reloads the saved entry. Guarded recovery retains evidence and does not intentionally overwrite a later external change. Process checks and filesystem observations reduce races; they are not a universal guarantee against arbitrary concurrent writers.

Only the reviewed fields are changed. Broader variants can correct a status-only stale flag or agreeing-but-stale publication fields after live verification; matching same-ID cached publication fields are included in the preview. Unrelated project-cache fields, preferences and unknown JSON fields are preserved. `user_info.json` and mod contents are untouched by that operation. Broader variants have synthetic regression coverage; the original E4 correction has the independent game-restart trial.

A genuine older installed revision requires actual updating. Damaged contents require content recovery. The tool does not download replacement CurseForge archives, determine the newest inaccessible development build, validate ownership from JSON, or infer Unreal compatibility from `cookerVersion`. Local reset/removal are separate operations. Network/server/FTP/FTPS/SFTP sources remain diagnostic-only in this candidate.

## Suggested startup improvements for Wildcard review

1. Capture complete persisted state before cleanup, including pending entries not exposed by the ordinary UI, and keep a generation/version for concurrent operations.
2. Separate local integrity, installation progress, intended revision, remote freshness, access authorization and UI visibility. An API failure is not a corrupt mod.
3. Validate identities, contained paths, module inventory and actual contents before loading. Perform expensive hashing away from the game thread using a stable snapshot; recheck state before publishing the result.
4. Reconcile a recovered Invalid installation or stale publication/update fields only against the appropriate verified reference and explicit branch intent. Treat public versus developer targeting as state, not numeric ID ordering.
5. Resume only from verified existing partial data. Present restart/cleanup options for stranded Pending entries even when normal installed-list filters exclude them.
6. Keep the prior installation and recovery journal until the replacement revision and metadata are committed. Quarantine unclaimed content before destructive cleanup when state recovery is unresolved.

These are integration recommendations derived from the observations, not claims that named functions in the proprietary executable are defective. The public SDK exposes validation and synchronization capabilities; their presence does not establish which calls or configuration ARK used in these tests. [SDK library reference](https://docs.curseforge.com/docs/game-integration/unreal/installing-mods/)

## Dedicated-server startup when CurseForge is unreachable

E7 is a separate operator report. The documented server flow includes `AssureServerModsUpdated` and platform-specific resolution, making a network dependency plausible; its existence does not prove the source of ARK's observed startup failure. [Dedicated-server integration](https://docs.curseforge.com/docs/game-integration/unreal/multiplayer-and-servers/)

An engineering investigation should capture the exact ARK build, mod list, failure log/codes, DNS/TLS/HTTP outcome, and whether installed content is complete. Test an isolated server with no required downloads and a trusted last-known-good deployment manifest, then repeat with one required mod missing. Avoid outage experiments on the production cluster.

A proposed resilience policy could permit an **explicitly enabled** last-known-good startup only when every required project, exact file, platform and transitive dependency is pinned and locally verified, with all applicable authorization checks still satisfied. Otherwise retain a clear recoverable startup block. Do not label cached data as current public metadata, turn an unavailable API into `Invalid`, switch test branches, discard files, or bypass ownership/session requirements. Bounded retry and deferred remote freshness checks should be distinct from local corruption handling. This policy is a proposal, not implemented ARK behavior or a network bypass in the tool.

## Validation of the companion candidate

The completed client candidate passes **131 regression scenarios**, including actual isolated Windows credential persistence, plus **72 controlled adapter checks** (FTP 22, FTPS 24, SFTP 26). Synthetic fixtures exercise changing libraries/targets, cancellation, read-only sources, held content locks, rollback, unknown-field preservation, genuine update retention, batch reload and broader metadata-review variants. WPF layouts are rendered and inspected. These checks support the tool's implementation; they are not independent ARK game reproductions.

The separate C++ policy example compiles under MSVC C++17 with warnings treated as errors and passes 28 standalone policy checks. Its engine adapter is illustrative and has not been compiled or run inside ARK.

The Windows x64 package is self-contained with the .NET 10 Desktop Runtime. This document can describe a newer candidate than the binary currently attached to the public `Tool` release; consult the supplied package/build record when reproducing. Application source remains private. No raw sign-in material or private user libraries are required for the public bug report.

The separate [startup proposal](Startup-Validation-Proposal.md) supplies an original, engine-independent C++ policy example and an integration sketch. It has no knowledge of ARK executable addresses, private headers or internal request origins.

