# C++ startup-validation proposal for ARK mod libraries

18 November 2026. Companion to [ASA Mod Doctor: technical findings](ASA-Mod-Doctor-Technical-Findings.md).

This is an original reference design for Wildcard review. It is **not** a patch for `ArkAscended.exe` or `ArkAscendedServer.exe`, a DevKit mod claiming access to native startup, or a verified reconstruction of their request call sites. The application source and private game/SDK headers are not available here. The proposal requires integration by the owner of the game's mod lifecycle.

Files: [C++17 policy header](examples/asa_mod_startup_policy.hpp), [standalone policy tests](examples/startup_policy_tests.cpp).

## Intended sequence

Run a local inspection before mounting required mod content and before destructive cleanup can discard diagnostic state. Where SDK initialization can run cleanup, the owner needs to capture the relevant persisted state earlier or supply an equivalent nondestructive inventory. Do not assume the ordinary installed-mod UI includes every stranded Pending record.

The first pass should not need an HTTP request. It checks actual identity, containment, complete module inventory, actual unpacked size and content fingerprints against the appropriate recorded/pinned reference. A `Pending` update remains a transaction even if the old installed revision is complete. Only a real partial archive with verified ranges is a resume candidate. A restored `Invalid` installation needs a recovery decision rather than being silently loaded.

Public freshness is a separate optional pass. It resolves the exact public platform target and verifies actual files against that reference. Developer/test intent or a server-required exact revision must take precedence over a public-default lookup. The SDK's documented initialization completion and game-thread requirements must be respected; a generic engine/module startup callback is not sufficient proof that the library interface is ready. [Initialization reference](https://docs.curseforge.com/docs/game-integration/unreal/initialization-and-settings/)

## Engine integration sketch

The following adapter names are **contracts to implement**, not real SDK functions. The sketch is intentionally separate from the compiled, engine-independent header. It demonstrates ownership and threading, not an assumed ARK binary hook.

```cpp
// Called by the game's own startup coordinator, at an integration-defined safe point.
// IGameModAdapter below represents code Wildcard would need to implement.
void BeginClientModInspection(std::shared_ptr<IGameModAdapter> adapter)
{
    // GAME THREAD: copy complete persisted state, target intent and generation.
    // No network access, JSON mutation, cleanup or ownership inference here.
    auto snapshot = adapter->CaptureCompleteReadOnlySnapshot();
    std::weak_ptr<IGameModAdapter> weak = adapter;

    adapter->InspectSnapshotOnWorker(snapshot,
        [weak, generation = snapshot.Generation](auto observations) mutable
        {
            if (auto owner = weak.lock())
            {
                owner->PostToGameThread(
                    [weak, generation, observations = std::move(observations)]() mutable
                    {
                        auto game = weak.lock();
                        if (!game || !game->GenerationStillMatches(generation)) return;
                        // Present Pending/Invalid/unavailable separately, including hidden records.
                        // AssessLocal() is from the standalone reference header.
                        for (const auto& observation : observations)
                            game->PresentInspection(observation,
                                asa_review::AssessLocal(observation));

                        // Existing licensing, signatures, dependency, ABI and branch rules still apply.
                        // Network failure must not be translated into a corrupt-file result.
                        game->ContinueStartupThroughExistingAuthorizationAndLoadPolicy();
                    });
            }
        });
}
```

`InspectSnapshotOnWorker` must open safely contained paths, reject unsupported links/reparse points, build a full inventory, calculate the file/module fingerprints described in the findings report, and detect mutations before returning immutable observations. It must distinguish missing data from denied/locked/unreadable data. Expected fingerprints must identify the installed/pinned revision, not an unrelated newest public cook. For large libraries, stream file data, support cancellation/progress and avoid blocking the game thread. Revalidation is required after any long-running operation; metadata timestamps alone are insufficient.

