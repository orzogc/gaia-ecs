#include "test_common.h"

#include <utility>

template <uint32_t I>
struct ComponentCacheLargeIdProbe {
	uint32_t value = I;
};

template <uint32_t I>
uint32_t component_cache_large_id_probe_type_id() {
	return gaia::meta::type_info::id<ComponentCacheLargeIdProbe<I>>();
}

template <size_t... Is>
auto make_component_cache_large_id_probe_table(std::index_sequence<Is...>) {
	using ProbeIdFunc = uint32_t (*)();
	return cnt::sarr<ProbeIdFunc, sizeof...(Is)>{&component_cache_large_id_probe_type_id<(uint32_t)Is>...};
}

TEST_CASE("Component cache - compile time registration") {
	SUBCASE("large component ids use the map-backed storage path") {
		TestWorld twld;
		auto& cc = wld.comp_cache_mut();

		constexpr uint32_t ProbeCount = 513;
		static const auto probeIds = make_component_cache_large_id_probe_table(std::make_index_sequence<ProbeCount>{});

		uint32_t largeCompDescId = 0;
		GAIA_FOR(ProbeCount) {
			largeCompDescId = probeIds[i]();
		}
		CHECK(largeCompDescId >= 512);

		const auto& item = wld.add<ComponentCacheLargeIdProbe<ProbeCount - 1>>();

		CHECK(item.comp.id() == largeCompDescId);
		CHECK(item.comp.id() >= 512);
		CHECK(cc.find(item.comp.id()) == &item);
		CHECK(cc.get(item.comp.id()).entity == item.entity);
		CHECK(cc.find<ComponentCacheLargeIdProbe<ProbeCount - 1>>() == &item);
		CHECK(cc.get<ComponentCacheLargeIdProbe<ProbeCount - 1>>().entity == item.entity);
	}
}
