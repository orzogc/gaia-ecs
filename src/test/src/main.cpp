#include <gaia.h>

#if GAIA_COMPILER_MSVC
	#if _MSC_VER <= 1916
// warning C4100: 'XYZ': unreferenced formal parameter
GAIA_MSVC_WARNING_DISABLE(4100)
	#endif
#endif

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include <string>

using namespace gaia;

struct Int3 {
	uint32_t x, y, z;
};
struct Position {
	float x, y, z;
};
struct PositionSoA {
	float x, y, z;
	static constexpr auto Layout = utils::DataLayout::SoA;
};
struct PositionSoA8 {
	float x, y, z;
	static constexpr auto Layout = utils::DataLayout::SoA8;
};
struct PositionSoA16 {
	float x, y, z;
	static constexpr auto Layout = utils::DataLayout::SoA16;
};
struct Acceleration {
	float x, y, z;
};
struct Rotation {
	float x, y, z, w;
};
struct Scale {
	float x, y, z;
};
struct Something {
	bool value;
};
struct Else {
	bool value;
};

TEST_CASE("ComponentTypes") {
	REQUIRE(ecs::component::IsGenericComponent<uint32_t> == true);
	REQUIRE(ecs::component::IsGenericComponent<Position> == true);
	REQUIRE(ecs::component::IsGenericComponent<ecs::AsChunk<Position>> == false);
}

TEST_CASE("Containers - sarray") {
	containers::sarray<uint32_t, 5> arr = {0, 1, 2, 3, 4};
	REQUIRE(arr[0] == 0);
	REQUIRE(arr[1] == 1);
	REQUIRE(arr[2] == 2);
	REQUIRE(arr[3] == 3);
	REQUIRE(arr[4] == 4);

	size_t cnt = 0;
	for (auto val: arr) {
		REQUIRE(val == cnt);
		++cnt;
	}
	REQUIRE(cnt == 5);
	REQUIRE(cnt == arr.size());

	REQUIRE(utils::find(arr, 0) == arr.begin());
	REQUIRE(utils::find(arr, 100) == arr.end());

	REQUIRE(utils::has(arr, 0));
	REQUIRE(!utils::has(arr, 100));
}

TEST_CASE("Containers - sarray_ext") {
	containers::sarray_ext<uint32_t, 5> arr;
	arr.push_back(0);
	REQUIRE(arr[0] == 0);
	arr.push_back(1);
	REQUIRE(arr[1] == 1);
	arr.push_back(2);
	REQUIRE(arr[2] == 2);
	arr.push_back(3);
	REQUIRE(arr[3] == 3);
	arr.push_back(4);
	REQUIRE(arr[4] == 4);

	size_t cnt = 0;
	for (auto val: arr) {
		REQUIRE(val == cnt);
		++cnt;
	}
	REQUIRE(cnt == 5);
	REQUIRE(cnt == arr.size());

	REQUIRE(utils::find(arr, 0) == arr.begin());
	REQUIRE(utils::find(arr, 100) == arr.end());

	REQUIRE(utils::has(arr, 0));
	REQUIRE(!utils::has(arr, 100));
}

TEST_CASE("Containers - darray") {
	containers::darray<uint32_t> arr;
	arr.push_back(0);
	REQUIRE(arr[0] == 0);
	arr.push_back(1);
	REQUIRE(arr[1] == 1);
	arr.push_back(2);
	REQUIRE(arr[2] == 2);
	arr.push_back(3);
	REQUIRE(arr[3] == 3);
	arr.push_back(4);
	REQUIRE(arr[4] == 4);
	arr.push_back(5);
	REQUIRE(arr[5] == 5);
	arr.push_back(6);
	REQUIRE(arr[6] == 6);

	size_t cnt = 0;
	for (auto val: arr) {
		REQUIRE(val == cnt);
		++cnt;
	}
	REQUIRE(cnt == 7);
	REQUIRE(cnt == arr.size());

	REQUIRE(utils::find(arr, 0) == arr.begin());
	REQUIRE(utils::find(arr, 100) == arr.end());

	REQUIRE(utils::has(arr, 0));
	REQUIRE(!utils::has(arr, 100));
}

TEST_CASE("EntityNull") {
	REQUIRE_FALSE(ecs::Entity{} == ecs::EntityNull);

	REQUIRE(ecs::EntityNull == ecs::EntityNull);
	REQUIRE_FALSE(ecs::EntityNull != ecs::EntityNull);

	ecs::World w;
	REQUIRE_FALSE(w.IsEntityValid(ecs::EntityNull));

	auto e = w.CreateEntity();
	REQUIRE(e != ecs::EntityNull);
	REQUIRE(ecs::EntityNull != e);
	REQUIRE_FALSE(e == ecs::EntityNull);
	REQUIRE_FALSE(ecs::EntityNull == e);
}

TEST_CASE("Compile-time sort descending") {
	containers::sarray<uint32_t, 5> arr = {4, 2, 1, 3, 0};
	utils::sort_ct(arr, utils::is_greater<uint32_t>());
	for (size_t i = 0; i < arr.size(); ++i)
		REQUIRE(arr[arr.size() - i - 1] == i);
}

TEST_CASE("Compile-time sort ascending") {
	containers::sarray<uint32_t, 5> arr = {4, 2, 1, 3, 0};
	utils::sort_ct(arr, utils::is_smaller<uint32_t>());
	for (size_t i = 0; i < arr.size(); ++i)
		REQUIRE(arr[i] == i);
}

TEST_CASE("Run-time sort - sorting network") {
	containers::sarray<uint32_t, 5> arr;
	for (uint32_t i = 0; i < arr.size(); ++i)
		arr[i] = i;

	utils::sort(arr, utils::is_greater<uint32_t>());
	for (size_t i = 0; i < arr.size(); ++i)
		REQUIRE(arr[arr.size() - i - 1] == i);

	utils::sort(arr, utils::is_smaller<uint32_t>());
	for (size_t i = 0; i < arr.size(); ++i)
		REQUIRE(arr[i] == i);
}

TEST_CASE("Run-time sort - bubble sort") {
	containers::sarray<uint32_t, 15> arr;
	for (uint32_t i = 0; i < arr.size(); ++i)
		arr[i] = i;

	utils::sort(arr, utils::is_greater<uint32_t>());
	for (size_t i = 0; i < arr.size(); ++i)
		REQUIRE(arr[arr.size() - i - 1] == i);

	utils::sort(arr, utils::is_smaller<uint32_t>());
	for (size_t i = 0; i < arr.size(); ++i)
		REQUIRE(arr[i] == i);
}

TEST_CASE("Run-time sort - quick sort") {
	containers::sarray<uint32_t, 45> arr;
	for (uint32_t i = 0; i < arr.size(); ++i)
		arr[i] = i;

	utils::sort(arr, utils::is_greater<uint32_t>());
	for (size_t i = 0; i < arr.size(); ++i)
		REQUIRE(arr[arr.size() - i - 1] == i);

	utils::sort(arr, utils::is_smaller<uint32_t>());
	for (size_t i = 0; i < arr.size(); ++i)
		REQUIRE(arr[i] == i);
}

// TEST_CASE("EntityQuery - equality") {
//  {
//  	ecs::EntityQuery qq1, qq2;
//  	qq1.All<Position, Rotation>();
//  	qq2.All<Rotation, Position>();
//  	REQUIRE(
//  			qq1.GetData(ecs::ComponentType::CT_Generic).hash[ecs::EntityQuery::ListType::LT_All] ==
//  			qq2.GetData(ecs::ComponentType::CT_Generic).hash[ecs::EntityQuery::ListType::LT_All]);
//  }
//  {
//  	ecs::EntityQuery qq1, qq2;
//  	qq1.All<Position, Rotation, Acceleration, Something>();
//  	qq2.All<Rotation, Something, Position, Acceleration>();
//  	REQUIRE(
//  			qq1.GetData(ecs::ComponentType::CT_Generic).hash[ecs::EntityQuery::ListType::LT_All] ==
//  			qq2.GetData(ecs::ComponentType::CT_Generic).hash[ecs::EntityQuery::ListType::LT_All]);
//  }
//}