Calling `ContinueStartupThroughExistingAuthorizationAndLoadPolicy` does not authorize loading every inspected record. The game's existing policy must block incomplete/mismatched required content and independently enforce all authorization, dependency and compatibility rules. Dynamic cosmetic visibility does not establish ownership. An optional cosmetic's inspection failure need not be treated identically to a missing required map or gameplay mod.

## Metadata correction is a separate transaction

`SuggestPublicReconciliation` returns proposed field changes only. It requires stable matching generations, the same project/file/platform, complete available public metadata, a consistent target before/after live-reference hashing and a complete live content match. It returns no correction for an unavailable API, another installed revision, Pending/Invalid state or intentional test build.

The proposal is not a writer. A real owner must reacquire a serialized operation gate, recheck the exact state/reference/content, enumerate all proposed associated field changes, persist a durable recovery record, stage atomically where supported, and validate commit or recover. Refreshing a latest-known record must update its associated file metadata; changing only its ID would create a new inconsistency. Preserve unknown fields and pair related SDK records only when their schema requires it. Never directly rewrite the live SDK's JSON behind its back.

The documented SDK exposes `PerformModsValidation`, `SyncWithServer`, installation and cleanup operations, but those are not interchangeable read-only checks. The integration owner should inspect the actual bundled implementation and sequencing rather than blindly add a call at startup. [Library API reference](https://docs.curseforge.com/docs/game-integration/unreal/installing-mods/)

## Optional dedicated-server API-outage policy

The operator's report that API unavailability prevents startup still needs a captured failure log and controlled reproduction. We cannot name its originating function in the game executables. The documented `AssureServerModsUpdated` flow is relevant background, not proof of ARK's exact failure path. [Server API reference](https://docs.curseforge.com/docs/game-integration/unreal/multiplayer-and-servers/)

The header includes `MayConsiderOfflinePinnedStartup` as a possible explicit last-known-good policy. It requires a trusted deployment manifest pinning every required project, exact file, platform and transitive dependency; current stable local verification; and independently satisfied authorization/compatibility checks. It rejects missing, duplicate, mismatched or Pending required installs. It does not infer a trusted manifest from arbitrary cached JSON, treat stale fingerprints as proof of current publication, or grant premium/tester access.

An eligible offline deployment would still be labelled **local contents verified; remote freshness unavailable**. Preserve files and retry the service with bounded backoff. Where a required reference, version agreement or authorization cannot be established under the game's rules, fail with a specific actionable reason. Do not erase a valid installation merely because DNS, TLS, HTTP or authentication failed. A real implementation may need changes earlier than this sketch if SDK initialization itself requires network access.

## Build and verification

The header and its tests use only the C++17 standard library. They contain no filesystem or HTTP adapter and no Unreal/CurseForge linkage; observations supplied by an adapter must actually have been measured. Standalone compilation validates policy decisions, not ARK integration or the completeness of a production validator.

On 18 November 2026, MSVC compiled the standalone header/tests as C++17 with `/W4 /WX`, and all 28 policy checks passed. The engine integration sketch above was not compiled against private game headers.

From a Visual Studio Developer Command Prompt:

```text
cl /nologo /std:c++17 /EHsc /W4 /WX examples\startup_policy_tests.cpp /Fe:startup_policy_tests.exe
startup_policy_tests.exe
```

Or with an appropriate C++17 compiler:

```text
c++ -std=c++17 -Wall -Wextra -pedantic examples/startup_policy_tests.cpp -o startup_policy_tests
./startup_policy_tests
```

Coverage includes complete local contents, same-size corruption, inaccessible/changing files, unsafe paths, incomplete coverage, restored Invalid state, missing/valid partial downloads, active transactions, ASR-like proposals, API outage, actual different targets, generation conflicts, intentional test builds, wrong platforms, no-op proposals, and opt-in pinned server fallback without bypassing game authorization. The actual SDK/engine adapter, durable commit implementation and game-start behavior are deliberately not claimed as compiled or tested.

