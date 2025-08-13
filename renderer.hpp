#pragma once

#include <stdexcept>
#include <atomic>
#include <memory>

#include "engine.hpp"
#include "geometry.hpp"

namespace okami {
	struct DummyTriangleComponent {};

	struct Mesh {
		VertexFormat m_format;

		using CreationData = InitMesh;
	};

	struct StaticMeshComponent {
		ResHandle<Mesh> m_mesh;
	};
}