TEST_CASE("EntityQuery - QueryResult") {
	ecs::World w;

	auto create = [&](uint32_t i) {
		auto e = w.CreateEntity();
		w.AddComponent<Position>(e, {(float)i, (float)i, (float)i});
	};

	const uint32_t N = 10'000;
	for (uint32_t i = 0; i < N; i++)
		create(i);

	ecs::EntityQuery q1 = w.CreateQuery().All<Position>();
	ecs::EntityQuery q2 = w.CreateQuery().All<Rotation>();
	ecs::EntityQuery q3 = w.CreateQuery().All<Position, Rotation>();

	{
		gaia::containers::darray<gaia::ecs::Entity> arr;
		q1.ToArray(arr);
		GAIA_ASSERT(arr.size() == N);
		for (size_t i = 0; i < arr.size(); ++i)
			REQUIRE(arr[i].id() == i);
	}
	{
		gaia::containers::darray<Position> arr;
		q1.ToArray(arr);
		GAIA_ASSERT(arr.size() == N);
		for (size_t i = 0; i < arr.size(); ++i) {
			const auto& pos = arr[i];
			REQUIRE(pos.x == (float)i);
			REQUIRE(pos.y == (float)i);
			REQUIRE(pos.z == (float)i);
		}
	}
	{
		gaia::containers::darray<gaia::ecs::Chunk*> arr;
		q1.ToChunkArray(arr);
		size_t entityCount = 0;
		for (const auto* pChunk: arr)
			entityCount += pChunk->GetEntityCount();
		REQUIRE(entityCount == N);
	}
	{
		const auto cnt = q1.CalculateEntityCount();
		const auto has = q1.HasEntities();

		size_t cnt2 = 0;
		q1.ForEach([&]() {
			++cnt2;
		});
		REQUIRE(cnt == cnt2);
		REQUIRE(cnt > 0);
		REQUIRE(has == true);
	}

	{
		const auto cnt = q2.CalculateEntityCount();
		const auto has = q2.HasEntities();

		size_t cnt2 = 0;
		q2.ForEach([&]() {
			++cnt2;
		});
		REQUIRE(cnt == cnt2);
		REQUIRE(cnt == 0);
		REQUIRE(has == false);
	}

	{
		const auto cnt = q3.CalculateEntityCount();
		const auto has = q3.HasEntities();

		size_t cnt3 = 0;
		q3.ForEach([&]() {
			++cnt3;
		});
		REQUIRE(cnt == cnt3);
		REQUIRE(cnt == 0);
		REQUIRE(has == false);
	}
}

TEST_CASE("EntityQuery - QueryResult complex") {
	ecs::World w;

	auto create = [&](uint32_t i) {
		auto e = w.CreateEntity();
		w.AddComponent<Position>(e, {(float)i, (float)i, (float)i});
		w.AddComponent<Scale>(e, {(float)i * 2, (float)i * 2, (float)i * 2});
		if (i % 2 == 0)
			w.AddComponent<Something>(e, {true});
	};

	const uint32_t N = 10'000;
	for (uint32_t i = 0; i < N; i++)
		create(i);

	ecs::EntityQuery q1 = w.CreateQuery().All<Position>();
	ecs::EntityQuery q2 = w.CreateQuery().All<Rotation>();
	ecs::EntityQuery q3 = w.CreateQuery().All<Position, Rotation>();
	ecs::EntityQuery q4 = w.CreateQuery().All<Position, Scale>();
	ecs::EntityQuery q5 = w.CreateQuery().All<Position, Scale, Something>();

	{
		gaia::containers::darray<gaia::ecs::Entity> ents;
		q1.ToArray(ents);
		gaia::containers::darray<Position> arr;
		q1.ToArray(arr);
		REQUIRE(ents.size() == arr.size());

		for (size_t i = 0; i < arr.size(); ++i) {
			const auto& pos = arr[i];
			const auto val = ents[i].id();
			REQUIRE(pos.x == (float)val);
			REQUIRE(pos.y == (float)val);
			REQUIRE(pos.z == (float)val);
		}
	}
	{
		gaia::containers::darray<gaia::ecs::Chunk*> arr;
		q1.ToChunkArray(arr);
		size_t entityCount = 0;
		for (const auto* pChunk: arr)
			entityCount += pChunk->GetEntityCount();
		REQUIRE(entityCount == N);
	}
	{
		const auto cnt = q1.CalculateEntityCount();
		const auto has = q1.HasEntities();

		size_t cnt2 = 0;
		q1.ForEach([&]() {
			++cnt2;
		});
		REQUIRE(cnt == cnt2);
		REQUIRE(cnt > 0);
		REQUIRE(has == true);
	}

	{
		const auto cnt = q2.CalculateEntityCount();
		const auto has = q2.HasEntities();

		size_t cnt2 = 0;
		q2.ForEach([&]() {
			++cnt2;
		});
		REQUIRE(cnt == cnt2);
		REQUIRE(cnt == 0);
		REQUIRE(has == false);
	}

	{
		const auto cnt = q3.CalculateEntityCount();
		const auto has = q3.HasEntities();

		size_t cnt3 = 0;
		q3.ForEach([&]() {
			++cnt3;
		});
		REQUIRE(cnt == cnt3);
		REQUIRE(cnt == 0);
		REQUIRE(has == false);
	}

	{
		gaia::containers::darray<gaia::ecs::Entity> ents;
		q4.ToArray(ents);
		gaia::containers::darray<Position> arr;
		q4.ToArray(arr);
		REQUIRE(ents.size() == arr.size());

		for (size_t i = 0; i < arr.size(); ++i) {
			const auto& pos = arr[i];
			const auto val = ents[i].id();
			REQUIRE(pos.x == (float)val);
			REQUIRE(pos.y == (float)val);
			REQUIRE(pos.z == (float)val);
		}
	}
	{
		gaia::containers::darray<gaia::ecs::Entity> ents;
		q4.ToArray(ents);
		gaia::containers::darray<Scale> arr;
		q4.ToArray(arr);
		REQUIRE(ents.size() == arr.size());

		for (size_t i = 0; i < arr.size(); ++i) {
			const auto& s = arr[i];
			const auto val = ents[i].id() * 2;
			REQUIRE(s.x == (float)val);
			REQUIRE(s.y == (float)val);
			REQUIRE(s.z == (float)val);
		}
	}
	{
		gaia::containers::darray<gaia::ecs::Chunk*> arr;
		q4.ToChunkArray(arr);
		size_t entityCount = 0;
		for (const auto* pChunk: arr)
			entityCount += pChunk->GetEntityCount();
		REQUIRE(entityCount == N);
	}
	{
		const auto cnt = q4.CalculateEntityCount();
		const auto has = q4.HasEntities();

		size_t cnt4 = 0;
		q4.ForEach([&]() {
			++cnt4;
		});
		REQUIRE(cnt == cnt4);
		REQUIRE(cnt > 0);
		REQUIRE(has == true);
	}

	{
		gaia::containers::darray<gaia::ecs::Entity> ents;
		q5.ToArray(ents);
		gaia::containers::darray<Position> arr;
		q5.ToArray(arr);
		REQUIRE(ents.size() == arr.size());

		for (size_t i = 0; i < arr.size(); ++i) {
			const auto& pos = arr[i];
			const auto val = ents[i].id();
			REQUIRE(pos.x == (float)val);
			REQUIRE(pos.y == (float)val);
			REQUIRE(pos.z == (float)val);
		}
	}
	{
		gaia::containers::darray<gaia::ecs::Entity> ents;
		q5.ToArray(ents);
		gaia::containers::darray<Scale> arr;
		q5.ToArray(arr);
		REQUIRE(ents.size() == arr.size());

		for (size_t i = 0; i < arr.size(); ++i) {
			const auto& s = arr[i];
			const auto val = ents[i].id() * 2;
			REQUIRE(s.x == (float)val);
			REQUIRE(s.y == (float)val);
			REQUIRE(s.z == (float)val);
		}
	}
	{
		gaia::containers::darray<gaia::ecs::Chunk*> arr;
		q5.ToChunkArray(arr);
		size_t entityCount = 0;
		for (const auto* pChunk: arr)
			entityCount += pChunk->GetEntityCount();
		REQUIRE(entityCount == N / 2);
	}
	{
		const auto cnt = q5.CalculateEntityCount();
		const auto has = q5.HasEntities();

		size_t cnt5 = 0;
		q5.ForEach([&]() {
			++cnt5;
		});
		REQUIRE(cnt == cnt5);
		REQUIRE(cnt > 0);
		REQUIRE(has == true);
	}
}

TEST_CASE("CreateEntity - no components") {
	ecs::World w;

	auto create = [&](uint32_t id) {
		auto e = w.CreateEntity();
		const bool ok = e.id() == id && e.gen() == 0;
		REQUIRE(ok);
		return e;
	};

	const uint32_t N = 10'000;
	for (uint32_t i = 0; i < N; i++)
		create(i);
}

