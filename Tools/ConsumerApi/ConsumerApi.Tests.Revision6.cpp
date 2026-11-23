// Consumer compile fixture for API revision 6. No private headers or test flags.
// Revision 6 is unreleased: each of its source breaks adds one numbered section below, in the order of the list in
// docs/upgrade.md ("API 6") and Specs/Build/Build_ToolchainAndConsumption.md, and the fixture is frozen at release.
// Each section asserts the new surface and that the revision-5 form it replaces no longer compiles.
#include <DxUi/DxUi.h>
#include <DxUi/Embedded.h>

#include <concepts>
#include <cstdint>
#include <type_traits>

// 1. Embedded animation request (W3.5): GetAnimationRequest and AdvanceAnimation return an EmbeddedAnimationRequest on the
// application's own clock; NeedsAnimation() and the bool AdvanceAnimation are gone.
namespace Revision6AnimationRequest
{
static_assert(DXUI_HAS_EMBEDDED_ANIMATION_REQUEST == 1);
static_assert(std::is_same_v<decltype(&DxUi::EmbeddedHost::GetAnimationRequest), DxUi::EmbeddedAnimationRequest (DxUi::EmbeddedHost::*)() const noexcept>);
static_assert(std::is_same_v<decltype(&DxUi::EmbeddedHost::AdvanceAnimation), DxUi::EmbeddedAnimationRequest (DxUi::EmbeddedHost::*)(uint64_t) noexcept>);
static_assert(std::is_same_v<decltype(DxUi::EmbeddedAnimationRequest{}.kind), DxUi::EmbeddedAnimationRequest::Kind>);
static_assert(std::is_same_v<decltype(DxUi::EmbeddedAnimationRequest{}.dueTickMs), uint64_t>);
static_assert(DxUi::EmbeddedAnimationRequest{}.kind == DxUi::EmbeddedAnimationRequest::Kind::None);
// The application's clock, required at Attach: the clock it passes to AdvanceAnimation, read where a stamp is taken.
static_assert(DXUI_HAS_EMBEDDED_APPLICATION_CLOCK == 1);
static_assert(std::is_same_v<decltype(DxUi::EmbeddedCallbacks{}.nowTickMs), uint64_t (*)(void*) noexcept>);
[[maybe_unused]] inline constexpr DxUi::EmbeddedCallbacks kCallbacks{.context = nullptr, .requestPreparation = nullptr, .nowTickMs = nullptr};

template <class Host>
concept HasNeedsAnimation = requires(const Host& host) { host.NeedsAnimation(); };
template <class Host>
concept AdvanceReturnsBool = requires(Host& host) {
    { host.AdvanceAnimation(uint64_t{}) } -> std::convertible_to<bool>;
};
static_assert(! HasNeedsAnimation<DxUi::EmbeddedHost>);
static_assert(! AdvanceReturnsBool<DxUi::EmbeddedHost>);

// The smallest correct loop of an application that sleeps until the request (docs/hosting.md): returns how long to sleep,
// in milliseconds of the clock passed to AdvanceAnimation, or UINT64_MAX to sleep until the next requestPreparation.
inline uint64_t AdvanceAndPlanSleep(DxUi::EmbeddedHost& view, uint64_t nowTickMs, uint64_t frameMs) noexcept
{
    DxUi::EmbeddedAnimationRequest request = view.GetAnimationRequest();
    if (request.kind == DxUi::EmbeddedAnimationRequest::Kind::Continuous ||
        (request.kind == DxUi::EmbeddedAnimationRequest::Kind::Deadline && request.dueTickMs <= nowTickMs))
        request = view.AdvanceAnimation(nowTickMs);
    switch (request.kind)
    {
        case DxUi::EmbeddedAnimationRequest::Kind::None: return UINT64_MAX;
        case DxUi::EmbeddedAnimationRequest::Kind::Continuous: return frameMs;
        case DxUi::EmbeddedAnimationRequest::Kind::Deadline: return request.dueTickMs > nowTickMs ? request.dueTickMs - nowTickMs : 0u;
    }
    return UINT64_MAX;
}
} // namespace Revision6AnimationRequest

// 2. One user-settings call (W3.5): ControlHost::RefreshUserSettings re-reads every user setting a host keeps; the
// per-setting refresh calls of earlier development builds are not public.
namespace Revision6UserSettings
{
static_assert(std::is_same_v<decltype(&DxUi::ControlHost::RefreshUserSettings), void (DxUi::ControlHost::*)() noexcept>);
template <class Host>
concept HasPublicPerSettingRefresh = requires(Host& host) { host.RefreshCaretBlinkSettings(); } || requires(Host& host) { host.RefreshTooltipHoverTime(); };
static_assert(! HasPublicPerSettingRefresh<DxUi::ControlHost>);
} // namespace Revision6UserSettings
