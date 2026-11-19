# ARK: Survival Ascended — client mod-library integrity and recovery findings

Technical report for Studio Wildcard · 18 November 2026

## Summary

Testing of the Windows client identified gaps between the installation state recorded in `library.json`, the mod browser's presentation, and the content present on disk. The principal findings are:

- Removing required mod content did not necessarily change a recorded `Normal` installation or its installed indicator.
- Restoring complete files did not clear an `Invalid` installation in the tested restart flow.
- An interrupted installation retained resumable-download metadata although its partial archive was absent.
- Different records for the same installed distribution file retained conflicting publication and update information after that file was publicly available.

These findings support reviewing startup integrity validation and recovery of persisted installation state. They do not establish the location or cause of a defect in ARK's internal code.

## Test environment

Controlled tests used a Windows Steam client installation on 11–12 November 2026. Additional library inspection, live CurseForge metadata checks and an independent user's repair/restart test took place on 18 November. Exact game executable and bundled SDK build numbers were not recorded. Platform-specific behavior outside these Windows tests remains unverified.

The relevant layout was:

```text
<GameRoot>/ShooterGame/Binaries/Win64/ShooterGame/
  ModsUserData/83374/library.json
  ModsUserData/83374/local/user_info.json
  Mods/83374/<projectId>_<fileId>/
  Mods/83374/.temp/<archive>.zip.tmp
```

`installedMods[].pathOnDisk` is relative to `Mods`; its `83374/` segment must not be appended to a root that already includes that segment. Local installation IDs, CurseForge project IDs and distribution file IDs are distinct identifiers.

## 1. Missing content can remain recorded as a healthy installation

**Observed:** a required `.ucas` file was moved out of an installed mod. Its directory size fell from approximately 316 MB to 2.6 MB. After launching and closing ARK, the entry still had `status: Normal`, and the mod browser still showed it as installed. This test did not establish whether attempting to load the affected content would succeed or fail.

**Reproduction sequence:**

1. In an isolated installation, install a mod and record its file inventory, unpacked size and module fingerprints.
2. Close ARK and move a required data file outside the mod directory.
3. Launch ARK and inspect the mod browser; then close it and inspect the persisted entry.
4. Compare the resulting state with the missing file, actual size and fingerprint mismatch.

**Expected:** the client should distinguish an incomplete installation from a verified installation before it is presented as usable. A persisted `Normal` flag and an existing folder are insufficient evidence of complete contents.

**Engineering implication:** review which startup and pre-load paths perform size/module validation, rather than relying exclusively on directory existence. The observed result applies to this tested path; it does not establish that every ARK validation path omits content checks.

## 2. Restored content can remain blocked by a stale Invalid state

**Observed:** removing a recorded mod folder caused the game to mark the entry `Invalid`. Returning the original intact folder and relaunching did not clear that state. Independent file verification subsequently matched the installed reference. After a verified external status reset, ARK retained the corrected state on its next launch without replacing the mod's contents.

**Reproduction sequence:**

1. Record a complete installed mod and close ARK.
2. Move its folder out, launch/close ARK, and confirm the transition to `Invalid`.
3. Restore the unchanged folder and verify its contents against the original reference.
4. Launch/close ARK again and inspect the entry's status.

**Expected:** when the original revision is present and verifies successfully, the client should offer or perform a controlled recovery of the installation state. A full replacement download should not be required solely because a previous missing-folder condition left a stale flag.

The intended revision still matters: recovering valid old contents and determining whether a newer public or server-required revision exists are separate decisions.

## 3. Interrupted installation state can outlive usable resume data

**Observed first-time installation:** ARK was closed during a roughly 7.25 GB map download. The resulting entry recorded:

| Field or observation | Value |
|---|---|
| `status` | `Pending` |
| `pathOnDisk` | Empty |
| Installed-file identity | No complete installed file |
| `downloadInfo.canResume` | `true` |
| Recorded downloaded ranges | Approximately 100 MB |
| Recorded temporary archive | Absent; `.temp` was empty |
| Installed mod folder | Absent |

The game still presented that first-time installation as installed. In a separate, later interrupted **update** case, two existing mods remained `Pending`, and the user reported that they no longer appeared in the in-game installed list.

**Reproduction sequence:** start an installation or update, close ARK before completion, then compare the persisted download record, actual partial archive, installed files and browser presentation on the next session. The update case should be tested separately from a first-time installation because a complete previous revision may still exist.

**Expected:** resume eligibility should depend on usable partial data for the correct target file, not merely on a recorded boolean or byte ranges. If that data is missing, the client should expose a restart or cleanup action even when the record is excluded from the ordinary installed list. Matching the previous revision's files must not complete a pending update by changing its status to `Normal`.

The observations do not establish when the partial archive disappeared. An early cleanup test removed an **empty** `.temp` directory; it does not prove that every partial download is deleted on every launch.

## 4. Publication and update records can disagree for the same installed file

Inspection of **Arkitect Structures Remastered**, project `931874`, found the following fields within one installed-mod entry:

| Record | Recorded value |
|---|---|
| `installedFile.iD` | `9642577` |
| `installedFile.fileStatus` | `ReadyForReview` |
| `latestUpdatedFile.iD` | `9555651` |
| `details.mainFileId` | `9642577` |
| Matching file in `details.latestFiles` | File `9642577`, publication `Approved` |
| Entry `status` | `OutOfDate` |