TEST_CASE("CreateEntity - 1 component") {
	ecs::World w;

	auto create = [&](uint32_t id) {
		auto e = w.CreateEntity();
		w.AddComponent<Int3>(e, {id, id, id});
		const bool ok = e.id() == id && e.gen() == 0;
		REQUIRE(ok);
		auto pos = w.GetComponent<Int3>(e);
		REQUIRE(pos.x == id);
		REQUIRE(pos.y == id);
		REQUIRE(pos.z == id);
		return e;
	};

	const uint32_t N = 10'000;
	for (uint32_t i = 0; i < N; i++)
		create(i);
}

TEST_CASE("CreateAndRemoveEntity - no components") {
	ecs::World w;

	auto create = [&](uint32_t id) {
		auto e = w.CreateEntity();
		const bool ok = e.id() == id && e.gen() == 0;
		REQUIRE(ok);
		return e;
	};
	auto remove = [&](ecs::Entity e) {
		w.DeleteEntity(e);
		auto de = w.GetEntity(e.id());
		const bool ok = de.gen() == e.gen() + 1;
		REQUIRE(ok);
		auto* ch = w.GetChunk(e);
		REQUIRE(ch == nullptr);
	};

	// 100,000 picked so we create enough entites that they overflow
	// into another chunk
	const uint32_t N = 100'000;
	containers::darray<ecs::Entity> arr;
	arr.reserve(N);

	// Create entities
	for (uint32_t i = 0; i < N; i++)
		arr.push_back(create(i));
	// Remove entities
	for (size_t i = 0; i < N; i++)
		remove(arr[i]);
}

TEST_CASE("CreateAndRemoveEntity - 1 component") {
	ecs::World w;

	auto create = [&](uint32_t id) {
		auto e = w.CreateEntity();
		w.AddComponent<Int3>(e, {id, id, id});
		const bool ok = e.id() == id && e.gen() == 0;
		REQUIRE(ok);
		auto pos = w.GetComponent<Int3>(e);
		REQUIRE(pos.x == id);
		REQUIRE(pos.y == id);
		REQUIRE(pos.z == id);
		return e;
	};
	auto remove = [&](ecs::Entity e) {
		w.DeleteEntity(e);
		auto de = w.GetEntity(e.id());
		const bool ok = de.gen() == e.gen() + 1;
		REQUIRE(ok);
		auto* ch = w.GetChunk(e);
		REQUIRE(ch == nullptr);
		const bool isEntityValid = w.IsEntityValid(e);
		REQUIRE(!isEntityValid);
	};

	// 10,000 picked so we create enough entites that they overflow
	// into another chunk
	const uint32_t N = 10'000;
	containers::darray<ecs::Entity> arr;
	arr.reserve(N);

	for (uint32_t i = 0; i < N; i++)
		arr.push_back(create(i));
	for (uint32_t i = 0; i < N; i++)
		remove(arr[i]);
}

TEST_CASE("EnableEntity") {
	ecs::World w;

	auto create = [&](uint32_t id) {
		auto e = w.CreateEntity();
		const bool ok = e.id() == id && e.gen() == 0;
		REQUIRE(ok);
		w.AddComponent<Position>(e, {});
		return e;
	};

	// 100,000 picked so we create enough entites that they overflow into another chunk
	const uint32_t N = 100'000;
	containers::darray<ecs::Entity> arr;
	arr.reserve(N);

	for (uint32_t i = 0; i < N; i++)
		arr.push_back(create(i));

	w.EnableEntity(arr[1000], false);

	ecs::EntityQuery q = w.CreateQuery().All<Position>();
	size_t cnt = q.CalculateEntityCount();
	REQUIRE(cnt == N - 1);

	q.SetConstraints(ecs::EntityQuery::Constraints::AcceptAll);
	cnt = q.CalculateEntityCount();
	REQUIRE(cnt == N);

	q.SetConstraints(ecs::EntityQuery::Constraints::DisabledOnly);
	cnt = q.CalculateEntityCount();
	REQUIRE(cnt == 1);

	w.EnableEntity(arr[1000], true);

	cnt = 0;
	w.ForEach([&]([[maybe_unused]] const Position& p) {
		++cnt;
	});
	REQUIRE(cnt == N);

	cnt = q.CalculateEntityCount();
	REQUIRE(cnt == 0);

	q.SetConstraints(ecs::EntityQuery::Constraints::EnabledOnly);
	cnt = q.CalculateEntityCount();
	REQUIRE(cnt == N);

	q.SetConstraints(ecs::EntityQuery::Constraints::AcceptAll);
	cnt = q.CalculateEntityCount();
	REQUIRE(cnt == N);
}

TEST_CASE("AddComponent - generic") {
	ecs::World w;

	{
		auto e = w.CreateEntity();
		w.AddComponent<Position>(e);
		w.AddComponent<Acceleration>(e);

		REQUIRE(w.HasComponent<Position>(e));
		REQUIRE(w.HasComponent<Acceleration>(e));
		REQUIRE_FALSE(w.HasComponent<ecs::AsChunk<Position>>(e));
		REQUIRE_FALSE(w.HasComponent<ecs::AsChunk<Acceleration>>(e));
	}

	{
		auto e = w.CreateEntity();
		w.AddComponent<Position>(e, {1, 2, 3});
		w.AddComponent<Acceleration>(e, {4, 5, 6});

		REQUIRE(w.HasComponent<Position>(e));
		REQUIRE(w.HasComponent<Acceleration>(e));
		REQUIRE_FALSE(w.HasComponent<ecs::AsChunk<Position>>(e));
		REQUIRE_FALSE(w.HasComponent<ecs::AsChunk<Acceleration>>(e));

		auto p = w.GetComponent<Position>(e);
		REQUIRE(p.x == 1);
		REQUIRE(p.y == 2);
		REQUIRE(p.z == 3);

		auto a = w.GetComponent<Acceleration>(e);
		REQUIRE(a.x == 4);
		REQUIRE(a.y == 5);
		REQUIRE(a.z == 6);
	}
}

// TEST_CASE("AddComponent - from query") {
// 	ecs::World w;

// 	gaia::containers::sarray<ecs::Entity, 5> ents;
// 	for (auto& e: ents)
// 		e = w.CreateEntity();

// 	for (size_t i = 0; i < ents.size() - 1; ++i)
// 		w.AddComponent<Position>(ents[i], {(float)i, (float)i, (float)i});

// 	ecs::EntityQuery q;
// 	q.All<Position>();
// 	w.AddComponent<Acceleration>(q, {1.f, 2.f, 3.f});

// 	for (size_t i = 0; i < ents.size() - 1; ++i) {
// 		REQUIRE(w.HasComponent<Position>(ents[i]));
// 		REQUIRE(w.HasComponent<Acceleration>(ents[i]));

// 		Position p;
// 		w.GetComponent<Position>(ents[i], p);
// 		REQUIRE(p.x == (float)i);
// 		REQUIRE(p.y == (float)i);
// 		REQUIRE(p.z == (float)i);

// 		Acceleration a;
// 		w.GetComponent<Acceleration>(ents[i], a);
// 		REQUIRE(a.x == 1.f);
// 		REQUIRE(a.y == 2.f);
// 		REQUIRE(a.z == 3.f);
// 	}

// 	{
// 		REQUIRE_FALSE(w.HasComponent<Position>(ents[4]));
// 		REQUIRE_FALSE(w.HasComponent<Acceleration>(ents[4]));
// 	}
// }

TEST_CASE("AddComponent - chunk") {
	{
		ecs::World w;
		auto e = w.CreateEntity();
		w.AddComponent<ecs::AsChunk<Position>>(e);
		w.AddComponent<ecs::AsChunk<Acceleration>>(e);

		REQUIRE_FALSE(w.HasComponent<Position>(e));
		REQUIRE_FALSE(w.HasComponent<Acceleration>(e));
		REQUIRE(w.HasComponent<ecs::AsChunk<Position>>(e));
		REQUIRE(w.HasComponent<ecs::AsChunk<Acceleration>>(e));
	}

	{
		ecs::World w;
		auto e = w.CreateEntity();

		// Add Position chunk component
		w.AddComponent<ecs::AsChunk<Position>>(e, {1, 2, 3});
		REQUIRE(w.HasComponent<ecs::AsChunk<Position>>(e));
		REQUIRE_FALSE(w.HasComponent<Position>(e));
		{
			auto p = w.GetComponent<ecs::AsChunk<Position>>(e);
			REQUIRE(p.x == 1);
			REQUIRE(p.y == 2);
			REQUIRE(p.z == 3);
		}
		// Add Acceleration chunk component.
		// This moves "e" to a new archetype.
		w.AddComponent<ecs::AsChunk<Acceleration>>(e, {4, 5, 6});
		REQUIRE(w.HasComponent<ecs::AsChunk<Position>>(e));
		REQUIRE(w.HasComponent<ecs::AsChunk<Acceleration>>(e));
		REQUIRE_FALSE(w.HasComponent<Position>(e));
		REQUIRE_FALSE(w.HasComponent<Acceleration>(e));
		{
			auto a = w.GetComponent<ecs::AsChunk<Acceleration>>(e);
			REQUIRE(a.x == 4);
			REQUIRE(a.y == 5);
			REQUIRE(a.z == 6);
		}
		{
			// Because "e" was moved to a new archetype nobody ever set the value.
			// Therefore, it is garbage and won't match the original chunk.
			auto p = w.GetComponent<ecs::AsChunk<Position>>(e);
			REQUIRE_FALSE(p.x == 1);
			REQUIRE_FALSE(p.y == 2);
			REQUIRE_FALSE(p.z == 3);
		}
	}
}

