#include "asa_mod_startup_policy.hpp"
#include <iostream>
#include <stdexcept>
using namespace asa_review;

static int checks = 0;
static void Check(bool result, const char* name) {
    if (!result) throw std::runtime_error(name);
    ++checks;
}
static Observation Healthy() {
    Observation o;
    o.generation = 7; o.projectId = o.installedProjectId = 42;
    o.installedFileId = o.latestKnownFileId = 100; o.status = Status::Normal;
    o.installedPublication = "Approved";
    o.identityComplete = o.pathsContainedAndNotLinked = o.moduleInventoryComplete = true;
    o.completeFileCoverage = o.actualSizeMatches = o.contentStable = true;
    o.installedReferenceIntegrity = Integrity::Match;
    return o;
}
int main() {
    try {
        auto o = Healthy(); Check(AssessLocal(o) == LocalResult::VerifiedBytes, "normal verified bytes");
        o.installedReferenceIntegrity = Integrity::Mismatch;
        Check(AssessLocal(o) == LocalResult::MissingOrDamaged, "same-size corruption is not healthy");
        o = Healthy(); o.installedReferenceIntegrity = Integrity::Unavailable;
        Check(AssessLocal(o) == LocalResult::InspectionUnavailable, "denied access is not corruption");
        o = Healthy(); o.contentStable = false;
        Check(AssessLocal(o) == LocalResult::ChangedDuringCheck, "changing content invalidates result");
        o = Healthy(); o.pathsContainedAndNotLinked = false;
        Check(AssessLocal(o) == LocalResult::UnsafePath, "unsafe path never loads");
        o = Healthy(); o.completeFileCoverage = false;
        Check(AssessLocal(o) == LocalResult::IncompleteReference, "extra files prevent complete validation");
        o = Healthy(); o.status = Status::Invalid;
        Check(AssessLocal(o) == LocalResult::RecoveredFilesNeedStateValidation, "Invalid is not silently normalized");
        o = Healthy(); o.status = Status::Pending;
        Check(AssessLocal(o) == LocalResult::PendingNeedsInspection, "old matching bytes do not complete update");
        o.partialInspectionCompleted = true;
        Check(AssessLocal(o) == LocalResult::RestartDownloadRequired, "missing partial cannot resume");
        o.partialArchiveExists = o.partialRangesVerified = true;
        Check(AssessLocal(o) == LocalResult::UnsafePath, "partial archive paths must be inspected too");
        o.partialPathsContainedAndNotLinked = true;
        Check(AssessLocal(o) == LocalResult::ResumeCandidate, "validated partial can be offered for resume");
        o.queuedOrInstalling = true;
        Check(AssessLocal(o) == LocalResult::InstallInProgress, "active transaction is preserved");

        PublicReference ref; ref.available = ref.publicRelease = ref.complete = ref.sameTargetBeforeAndAfterHash = true;
        ref.projectId = 42; ref.fileId = 100; ref.publication = "Approved";
        LiveVerification verified; verified.generation = 7; verified.integrity = Integrity::Match;
        verified.completeCoverage = verified.sizeMatches = verified.stable = true;
        o = Healthy(); o.status = Status::OutOfDate; o.latestKnownFileId = 50; o.installedPublication = "ReadyForReview";
        Check(SuggestPublicReconciliation(o, ref, verified, 7).changes.size() == 3, "ASR-like proposal");
        auto unavailable = ref; unavailable.available = false;
        Check(SuggestPublicReconciliation(o, unavailable, verified, 7).changes.empty(), "offline cannot certify current public release");
        auto other = ref; other.fileId = 101;
        Check(SuggestPublicReconciliation(o, other, verified, 7).changes.empty(), "other file needs actual update");
        Check(SuggestPublicReconciliation(o, ref, verified, 8).changes.empty(), "generation conflict aborts");
        o.intentionalTestBuild = true;
        Check(SuggestPublicReconciliation(o, ref, verified, 7).changes.empty(), "intentional branch is preserved");
        o = Healthy(); o.status = Status::Pending;
        Check(SuggestPublicReconciliation(o, ref, verified, 7).changes.empty(), "Pending cannot be normalized");
        o = Healthy(); auto wrongPlatform = ref; wrongPlatform.platform = Platform::WindowsServer;
        Check(SuggestPublicReconciliation(o, wrongPlatform, verified, 7).changes.empty(), "client/server packages are distinct");
        Check(SuggestPublicReconciliation(o, ref, verified, 7).changes.empty(), "healthy metadata is no-op");
        o.installedReferenceIntegrity = Integrity::Mismatch;
        Check(SuggestPublicReconciliation(o, ref, verified, 7).changes.empty(), "cached content disagreement needs separate recovery");

        o = Healthy(); o.platform = Platform::WindowsServer;
        const std::vector<RequiredBuild> pins = {{42, 100, Platform::WindowsServer}};
        OfflinePolicy policy;
        Check(!MayConsiderOfflinePinnedStartup(pins, {o}, policy, 7), "offline fallback is opt-in");
        policy.operatorExplicitlyAllowsLastKnownGood = policy.deploymentManifestIsTrusted = true;
        policy.allTransitiveDependenciesPinned = policy.authorizationAndCompatibilityChecksSatisfied = true;
        Check(MayConsiderOfflinePinnedStartup(pins, {o}, policy, 7), "pinned verified offline candidate");
        Check(!MayConsiderOfflinePinnedStartup(pins, {}, policy, 7), "missing required mod blocks startup");
        Check(!MayConsiderOfflinePinnedStartup(pins, {o, o}, policy, 7), "ambiguous install blocks fallback");
        auto wrong = o; wrong.installedFileId = 101;
        Check(!MayConsiderOfflinePinnedStartup(pins, {wrong}, policy, 7), "wrong pinned file blocks fallback");
        o.status = Status::Pending;
        Check(!MayConsiderOfflinePinnedStartup(pins, {o}, policy, 7), "pending required mod blocks fallback");
        o = Healthy(); o.platform = Platform::WindowsServer; policy.authorizationAndCompatibilityChecksSatisfied = false;
        Check(!MayConsiderOfflinePinnedStartup(pins, {o}, policy, 7), "fallback cannot bypass game authorization");
        std::cout << "Passed " << checks << " standalone startup-policy checks.\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}

