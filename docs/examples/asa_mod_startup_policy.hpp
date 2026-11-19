#pragma once
// Original reference policy for the accompanying findings report, not ASA Mod Doctor source.
// No ARK/Unreal/CurseForge binary integration, network calls, JSON writes or entitlement bypass.
// The game adapter must actually collect and verify every observation supplied here.
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace asa_review {

enum class Status { Normal, OutOfDate, Pending, Invalid, Uninstalled, Other };
enum class Integrity { Match, Mismatch, Missing, Unavailable, Changed, NotChecked };
enum class Platform { Windows, WindowsGdk, WindowsServer, LinuxServer };
enum class LocalResult {
    VerifiedBytes, InvalidRecord, UnsafePath, IncompleteReference,
    MissingOrDamaged, InspectionUnavailable, ChangedDuringCheck,
    InstallInProgress, ResumeCandidate, RestartDownloadRequired,
    PendingNeedsInspection, RecoveredFilesNeedStateValidation, NotLoadable
};

// A copied, immutable observation: never pass live UObject/SDK state to a hashing worker.
struct Observation {
    std::uint64_t generation = 0;
    std::int64_t gameId = 83374, projectId = 0, installedProjectId = 0;
    std::int64_t installedFileId = 0, latestKnownFileId = 0;
    Platform platform = Platform::Windows;
    Status status = Status::Other;
    std::string installedPublication;
    bool intentionalTestBuild = false, unmanaged = false, dynamicContent = false;
    bool identityComplete = false, pathsContainedAndNotLinked = false;
    bool moduleInventoryComplete = false, completeFileCoverage = false;
    bool actualSizeMatches = false, contentStable = false;
    Integrity installedReferenceIntegrity = Integrity::NotChecked;
    bool queuedOrInstalling = false;
    // Range validation includes archive identity/length, bounds, coverage and actual bytes.
    // A recorded canResume=true alone must not set these observations to true.
    bool partialInspectionCompleted = false, partialArchiveExists = false;
    bool partialRangesVerified = false, partialPathsContainedAndNotLinked = false;
};

inline LocalResult AssessLocal(const Observation& o) {
    if (o.gameId != 83374 || o.projectId <= 0) return LocalResult::InvalidRecord;
    // Pending is a separate transaction, even if an old installed revision still hashes correctly.
    if (o.status == Status::Pending) {
        if (o.queuedOrInstalling) return LocalResult::InstallInProgress;
        if (!o.partialInspectionCompleted) return LocalResult::PendingNeedsInspection;
        if (o.partialArchiveExists && !o.partialPathsContainedAndNotLinked) return LocalResult::UnsafePath;
        return o.partialArchiveExists && o.partialRangesVerified
            ? LocalResult::ResumeCandidate : LocalResult::RestartDownloadRequired;
    }
    if (o.gameId != 83374 || o.projectId <= 0 || o.installedFileId <= 0 ||
        o.installedProjectId != o.projectId || !o.identityComplete || o.unmanaged)
        return LocalResult::InvalidRecord;
    if (!o.pathsContainedAndNotLinked) return LocalResult::UnsafePath;
    if (!o.moduleInventoryComplete || !o.completeFileCoverage) return LocalResult::IncompleteReference;
    if (o.installedReferenceIntegrity == Integrity::Changed || !o.contentStable)
        return LocalResult::ChangedDuringCheck;
    if (o.installedReferenceIntegrity == Integrity::Unavailable ||
        o.installedReferenceIntegrity == Integrity::NotChecked) return LocalResult::InspectionUnavailable;
    if (o.installedReferenceIntegrity != Integrity::Match || !o.actualSizeMatches)
        return LocalResult::MissingOrDamaged;
    if (o.status == Status::Invalid) return LocalResult::RecoveredFilesNeedStateValidation;
    if (o.status != Status::Normal && o.status != Status::OutOfDate) return LocalResult::NotLoadable;
    return LocalResult::VerifiedBytes;
}

struct PublicReference {
    bool available = false, publicRelease = false, complete = false;
    bool sameTargetBeforeAndAfterHash = false;
    std::int64_t gameId = 83374, projectId = 0, fileId = 0;
    Platform platform = Platform::Windows;
    std::string publication;
};

struct LiveVerification {
    // Filled by hashing ACTUAL contents against the live reference, not the cached installed one.
    std::uint64_t generation = 0;
    Integrity integrity = Integrity::NotChecked;
    bool completeCoverage = false, sizeMatches = false, stable = false;
};

struct FieldChange { std::string field, before, after; };
struct MetadataProposal {
    std::uint64_t expectedGeneration = 0;
    std::int64_t projectId = 0, installedFileId = 0;
    std::vector<FieldChange> changes;
};

inline MetadataProposal SuggestPublicReconciliation(const Observation& o,
    const PublicReference& ref, const LiveVerification& verified,
    std::uint64_t currentGeneration) {
    MetadataProposal p;
    // Deliberately returns NO mutation when the API is unavailable, the target differs,
    // explicit developer intent exists, or local state cannot be safely normalized.
    if (o.intentionalTestBuild || o.unmanaged || o.queuedOrInstalling ||
        AssessLocal(o) != LocalResult::VerifiedBytes ||
        (o.status != Status::Normal && o.status != Status::OutOfDate) ||
        !o.identityComplete || o.gameId != 83374 || o.projectId <= 0 ||
        o.installedProjectId != o.projectId || o.installedFileId <= 0 ||
        !o.pathsContainedAndNotLinked || !o.moduleInventoryComplete ||
        o.generation != currentGeneration || verified.generation != currentGeneration ||
        !ref.available || !ref.publicRelease || !ref.complete || !ref.sameTargetBeforeAndAfterHash ||
        ref.gameId != o.gameId || ref.projectId != o.projectId || ref.fileId != o.installedFileId ||
        ref.platform != o.platform || (ref.publication != "Approved" && ref.publication != "Released") ||
        verified.integrity != Integrity::Match || !verified.completeCoverage ||
        !verified.sizeMatches || !verified.stable) return p;
    p.expectedGeneration = currentGeneration; p.projectId = o.projectId; p.installedFileId = o.installedFileId;
    if (o.installedPublication != ref.publication)
        p.changes.push_back({"Installed publication", o.installedPublication, ref.publication});
    if (o.latestKnownFileId != ref.fileId)
        p.changes.push_back({"Latest-known FILE RECORD", std::to_string(o.latestKnownFileId), std::to_string(ref.fileId)});
    if (o.status == Status::OutOfDate) p.changes.push_back({"Status", "OutOfDate", "Normal"});
    // Not an editor. A production owner must review all associated same-ID cached records,
    // regenerate the exact field preview and stage the COMPLETE known target file metadata.
    // Never implement Latest-known FILE RECORD as an isolated numeric ID substitution.
    return p;
}

struct RequiredBuild {
    std::int64_t projectId = 0, fileId = 0;
    Platform platform = Platform::WindowsServer;
};

struct OfflinePolicy {
    bool operatorExplicitlyAllowsLastKnownGood = false;
    bool deploymentManifestIsTrusted = false;
    bool allTransitiveDependenciesPinned = false;
    bool authorizationAndCompatibilityChecksSatisfied = false;
};

// A possible SERVER fallback decision only. It neither changes status nor calls the loader.
// A trusted deployment manifest is not merely the contents of arbitrary cached library.json.
inline bool MayConsiderOfflinePinnedStartup(const std::vector<RequiredBuild>& required,
    const std::vector<Observation>& installed, const OfflinePolicy& policy,
    std::uint64_t currentGeneration) {
    if (!policy.operatorExplicitlyAllowsLastKnownGood || !policy.deploymentManifestIsTrusted ||
        !policy.allTransitiveDependenciesPinned || !policy.authorizationAndCompatibilityChecksSatisfied ||
        required.empty()) return false;
    std::vector<std::int64_t> seen;
    for (const auto& pin : required) {
        if (pin.projectId <= 0 || pin.fileId <= 0 ||
            (pin.platform != Platform::WindowsServer && pin.platform != Platform::LinuxServer) ||
            std::find(seen.begin(), seen.end(), pin.projectId) != seen.end()) return false;
        seen.push_back(pin.projectId);
        const Observation* match = nullptr;
        for (const auto& o : installed) if (o.projectId == pin.projectId && o.platform == pin.platform) {
            if (match != nullptr) return false;
            match = &o;
        }
        if (match == nullptr || match->installedFileId != pin.fileId || match->generation != currentGeneration ||
            match->queuedOrInstalling || AssessLocal(*match) != LocalResult::VerifiedBytes) return false;
    }
    return true;
}
} // namespace asa_review