TEST_CASE("RemoveComponent - generic") {
	ecs::World w;
	auto e1 = w.CreateEntity();

	{
		w.AddComponent<Position>(e1);
		w.AddComponent<Rotation>(e1);

		{
			w.RemoveComponent<Position>(e1);
			REQUIRE_FALSE(w.HasComponent<Position>(e1));
			REQUIRE(w.HasComponent<Rotation>(e1));
		}
		{
			w.RemoveComponent<Rotation>(e1);
			REQUIRE_FALSE(w.HasComponent<Position>(e1));
			REQUIRE_FALSE(w.HasComponent<Rotation>(e1));
		}
	}

	{
		w.AddComponent<Rotation>(e1);
		w.AddComponent<Position>(e1);
		{
			w.RemoveComponent<Position>(e1);
			REQUIRE_FALSE(w.HasComponent<Position>(e1));
			REQUIRE(w.HasComponent<Rotation>(e1));
		}
		{
			w.RemoveComponent<Rotation>(e1);
			REQUIRE_FALSE(w.HasComponent<Position>(e1));
			REQUIRE_FALSE(w.HasComponent<Rotation>(e1));
		}
	}
}

TEST_CASE("RemoveComponent - chunk") {
	ecs::World w;
	auto e1 = w.CreateEntity();

	{
		w.AddComponent<ecs::AsChunk<Position>>(e1);
		w.AddComponent<ecs::AsChunk<Acceleration>>(e1);
		{
			w.RemoveComponent<ecs::AsChunk<Position>>(e1);
			REQUIRE_FALSE(w.HasComponent<ecs::AsChunk<Position>>(e1));
			REQUIRE(w.HasComponent<ecs::AsChunk<Acceleration>>(e1));
		}
		{
			w.RemoveComponent<ecs::AsChunk<Acceleration>>(e1);
			REQUIRE_FALSE(w.HasComponent<ecs::AsChunk<Position>>(e1));
			REQUIRE_FALSE(w.HasComponent<ecs::AsChunk<Acceleration>>(e1));
		}
	}

	{
		w.AddComponent<ecs::AsChunk<Acceleration>>(e1);
		w.AddComponent<ecs::AsChunk<Position>>(e1);
		{
			w.RemoveComponent<ecs::AsChunk<Position>>(e1);
			REQUIRE_FALSE(w.HasComponent<ecs::AsChunk<Position>>(e1));
			REQUIRE(w.HasComponent<ecs::AsChunk<Acceleration>>(e1));
		}
		{
			w.RemoveComponent<ecs::AsChunk<Acceleration>>(e1);
			REQUIRE_FALSE(w.HasComponent<ecs::AsChunk<Position>>(e1));
			REQUIRE_FALSE(w.HasComponent<ecs::AsChunk<Acceleration>>(e1));
		}
	}
}

TEST_CASE("RemoveComponent - generic, chunk") {
	ecs::World w;
	auto e1 = w.CreateEntity();

	{
		w.AddComponent<Position>(e1);
		w.AddComponent<Acceleration>(e1);
		w.AddComponent<ecs::AsChunk<Position>>(e1);
		w.AddComponent<ecs::AsChunk<Acceleration>>(e1);
		{
			w.RemoveComponent<Position>(e1);
			REQUIRE_FALSE(w.HasComponent<Position>(e1));
			REQUIRE(w.HasComponent<Acceleration>(e1));
			REQUIRE(w.HasComponent<ecs::AsChunk<Position>>(e1));
			REQUIRE(w.HasComponent<ecs::AsChunk<Acceleration>>(e1));
		}
		{
			w.RemoveComponent<Acceleration>(e1);
			REQUIRE_FALSE(w.HasComponent<Position>(e1));
			REQUIRE_FALSE(w.HasComponent<Acceleration>(e1));
			REQUIRE(w.HasComponent<ecs::AsChunk<Position>>(e1));
			REQUIRE(w.HasComponent<ecs::AsChunk<Acceleration>>(e1));
		}
		{
			w.RemoveComponent<ecs::AsChunk<Acceleration>>(e1);
			REQUIRE_FALSE(w.HasComponent<Position>(e1));
			REQUIRE_FALSE(w.HasComponent<Acceleration>(e1));
			REQUIRE(w.HasComponent<ecs::AsChunk<Position>>(e1));
			REQUIRE_FALSE(w.HasComponent<ecs::AsChunk<Acceleration>>(e1));
		}
	}
}

TEST_CASE("Usage 1 - simple query, 0 component") {
	ecs::World w;

	auto e = w.CreateEntity();

	{
		uint32_t cnt = 0;
		w.ForEach([&]([[maybe_unused]] const Acceleration& a) {
			++cnt;
		});
		REQUIRE(cnt == 0);
	}
	{
		uint32_t cnt = 0;
		w.ForEach([&]([[maybe_unused]] const Position& a) {
			++cnt;
		});
		REQUIRE(cnt == 0);
	}

	auto e1 = w.CreateEntity(e);
	auto e2 = w.CreateEntity(e);
	auto e3 = w.CreateEntity(e);

	{
		uint32_t cnt = 0;
		w.ForEach([&]([[maybe_unused]] const Position& a) {
			++cnt;
		});
		REQUIRE(cnt == 0);
	}

	w.DeleteEntity(e1);

	{
		uint32_t cnt = 0;
		w.ForEach([&]([[maybe_unused]] const Position& a) {
			++cnt;
		});
		REQUIRE(cnt == 0);
	}

	w.DeleteEntity(e2);
	w.DeleteEntity(e3);
	w.DeleteEntity(e);

	{
		uint32_t cnt = 0;
		w.ForEach([&]([[maybe_unused]] const Position& a) {
			++cnt;
		});
		REQUIRE(cnt == 0);
	}
}

TEST_CASE("Usage 1 - simple query, 1 component") {
	ecs::World w;

	auto e = w.CreateEntity();
	w.AddComponent<Position>(e);

	{
		uint32_t cnt = 0;
		w.ForEach([&]([[maybe_unused]] const Acceleration& a) {
			++cnt;
		});
		REQUIRE(cnt == 0);
	}
	{
		uint32_t cnt = 0;
		w.ForEach([&]([[maybe_unused]] const Position& a) {
			++cnt;
		});
		REQUIRE(cnt == 1);
	}

	auto e1 = w.CreateEntity(e);
	auto e2 = w.CreateEntity(e);
	auto e3 = w.CreateEntity(e);

	{
		uint32_t cnt = 0;
		w.ForEach([&]([[maybe_unused]] const Position& a) {
			++cnt;
		});
		REQUIRE(cnt == 4);
	}

	w.DeleteEntity(e1);

	{
		uint32_t cnt = 0;
		w.ForEach([&]([[maybe_unused]] const Position& a) {
			++cnt;
		});
		REQUIRE(cnt == 3);
	}

	w.DeleteEntity(e2);
	w.DeleteEntity(e3);
	w.DeleteEntity(e);

	{
		uint32_t cnt = 0;
		w.ForEach([&]([[maybe_unused]] const Position& a) {
			++cnt;
		});
		REQUIRE(cnt == 0);
	}
}