The installed-file and matching cached project-file records agreed on the relevant content metadata; their publication state differed. A live Windows-platform query independently identified `9642577` as the public target, and the user's installed files matched that reference:

```text
Archive:        arkitect-structures-remastered-windows-168.zip
Unpacked size:  1,765,616,367 bytes
Module:         ArkitectStructures
Fingerprint:    4178414037
```

**Observed recovery:** with ARK closed, the installed publication status was corrected to `Approved`, the complete latest-known file record was updated to `9642577`, and the obsolete `OutOfDate` status was changed to `Normal`. After launching and closing ARK, those corrections remained. The recorded installation identity, folder, install date and size were unchanged. A fresh post-restart fingerprint check was not captured.

**Expected:** records describing the same distribution file should converge on its current publication state, and update status should agree with the intended verified revision. Publication state should not be treated as an immutable property captured only at installation time.

**Scope:** ARK's own mod browser already displayed this mod correctly. The inconsistent persisted fields caused misleading labels in earlier versions of the external diagnostic tool. This case demonstrates a record-consistency issue, not a failed game update or an incorrect in-game ASR update warning.

## Diagnostic validation used

ASA Mod Doctor separates recorded installation state, physical integrity and current release metadata:

| Validation | Comparison and interpretation |
|---|---|
| Identity and paths | Check project/file relationships, recorded folder identity, canonical containment and unsafe linked paths. |
| Presence and size | Check the mod directory and referenced modules; compare actual unpacked bytes with `fileSizeOnDisk`. Distinguish missing from inaccessible data. |
| Installed-reference integrity | Compare actual module fingerprints with `installedFile.modules`. A match establishes agreement with that reference, not current public-release status. |
| Cached consistency | Compare same-file installed/project records and installed/latest-known/main-file relationships. Agreement between cached records does not establish freshness. |
| Live-reference integrity | Resolve the exact platform's public target; confirm its exact-file metadata; compare complete actual contents with its size and module fingerprints; recheck the target around verification. |
| Change detection | Compare library contents and file inventories before/after a scan. Discard results when an update changes the reference or content during inspection. |
| Interrupted-state inspection | Compare recorded status and progress with actual installed/temporary data. Temporary-archive absence in the interruption test was established by filesystem inspection. |

The fingerprint calculation was reconstructed and verified against 55 installed mods and deliberately altered content:

1. Exclude tab, LF, CR and space bytes (`09`, `0A`, `0D`, `20` hexadecimal) from each file's input.
2. Calculate 32-bit MurmurHash2 with seed `1` for each file.
3. Sort the unsigned file hashes numerically and concatenate their invariant decimal strings without separators.
4. Hash that ASCII string with the same seed-1 algorithm to obtain the module fingerprint.

These noncryptographic fingerprints do not prove byte identity or authenticity. Equal unpacked size alone is weaker still. Neither check determines premium ownership, developer/tester entitlement or Unreal compatibility. Public target checks must distinguish client and server packages; numeric file-ID order is not a branch-selection rule.

## Cosmetic behavior and additional reports

Complete dynamically downloaded cosmetic files were observed on disk without an ordinary installed-mod listing. Browser absence therefore does not establish physical uninstallation. Main-menu **Uninstall suppressed cosmetics** also removed an owned, explicitly installed cosmetic in the suppression test. These behaviors are relevant to inventory and recovery UX; they do not by themselves demonstrate random deletion.

Two additional reports remain unconfirmed:

- **Development skins reverting or disappearing:** a developer recalled unpublished revisions reverting to the public version and later appearing to uninstall. Before/after file IDs, folder inventories and the triggering action are needed to distinguish revision replacement, hidden dynamic content and actual removal. Public and unpublished revisions sharing a project ID is normal; that alone does not establish a synchronization defect.
- **Dedicated-server startup during CurseForge outages:** an operator reports that the server fails to start when the API cannot be reached. The exact game build, failure log and DNS/TLS/HTTP outcome are needed to identify the failing stage. This report includes no controlled outage reproduction or internal call trace.

## Requested engineering review

1. Verify complete required content before loading or presenting an installation as healthy, and re-evaluate recovered `Invalid` entries after successful validation.
2. Reconcile pending transactions with actual resume/staging data and provide recovery controls for stranded records that the installed-list UI does not expose.
3. Update related publication/version records consistently while preserving explicit public/development/server-required revision intent.
4. Preserve the previous complete installation until a replacement and its metadata are committed; avoid destructive cleanup while ownership of a folder or transaction is unresolved.
5. Keep API unavailability separate from local corruption. Review whether an explicitly configured, locally verified, last-known-good server deployment can start without a fresh catalog query when all existing authorization and compatibility requirements remain satisfied.

The public SDK documents validation and synchronization operations, but identifying the correct integration points requires review of ARK's actual bundled implementation. No executable addresses, private function names or internal call origins are asserted here.

References: [CurseForge library and validation API](https://docs.curseforge.com/docs/game-integration/unreal/installing-mods/), [Wildcard's unpublished-mod testing workflow](https://devkit.studiowildcard.com/getting-started/testing).

[C++ startup-validation proposal](Startup-Validation-Proposal.md) · [ASA Mod Doctor](https://github.com/bushome/arkascendedpcsinglemodrepairguide/releases/tag/Tool)

