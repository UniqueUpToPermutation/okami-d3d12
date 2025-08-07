#pragma once

#include <stdexcept>
#include <atomic>
#include <memory>

#include "engine.hpp"
#include "geometry.hpp"

namespace okami {
	struct DummyTriangleComponent {};

	struct Mesh {
		using CreationData = Geometry;
	};

	using MeshHandle = ResHandle<Mesh>;

	struct StaticMeshComponent {
		MeshHandle m_mesh;
	};
}