TEST_CASE("Usage 1 - simple query, 1 chunk component") {
	ecs::World w;

	auto e = w.CreateEntity();
	w.AddComponent<ecs::AsChunk<Position>>(e);

	auto q = w.CreateQuery().All<ecs::AsChunk<Position>>();
	auto qq = w.CreateQuery().All<Position>();

	{
		uint32_t cnt = 0;
		w.ForEach([&]([[maybe_unused]] const Position& a) {
			++cnt;
		});
		REQUIRE(cnt == 0);
	}
	{
		uint32_t cnt = 0;
		qq.ForEach([&]() {
			++cnt;
		});
		REQUIRE(cnt == 0);
	}
	{
		uint32_t cnt = 0;
		q.ForEach([&]([[maybe_unused]] const ecs::Chunk& chunk) {
			++cnt;
		});
		REQUIRE(cnt == 1);
	}

	auto e1 = w.CreateEntity(e);
	auto e2 = w.CreateEntity(e);
	auto e3 = w.CreateEntity(e);

	{
		uint32_t cnt = 0;
		q.ForEach([&]([[maybe_unused]] const ecs::Chunk& chunk) {
			++cnt;
		});
		REQUIRE(cnt == 1);
	}

	w.DeleteEntity(e1);

	{
		uint32_t cnt = 0;
		q.ForEach([&]([[maybe_unused]] const ecs::Chunk& chunk) {
			++cnt;
		});
		REQUIRE(cnt == 1);
	}

	w.DeleteEntity(e2);
	w.DeleteEntity(e3);
	w.DeleteEntity(e);

	{
		uint32_t cnt = 0;
		q.ForEach([&]([[maybe_unused]] const ecs::Chunk& chunk) {
			++cnt;
		});
		REQUIRE(cnt == 0);
	}
}

TEST_CASE("Usage 2 - simple query, many components") {
	ecs::World w;

	auto e1 = w.CreateEntity();
	w.AddComponent<Position>(e1, {});
	w.AddComponent<Acceleration>(e1, {});
	w.AddComponent<Else>(e1, {});
	auto e2 = w.CreateEntity();
	w.AddComponent<Rotation>(e2, {});
	w.AddComponent<Scale>(e2, {});
	w.AddComponent<Else>(e2, {});
	auto e3 = w.CreateEntity();
	w.AddComponent<Position>(e3, {});
	w.AddComponent<Acceleration>(e3, {});
	w.AddComponent<Scale>(e3, {});

	{
		uint32_t cnt = 0;
		w.ForEach([&]([[maybe_unused]] const Position& a) {
			++cnt;
		});
		REQUIRE(cnt == 2);
	}
	{
		uint32_t cnt = 0;
		w.ForEach([&]([[maybe_unused]] const Acceleration& a) {
			++cnt;
		});
		REQUIRE(cnt == 2);
	}
	{
		uint32_t cnt = 0;
		w.ForEach([&]([[maybe_unused]] const Rotation& a) {
			++cnt;
		});
		REQUIRE(cnt == 1);
	}
	{
		uint32_t cnt = 0;
		w.ForEach([&]([[maybe_unused]] const Scale& a) {
			++cnt;
		});
		REQUIRE(cnt == 2);
	}
	{
		uint32_t cnt = 0;
		w.ForEach([&]([[maybe_unused]] const Position& p, [[maybe_unused]] const Acceleration& s) {
			++cnt;
		});
		REQUIRE(cnt == 2);
	}
	{
		uint32_t cnt = 0;
		w.ForEach([&]([[maybe_unused]] const Position& p, [[maybe_unused]] const Scale& s) {
			++cnt;
		});
		REQUIRE(cnt == 1);
	}
	{
		ecs::EntityQuery q = w.CreateQuery().Any<Position, Acceleration>();

		uint32_t cnt = 0;
		q.ForEach([&](ecs::Chunk& chunk) {
			++cnt;

			const bool ok1 = chunk.HasComponent<Position>() || chunk.HasComponent<Acceleration>();
			REQUIRE(ok1);
			const bool ok2 = chunk.HasComponent<Acceleration>() || chunk.HasComponent<Position>();
			REQUIRE(ok2);
		});
		REQUIRE(cnt == 2);
	}
	{
		ecs::EntityQuery q = w.CreateQuery().Any<Position, Acceleration>().All<Scale>();

		uint32_t cnt = 0;
		q.ForEach([&](ecs::Chunk& chunk) {
			++cnt;

			REQUIRE(chunk.GetEntityCount() == 1);

			const bool ok1 = chunk.HasComponent<Position>() || chunk.HasComponent<Acceleration>();
			REQUIRE(ok1);
			const bool ok2 = chunk.HasComponent<Acceleration>() || chunk.HasComponent<Position>();
			REQUIRE(ok2);
		});
		REQUIRE(cnt == 1);
	}
	{
		ecs::EntityQuery q = w.CreateQuery().Any<Position, Acceleration>().None<Scale>();

		uint32_t cnt = 0;
		q.ForEach([&](ecs::Chunk& chunk) {
			++cnt;

			REQUIRE(chunk.GetEntityCount() == 1);
		});
		REQUIRE(cnt == 1);
	}
}

TEST_CASE("Usage 2 - simple query, many chunk components") {
	ecs::World w;

	auto e1 = w.CreateEntity();
	w.AddComponent<ecs::AsChunk<Position>>(e1, {});
	w.AddComponent<ecs::AsChunk<Acceleration>>(e1, {});
	w.AddComponent<ecs::AsChunk<Else>>(e1, {});
	auto e2 = w.CreateEntity();
	w.AddComponent<ecs::AsChunk<Rotation>>(e2, {});
	w.AddComponent<ecs::AsChunk<Scale>>(e2, {});
	w.AddComponent<ecs::AsChunk<Else>>(e2, {});
	auto e3 = w.CreateEntity();
	w.AddComponent<ecs::AsChunk<Position>>(e3, {});
	w.AddComponent<ecs::AsChunk<Acceleration>>(e3, {});
	w.AddComponent<ecs::AsChunk<Scale>>(e3, {});

	{
		uint32_t cnt = 0;
		auto q = w.CreateQuery().All<ecs::AsChunk<Position>>();
		q.ForEach([&]([[maybe_unused]] const ecs::Chunk& chunk) {
			++cnt;
		});
		REQUIRE(cnt == 2);
	}
	{
		uint32_t cnt = 0;
		auto q = w.CreateQuery().All<ecs::AsChunk<Acceleration>>();
		q.ForEach([&]([[maybe_unused]] const ecs::Chunk& chunk) {
			++cnt;
		});
		REQUIRE(cnt == 2);
	}
	{
		uint32_t cnt = 0;
		auto q = w.CreateQuery().All<ecs::AsChunk<Rotation>>();
		q.ForEach([&]([[maybe_unused]] const ecs::Chunk& chunk) {
			++cnt;
		});
		REQUIRE(cnt == 1);
	}
	{
		uint32_t cnt = 0;
		auto q = w.CreateQuery().All<ecs::AsChunk<Else>>();
		q.ForEach([&]([[maybe_unused]] const ecs::Chunk& chunk) {
			++cnt;
		});
		REQUIRE(cnt == 2);
	}
	{
		uint32_t cnt = 0;
		auto q = w.CreateQuery().All<ecs::AsChunk<Scale>>();
		q.ForEach([&]([[maybe_unused]] const ecs::Chunk& chunk) {
			++cnt;
		});
		REQUIRE(cnt == 2);
	}
	{
		ecs::EntityQuery q = w.CreateQuery().Any<ecs::AsChunk<Position>, ecs::AsChunk<Acceleration>>();

		uint32_t cnt = 0;
		q.ForEach([&](ecs::Chunk& chunk) {
			++cnt;

			const bool ok1 = chunk.HasComponent<ecs::AsChunk<Position>>() || chunk.HasComponent<ecs::AsChunk<Acceleration>>();
			REQUIRE(ok1);
			const bool ok2 = chunk.HasComponent<ecs::AsChunk<Acceleration>>() || chunk.HasComponent<ecs::AsChunk<Position>>();
			REQUIRE(ok2);
		});
		REQUIRE(cnt == 2);
	}
	{
		ecs::EntityQuery q =
				w.CreateQuery().Any<ecs::AsChunk<Position>, ecs::AsChunk<Acceleration>>().All<ecs::AsChunk<Scale>>();

		uint32_t cnt = 0;
		q.ForEach([&](ecs::Chunk& chunk) {
			++cnt;

			REQUIRE(chunk.GetEntityCount() == 1);

			const bool ok1 = chunk.HasComponent<ecs::AsChunk<Position>>() || chunk.HasComponent<ecs::AsChunk<Acceleration>>();
			REQUIRE(ok1);
			const bool ok2 = chunk.HasComponent<ecs::AsChunk<Acceleration>>() || chunk.HasComponent<ecs::AsChunk<Position>>();
			REQUIRE(ok2);
		});
		REQUIRE(cnt == 1);
	}
	{
		ecs::EntityQuery q =
				w.CreateQuery().Any<ecs::AsChunk<Position>, ecs::AsChunk<Acceleration>>().None<ecs::AsChunk<Scale>>();

		uint32_t cnt = 0;
		q.ForEach([&](ecs::Chunk& chunk) {
			++cnt;

			REQUIRE(chunk.GetEntityCount() == 1);
		});
		REQUIRE(cnt == 1);
	}
}

