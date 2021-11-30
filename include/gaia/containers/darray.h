#pragma once

#define USE_VECTOR GAIA_USE_STL_CONTAINERS

#if USE_VECTOR == 1

	#include <vector>

namespace gaia {
	namespace containers {
		template <typename T>
		using darray = std::vector<T>;
	} // namespace containers
} // namespace gaia
#elif USE_VECTOR == 0

	#include "impl/darray.h"

namespace gaia {
	namespace containers {
		template <typename T>
		using darray = containers::darr<T>;
	}
} // namespace gaia
#else

	#error Unsupported value used for USE_VECTOR

#endif
