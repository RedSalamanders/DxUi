#pragma once
// The performance fixture catalog (plan W0.4): FX1-FX14, one header each, every one listed here once. Both test
// executables include this header, so a fixture is defined once and both runners, the receipt writer, the timing harness
// and the determinism gate find it here.
//
// Convention for concurrent work: each fixture already has its own header and its own line below, also when it is only
// declared (no create function yet). Building a fixture edits only Embedded.Tests.PerformanceFixtureFx<N>.h; adding a
// scene helper adds a new file next to it (registered in Tests/native-test-files.json). Never reorder or reformat this
// list: two fixtures built at the same time then touch different files and never this one. A fixture beyond FX14 adds
// one include and one entry at the end, each on its own line.
// Scenarios are owned in place and never copied: their implicitly deleted copy and move operations are intended.
// Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
#include "Embedded.Tests.PerformanceFixtureFx1.h"
#include "Embedded.Tests.PerformanceFixtureFx10.h"
#include "Embedded.Tests.PerformanceFixtureFx11.h"
#include "Embedded.Tests.PerformanceFixtureFx12.h"
#include "Embedded.Tests.PerformanceFixtureFx13.h"
#include "Embedded.Tests.PerformanceFixtureFx14.h"
#include "Embedded.Tests.PerformanceFixtureFx2.h"
#include "Embedded.Tests.PerformanceFixtureFx3.h"
#include "Embedded.Tests.PerformanceFixtureFx4.h"
#include "Embedded.Tests.PerformanceFixtureFx5.h"
#include "Embedded.Tests.PerformanceFixtureFx6.h"
#include "Embedded.Tests.PerformanceFixtureFx7.h"
#include "Embedded.Tests.PerformanceFixtureFx8.h"
#include "Embedded.Tests.PerformanceFixtureFx9.h"
#pragma warning(pop)

namespace PerformanceFixtures
{
inline constexpr std::array<const Definition*, 14> kCatalog{
    &Fx1::kDefinition,
    &Fx2::kDefinition,
    &Fx3::kDefinition,
    &Fx4::kDefinition,
    &Fx5::kDefinition,
    &Fx6::kDefinition,
    &Fx7::kDefinition,
    &Fx8::kDefinition,
    &Fx9::kDefinition,
    &Fx10::kDefinition,
    &Fx11::kDefinition,
    &Fx12::kDefinition,
    &Fx13::kDefinition,
    &Fx14::kDefinition,
};

[[nodiscard]] inline const Definition* FindFixture(std::string_view id) noexcept
{
    for (const Definition* definition : kCatalog)
        if (definition->id == id)
            return definition;
    return nullptr;
}

// The variant a selection names: its default (variants[0]) when the name is empty.
[[nodiscard]] inline const Variant* FindVariant(const Definition& definition, std::string_view name) noexcept
{
    if (definition.variants.empty())
        return nullptr;
    return name.empty() ? &definition.variants.front() : definition.FindVariant(name);
}
} // namespace PerformanceFixtures