TEST_CASE("SetComponent - generic") {
	ecs::World w;

	constexpr size_t N = 100;
	containers::darray<ecs::Entity> arr;
	arr.reserve(N);

	for (size_t i = 0; i < N; ++i) {
		arr.push_back(w.CreateEntity());
		w.AddComponent<Rotation>(arr.back(), {});
		w.AddComponent<Scale>(arr.back(), {});
		w.AddComponent<Else>(arr.back(), {});
	}

	// Default values
	for (const auto ent: arr) {
		auto r = w.GetComponent<Rotation>(ent);
		REQUIRE(r.x == 0);
		REQUIRE(r.y == 0);
		REQUIRE(r.z == 0);
		REQUIRE(r.w == 0);

		auto s = w.GetComponent<Scale>(ent);
		REQUIRE(s.x == 0);
		REQUIRE(s.y == 0);
		REQUIRE(s.z == 0);

		auto e = w.GetComponent<Else>(ent);
		REQUIRE(e.value == false);
	}

	// Modify values
	{
		ecs::EntityQuery q = w.CreateQuery().All<Rotation, Scale, Else>();

		q.ForEach([&](ecs::Chunk& chunk) {
			auto rotationView = chunk.ViewRW<Rotation>();
			auto scaleView = chunk.ViewRW<Scale>();
			auto elseView = chunk.ViewRW<Else>();

			for (size_t i = 0; i < chunk.GetEntityCount(); ++i) {
				rotationView[i] = {1, 2, 3, 4};
				scaleView[i] = {11, 22, 33};
				elseView[i] = {true};
			}
		});

		for (const auto ent: arr) {
			auto r = w.GetComponent<Rotation>(ent);
			REQUIRE(r.x == 1);
			REQUIRE(r.y == 2);
			REQUIRE(r.z == 3);
			REQUIRE(r.w == 4);

			auto s = w.GetComponent<Scale>(ent);
			REQUIRE(s.x == 11);
			REQUIRE(s.y == 22);
			REQUIRE(s.z == 33);

			auto e = w.GetComponent<Else>(ent);
			REQUIRE(e.value == true);
		}
	}

	// Add one more component and check if the values are still fine after creating a new archetype
	{
		auto ent = w.CreateEntity(arr[0]);
		w.AddComponent<Position>(ent, {5, 6, 7});

		auto r = w.GetComponent<Rotation>(ent);
		REQUIRE(r.x == 1);
		REQUIRE(r.y == 2);
		REQUIRE(r.z == 3);
		REQUIRE(r.w == 4);

		auto s = w.GetComponent<Scale>(ent);
		REQUIRE(s.x == 11);
		REQUIRE(s.y == 22);
		REQUIRE(s.z == 33);

		auto e = w.GetComponent<Else>(ent);
		REQUIRE(e.value == true);
	}
}

TEST_CASE("SetComponent - generic & chunk") {
	ecs::World w;

	constexpr size_t N = 100;
	containers::darray<ecs::Entity> arr;
	arr.reserve(N);

	for (size_t i = 0; i < N; ++i) {
		arr.push_back(w.CreateEntity());
		w.AddComponent<Rotation>(arr.back(), {});
		w.AddComponent<Scale>(arr.back(), {});
		w.AddComponent<Else>(arr.back(), {});
		w.AddComponent<ecs::AsChunk<Position>>(arr.back(), {});
	}

	// Default values
	for (const auto ent: arr) {
		auto r = w.GetComponent<Rotation>(ent);
		REQUIRE(r.x == 0);
		REQUIRE(r.y == 0);
		REQUIRE(r.z == 0);
		REQUIRE(r.w == 0);

		auto s = w.GetComponent<Scale>(ent);
		REQUIRE(s.x == 0);
		REQUIRE(s.y == 0);
		REQUIRE(s.z == 0);

		auto e = w.GetComponent<Else>(ent);
		REQUIRE(e.value == false);

		auto p = w.GetComponent<ecs::AsChunk<Position>>(ent);
		REQUIRE(p.x == 0);
		REQUIRE(p.y == 0);
		REQUIRE(p.z == 0);
	}

	// Modify values
	{
		ecs::EntityQuery q = w.CreateQuery().All<Rotation, Scale, Else>();

		q.ForEach([&](ecs::Chunk& chunk) {
			auto rotationView = chunk.ViewRW<Rotation>();
			auto scaleView = chunk.ViewRW<Scale>();
			auto elseView = chunk.ViewRW<Else>();

			chunk.SetComponent<ecs::AsChunk<Position>>({111, 222, 333});

			for (size_t i = 0; i < chunk.GetEntityCount(); ++i) {
				rotationView[i] = {1, 2, 3, 4};
				scaleView[i] = {11, 22, 33};
				elseView[i] = {true};
			}
		});

		{
			Position p = w.GetComponent<ecs::AsChunk<Position>>(arr[0]);
			REQUIRE(p.x == 111);
			REQUIRE(p.y == 222);
			REQUIRE(p.z == 333);
		}
		{
			for (const auto ent: arr) {
				auto r = w.GetComponent<Rotation>(ent);
				REQUIRE(r.x == 1);
				REQUIRE(r.y == 2);
				REQUIRE(r.z == 3);
				REQUIRE(r.w == 4);

				auto s = w.GetComponent<Scale>(ent);
				REQUIRE(s.x == 11);
				REQUIRE(s.y == 22);
				REQUIRE(s.z == 33);

				auto e = w.GetComponent<Else>(ent);
				REQUIRE(e.value == true);
			}
		}
		{
			auto p = w.GetComponent<ecs::AsChunk<Position>>(arr[0]);
			REQUIRE(p.x == 111);
			REQUIRE(p.y == 222);
			REQUIRE(p.z == 333);
		}
	}
}

TEST_CASE("Components - non trivial") {

	struct PositionNonTrivial {
		float x, y, z;
		PositionNonTrivial(): x(1), y(2), z(3) {}
		PositionNonTrivial(float xx, float yy, float zz): x(xx), y(yy), z(zz) {}
	};
	struct StringComponent {
		std::string value;
	};
	static constexpr const char* DefaultValue = "test";
	struct StringComponent2 {
		std::string value;
		StringComponent2(): value(DefaultValue) {}
		~StringComponent2() {
			value = "empty";
		}
	};

	ecs::World w;

	constexpr size_t N = 100;
	containers::darray<ecs::Entity> arr;
	arr.reserve(N);

	for (size_t i = 0; i < N; ++i) {
		arr.push_back(w.CreateEntity());
		w.AddComponent<StringComponent>(arr.back(), {});
		w.AddComponent<StringComponent2>(arr.back(), {});
		w.AddComponent<PositionNonTrivial>(arr.back(), {});
	}

	// Default values
	for (const auto ent: arr) {
		const auto& s1 = w.GetComponent<StringComponent>(ent);
		REQUIRE(s1.value.empty());

		{
			auto s2 = w.GetComponent<StringComponent2>(ent);
			REQUIRE(s2.value == DefaultValue);
		}
		{
			const auto& s2 = w.GetComponent<StringComponent2>(ent);
			REQUIRE(s2.value == DefaultValue);
		}
		{
			const auto& s2 = w.GetComponent<StringComponent2>(ent);
			REQUIRE(s2.value == DefaultValue);
		}

		const auto& p = w.GetComponent<PositionNonTrivial>(ent);
		REQUIRE(p.x == 1);
		REQUIRE(p.y == 2);
		REQUIRE(p.z == 3);
	}

	// Modify values
	{
		ecs::EntityQuery q = w.CreateQuery().All<StringComponent, StringComponent2, PositionNonTrivial>();

		q.ForEach([&](ecs::Chunk& chunk) {
			auto strView = chunk.ViewRW<StringComponent>();
			auto str2View = chunk.ViewRW<StringComponent2>();
			auto posView = chunk.ViewRW<PositionNonTrivial>();

			for (size_t i = 0; i < chunk.GetEntityCount(); ++i) {
				strView[i] = {"string_component"};
				str2View[i].value = "another_text";
				posView[i] = {111, 222, 333};
			}
		});

		for (const auto ent: arr) {
			const auto& s1 = w.GetComponent<StringComponent>(ent);
			REQUIRE(s1.value == "string_component");

			const auto& s2 = w.GetComponent<StringComponent2>(ent);
			REQUIRE(s2.value == "another_text");

			const auto& p = w.GetComponent<PositionNonTrivial>(ent);
			REQUIRE(p.x == 111);
			REQUIRE(p.y == 222);
			REQUIRE(p.z == 333);
		}
	}

	// Add one more component and check if the values are still fine after creating a new archetype
	{
		auto ent = w.CreateEntity(arr[0]);
		w.AddComponent<Position>(ent, {5, 6, 7});

		const auto& s1 = w.GetComponent<StringComponent>(ent);
		REQUIRE(s1.value == "string_component");

		const auto& s2 = w.GetComponent<StringComponent2>(ent);
		REQUIRE(s2.value == "another_text");

		const auto& p = w.GetComponent<PositionNonTrivial>(ent);
		REQUIRE(p.x == 111);
		REQUIRE(p.y == 222);
		REQUIRE(p.z == 333);
	}
}

TEST_CASE("CommandBuffer") {
	// Entity creation
	{
		ecs::World w;
		ecs::CommandBuffer cb(w);

		const uint32_t N = 100;
		for (uint32_t i = 0; i < N; i++) {
			[[maybe_unused]] auto tmp = cb.CreateEntity();
		}

		cb.Commit();

		for (uint32_t i = 0; i < N; i++) {
			auto e = w.GetEntity(i);
			REQUIRE(e.id() == i);
		}
	}

	// Entity creation from another entity
	{
		ecs::World w;
		ecs::CommandBuffer cb(w);

		auto mainEntity = w.CreateEntity();

		const uint32_t N = 100;
		for (uint32_t i = 0; i < N; i++) {
			[[maybe_unused]] auto tmp = cb.CreateEntity(mainEntity);
		}

		cb.Commit();

		for (uint32_t i = 0; i < N; i++) {
			auto e = w.GetEntity(i + 1);
			REQUIRE(e.id() == i + 1);
		}
	}

	// Entity creation from another entity with a component
	{
		ecs::World w;
		ecs::CommandBuffer cb(w);

		auto mainEntity = w.CreateEntity();
		w.AddComponent<Position>(mainEntity, {1, 2, 3});

		[[maybe_unused]] auto tmp = cb.CreateEntity(mainEntity);
		cb.Commit();
		auto e = w.GetEntity(1);
		REQUIRE(w.HasComponent<Position>(e));
		auto p = w.GetComponent<Position>(e);
		REQUIRE(p.x == 1);
		REQUIRE(p.y == 2);
		REQUIRE(p.z == 3);
	}

	// Delayed component addition to an existing entity
	{
		ecs::World w;
		ecs::CommandBuffer cb(w);

		auto e = w.CreateEntity();
		cb.AddComponent<Position>(e);
		REQUIRE(!w.HasComponent<Position>(e));
		cb.Commit();
		REQUIRE(w.HasComponent<Position>(e));
	}

	// Delayed component addition to a to-be-created entity
	{
		ecs::World w;
		ecs::CommandBuffer cb(w);

		auto tmp = cb.CreateEntity();
		REQUIRE(!w.GetEntityCount());
		cb.AddComponent<Position>(tmp);
		cb.Commit();

		auto e = w.GetEntity(0);
		REQUIRE(w.HasComponent<Position>(e));
	}

	// Delayed component setting of an existing entity
	{
		ecs::World w;
		ecs::CommandBuffer cb(w);

		auto e = w.CreateEntity();

		cb.AddComponent<Position>(e);
		cb.SetComponent<Position>(e, {1, 2, 3});
		REQUIRE(!w.HasComponent<Position>(e));

		cb.Commit();
		REQUIRE(w.HasComponent<Position>(e));

		auto p = w.GetComponent<Position>(e);
		REQUIRE(p.x == 1);
		REQUIRE(p.y == 2);
		REQUIRE(p.z == 3);
	}

	// Delayed 2 components setting of an existing entity
	{
		ecs::World w;
		ecs::CommandBuffer cb(w);

		auto e = w.CreateEntity();

		cb.AddComponent<Position>(e);
		cb.SetComponent<Position>(e, {1, 2, 3});
		cb.AddComponent<Acceleration>(e);
		cb.SetComponent<Acceleration>(e, {4, 5, 6});
		REQUIRE_FALSE(w.HasComponent<Position>(e));
		REQUIRE_FALSE(w.HasComponent<Acceleration>(e));

		cb.Commit();
		REQUIRE(w.HasComponent<Position>(e));
		REQUIRE(w.HasComponent<Acceleration>(e));

		auto p = w.GetComponent<Position>(e);
		REQUIRE(p.x == 1);
		REQUIRE(p.y == 2);
		REQUIRE(p.z == 3);

		auto a = w.GetComponent<Acceleration>(e);
		REQUIRE(a.x == 4);
		REQUIRE(a.y == 5);
		REQUIRE(a.z == 6);
	}

	// Delayed component setting of a to-be-created entity
	{
		ecs::World w;
		ecs::CommandBuffer cb(w);

		auto tmp = cb.CreateEntity();
		REQUIRE(!w.GetEntityCount());

		cb.AddComponent<Position>(tmp);
		cb.SetComponent<Position>(tmp, {1, 2, 3});
		cb.Commit();

		auto e = w.GetEntity(0);
		REQUIRE(w.HasComponent<Position>(e));

		auto p = w.GetComponent<Position>(e);
		REQUIRE(p.x == 1);
		REQUIRE(p.y == 2);
		REQUIRE(p.z == 3);
	}

	// Delayed 2 components setting of a to-be-created entity
	{
		ecs::World w;
		ecs::CommandBuffer cb(w);

		auto tmp = cb.CreateEntity();
		REQUIRE(!w.GetEntityCount());

		cb.AddComponent<Position>(tmp);
		cb.AddComponent<Acceleration>(tmp);
		cb.SetComponent<Position>(tmp, {1, 2, 3});
		cb.SetComponent<Acceleration>(tmp, {4, 5, 6});
		cb.Commit();

		auto e = w.GetEntity(0);
		REQUIRE(w.HasComponent<Position>(e));
		REQUIRE(w.HasComponent<Acceleration>(e));

		auto p = w.GetComponent<Position>(e);
		REQUIRE(p.x == 1);
		REQUIRE(p.y == 2);
		REQUIRE(p.z == 3);

		auto a = w.GetComponent<Acceleration>(e);
		REQUIRE(a.x == 4);
		REQUIRE(a.y == 5);
		REQUIRE(a.z == 6);
	}

	// Delayed component add with setting of a to-be-created entity
	{
		ecs::World w;
		ecs::CommandBuffer cb(w);

		auto tmp = cb.CreateEntity();
		REQUIRE(!w.GetEntityCount());

		cb.AddComponent<Position>(tmp, {1, 2, 3});
		cb.Commit();

		auto e = w.GetEntity(0);
		REQUIRE(w.HasComponent<Position>(e));

		auto p = w.GetComponent<Position>(e);
		REQUIRE(p.x == 1);
		REQUIRE(p.y == 2);
		REQUIRE(p.z == 3);
	}

	// Delayed 2 components add with setting of a to-be-created entity
	{
		ecs::World w;
		ecs::CommandBuffer cb(w);

		auto tmp = cb.CreateEntity();
		REQUIRE(!w.GetEntityCount());

		cb.AddComponent<Position>(tmp, {1, 2, 3});
		cb.AddComponent<Acceleration>(tmp, {4, 5, 6});
		cb.Commit();

		auto e = w.GetEntity(0);
		REQUIRE(w.HasComponent<Position>(e));
		REQUIRE(w.HasComponent<Acceleration>(e));

		auto p = w.GetComponent<Position>(e);
		REQUIRE(p.x == 1);
		REQUIRE(p.y == 2);
		REQUIRE(p.z == 3);

		auto a = w.GetComponent<Acceleration>(e);
		REQUIRE(a.x == 4);
		REQUIRE(a.y == 5);
		REQUIRE(a.z == 6);
	}

	// Delayed component removal from an existing entity
	{
		ecs::World w;
		ecs::CommandBuffer cb(w);

		auto e = w.CreateEntity();
		w.AddComponent<Position>(e, {1, 2, 3});

		cb.RemoveComponent<Position>(e);
		REQUIRE(w.HasComponent<Position>(e));
		{
			auto p = w.GetComponent<Position>(e);
			REQUIRE(p.x == 1);
			REQUIRE(p.y == 2);
			REQUIRE(p.z == 3);
		}

		cb.Commit();
		REQUIRE_FALSE(w.HasComponent<Position>(e));
	}

	// Delayed 2 component removal from an existing entity
	{
		ecs::World w;
		ecs::CommandBuffer cb(w);

		auto e = w.CreateEntity();
		w.AddComponent<Position>(e, {1, 2, 3});
		w.AddComponent<Acceleration>(e, {4, 5, 6});

		cb.RemoveComponent<Position>(e);
		cb.RemoveComponent<Acceleration>(e);
		REQUIRE(w.HasComponent<Position>(e));
		REQUIRE(w.HasComponent<Acceleration>(e));
		{
			auto p = w.GetComponent<Position>(e);
			REQUIRE(p.x == 1);
			REQUIRE(p.y == 2);
			REQUIRE(p.z == 3);

			auto a = w.GetComponent<Acceleration>(e);
			REQUIRE(a.x == 4);
			REQUIRE(a.y == 5);
			REQUIRE(a.z == 6);
		}

		cb.Commit();
		REQUIRE_FALSE(w.HasComponent<Position>(e));
		REQUIRE_FALSE(w.HasComponent<Acceleration>(e));
	}
}

TEST_CASE("Query Filter - no systems") {
	ecs::World w;
	ecs::EntityQuery q = w.CreateQuery().All<const Position>().WithChanged<Position>();

	auto e = w.CreateEntity();
	w.AddComponent<Position>(e);

	// System-less filters
	{
		uint32_t cnt = 0;
		q.ForEach([&]([[maybe_unused]] const Position& a) {
			++cnt;
		});
		REQUIRE(cnt == 1); // first run always happens
	}
	{
		uint32_t cnt = 0;
		q.ForEach([&]([[maybe_unused]] const Position& a) {
			++cnt;
		});
		REQUIRE(cnt == 0); // no change of position so this shouldn't run
	}
	{ w.SetComponent<Position>(e, {}); }
	{
		uint32_t cnt = 0;
		q.ForEach([&]([[maybe_unused]] const Position& a) {
			++cnt;
		});
		REQUIRE(cnt == 1);
	}
	{
		uint32_t cnt = 0;
		q.ForEach([&]([[maybe_unused]] const Position& a) {
			++cnt;
		});
		REQUIRE(cnt == 0);
	}
	{ w.SetComponentSilent<Position>(e, {}); }
	{
		uint32_t cnt = 0;
		q.ForEach([&]([[maybe_unused]] const Position& a) {
			++cnt;
		});
		REQUIRE(cnt == 0);
	}
	{
		auto* ch = w.GetChunk(e);
		auto p = ch->ViewRWSilent<Position>();
		p[0] = {};
	}
	{
		uint32_t cnt = 0;
		q.ForEach([&]([[maybe_unused]] const Position& a) {
			++cnt;
		});
		REQUIRE(cnt == 0);
	}
}

TEST_CASE("Query Filter - systems") {
	ecs::World w;

	auto e = w.CreateEntity();
	w.AddComponent<Position>(e);

	class WriterSystem final: public ecs::System {
	public:
		void OnUpdate() override {
			GetWorld().ForEach([]([[maybe_unused]] Position& a) {});
		}
	};
	class WriterSystemSilent final: public ecs::System {
		ecs::EntityQuery m_q;

	public:
		void OnCreated() override {
			m_q = GetWorld().CreateQuery().All<Position>();
		}
		void OnUpdate() override {
			m_q.ForEach([&](ecs::Chunk& chunk) {
				auto posRWView = chunk.ViewRWSilent<Position>();
				(void)posRWView;
			});
		}
	};
	class ReaderSystem final: public ecs::System {
		uint32_t m_expectedCnt = 0;

		ecs::EntityQuery m_q;

	public:
		void SetExpectedCount(uint32_t cnt) {
			m_expectedCnt = cnt;
		}
		void OnCreated() override {
			m_q = GetWorld().CreateQuery().All<const Position>().WithChanged<Position>();
		}
		void OnUpdate() override {
			uint32_t cnt = 0;
			m_q.ForEach([&]([[maybe_unused]] const Position& a) {
				++cnt;
			});
			REQUIRE(cnt == m_expectedCnt);
		}
	};
	ecs::SystemManager sm(w);
	auto* ws = sm.CreateSystem<WriterSystem>();
	auto* wss = sm.CreateSystem<WriterSystemSilent>();
	auto* rs = sm.CreateSystem<ReaderSystem>();

	// first run always happens
	ws->Enable(false);
	wss->Enable(false);
	rs->SetExpectedCount(1);
	sm.Update();
	// no change of position so ReaderSystem should't see changes
	rs->SetExpectedCount(0);
	sm.Update();
	// update position so ReaderSystem should detect a change
	ws->Enable(true);
	rs->SetExpectedCount(1);
	sm.Update();
	// no change of position so ReaderSystem shouldn't see changes
	ws->Enable(false);
	rs->SetExpectedCount(0);
	sm.Update();
	// silent writer enabled again. If should not cause an update
	ws->Enable(false);
	wss->Enable(true);
	rs->SetExpectedCount(0);
	sm.Update();
}

template <size_t N, typename T>
void TestDataLayoutSoA() {
	GAIA_ALIGNAS(N * 4) containers::sarray<T, N> data{};
	const auto* arr = (const float*)data.data();

	using soa = gaia::utils::soa_view_policy<T>;
	using view_deduced = gaia::utils::auto_view_policy<T>;

	for (size_t i = 0; i < N; ++i) {
		const auto f = (float)i;
		soa::set({data}, i, {f, f, f});
		REQUIRE(arr[i + N * 0] == f);
		REQUIRE(arr[i + N * 1] == f);
		REQUIRE(arr[i + N * 2] == f);

		auto val = soa::get({data}, i);
		REQUIRE(val.x == f);
		REQUIRE(val.y == f);
		REQUIRE(val.z == f);
	}

	for (size_t i = 0; i < N; ++i) {
		const auto f = (float)i;
		view_deduced::set({data}, i, {f, f, f});
		REQUIRE(arr[i + N * 0] == f);
		REQUIRE(arr[i + N * 1] == f);
		REQUIRE(arr[i + N * 2] == f);

		auto val = view_deduced::get({data}, i);
		REQUIRE(val.x == f);
		REQUIRE(val.y == f);
		REQUIRE(val.z == f);
	}
}

TEST_CASE("DataLayout SoA") {
	TestDataLayoutSoA<4, PositionSoA>();
}

TEST_CASE("DataLayout SoA8") {
	TestDataLayoutSoA<8, PositionSoA8>();
}

TEST_CASE("DataLayout SoA16") {
	TestDataLayoutSoA<16, PositionSoA16>();
}

template <typename T>
void TestDataLayoutSoA_ECS() {
	ecs::World w;

	auto create = [&]() {
		auto e = w.CreateEntity();
		w.AddComponent<T>(e, {});
	};

	const uint32_t N = 10'000;
	for (uint32_t i = 0; i < N; i++)
		create();

	ecs::EntityQuery q = w.CreateQuery().All<T>();
	uint32_t j = 0;
	q.ForEach([&](ecs::Chunk& ch) {
		auto t = ch.ViewRW<T>();
		auto tx = t.template set<0>();
		auto ty = t.template set<1>();
		auto tz = t.template set<2>();
		for (uint32_t i = 0; i < ch.GetEntityCount(); ++i, ++j) {
			auto f = (float)j;
			tx[i] = f;
			ty[i] = f;
			tz[i] = f;

			REQUIRE(tx[i] == f);
			REQUIRE(ty[i] == f);
			REQUIRE(tz[i] == f);
		}
	});
}

TEST_CASE("DataLayout SoA - ECS") {
	TestDataLayoutSoA_ECS<PositionSoA>();
}

TEST_CASE("DataLayout SoA8 - ECS") {
	TestDataLayoutSoA_ECS<PositionSoA8>();
}

TEST_CASE("DataLayout SoA16 - ECS") {
	TestDataLayoutSoA_ECS<PositionSoA16>();
}

TEST_CASE("DataLayout AoS") {
	constexpr size_t N = 4U;
	containers::sarray<Position, N> data{};
	const auto* arr = (const float*)data.data();

	using aos = gaia::utils::aos_view_policy<Position>;
	using view_deduced = gaia::utils::auto_view_policy<Position>;

	for (size_t i = 0; i < N; ++i) {
		const auto f = (float)i;
		aos::set({data}, i, {f, f, f});
		REQUIRE(arr[i * 3 + 0] == f);
		REQUIRE(arr[i * 3 + 1] == f);
		REQUIRE(arr[i * 3 + 2] == f);

		auto val = aos::getc({data}, i);
		REQUIRE(val.x == f);
		REQUIRE(val.y == f);
		REQUIRE(val.z == f);
	}

	for (size_t i = 0; i < N; ++i) {
		const auto f = (float)i;
		view_deduced::set({data}, i, {f, f, f});
		REQUIRE(arr[i * 3 + 0] == f);
		REQUIRE(arr[i * 3 + 1] == f);
		REQUIRE(arr[i * 3 + 2] == f);

		auto val = view_deduced::getc({data}, i);
		REQUIRE(val.x == f);
		REQUIRE(val.y == f);
		REQUIRE(val.z == f);
	}
}