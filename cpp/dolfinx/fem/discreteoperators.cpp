// Copyright (C) 2015-2021 Garth N. Wells, Jørgen S. Dokken
//
// This file is part of DOLFINX (https://www.fenicsproject.org)
//
// SPDX-License-Identifier:    LGPL-3.0-or-later

#include "discreteoperators.h"

using namespace dolfinx;

//-----------------------------------------------------------------------------
la::SparsityPattern
fem::create_sparsity_discrete_gradient(const fem::FunctionSpace& V0,
                                       const fem::FunctionSpace& V1)
{
  // Get mesh
  std::shared_ptr<const mesh::Mesh> mesh = V0.mesh();
  assert(mesh);

  // Check that mesh is the same for both function spaces
  assert(V1.mesh());
  if (mesh != V1.mesh())
  {
    throw std::runtime_error("Compute discrete gradient operator. Function "
                             "spaces do not share the same mesh");
  }

  // Check that V0 is a (lowest-order) edge basis
  mesh->topology_mutable().create_entities(1);
  std::int64_t num_edges_global = mesh->topology().index_map(1)->size_global();
  const std::int64_t V0dim
      = V0.dofmap()->index_map->size_global() * V0.dofmap()->index_map_bs();
  if (V0dim != num_edges_global)
  {
    throw std::runtime_error(
        "Cannot compute discrete gradient operator. Function "
        "spaces is not a lowest-order edge space");
  }

  // Check that V1 is a linear nodal basis
  const std::int64_t num_vertices_global
      = mesh->topology().index_map(0)->size_global();
  const std::int64_t V1dim
      = V1.dofmap()->index_map->size_global() * V1.dofmap()->index_map_bs();
  if (V1dim != num_vertices_global)
  {
    throw std::runtime_error(
        "Cannot compute discrete gradient operator. Function "
        "space is not a linear nodal function space");
  }

  // Build maps from entities to local dof indices
  std::shared_ptr<const dolfinx::fem::ElementDofLayout> layout0
      = V0.dofmap()->element_dof_layout;
  std::shared_ptr<const dolfinx::fem::ElementDofLayout> layout1
      = V1.dofmap()->element_dof_layout;

  // Copy index maps from dofmaps
  std::array<std::shared_ptr<const common::IndexMap>, 2> index_maps
      = {{V0.dofmap()->index_map, V1.dofmap()->index_map}};
  std::array<int, 2> block_sizes
      = {V0.dofmap()->index_map_bs(), V1.dofmap()->index_map_bs()};
  std::vector<std::array<std::int64_t, 2>> local_range
      = {index_maps[0]->local_range(), index_maps[1]->local_range()};
  assert(block_sizes[0] == block_sizes[1]);

  // Initialise sparsity pattern
  la::SparsityPattern pattern(mesh->mpi_comm(), index_maps, block_sizes);

  // Initialize required connectivities
  const int tdim = mesh->topology().dim();
  mesh->topology_mutable().create_connectivity(1, 0);
  auto e_to_v = mesh->topology().connectivity(1, 0);
  mesh->topology_mutable().create_connectivity(tdim, 1);
  auto c_to_e = mesh->topology().connectivity(tdim, 1);
  mesh->topology_mutable().create_connectivity(1, tdim);
  auto e_to_c = mesh->topology().connectivity(1, tdim);
  mesh->topology_mutable().create_connectivity(tdim, 0);
  auto c_to_v = mesh->topology().connectivity(tdim, 0);

  // Build sparsity pattern
  const std::int32_t num_edges = mesh->topology().index_map(1)->size_local()
                                 + mesh->topology().index_map(1)->num_ghosts();
  const std::shared_ptr<const fem::DofMap> dofmap0 = V0.dofmap();
  assert(dofmap0);
  // Create local lookup table for local edge to cell dofs
  const int num_edges_per_cell
      = mesh::cell_num_entities(mesh->topology().cell_type(), 1);
  std::map<std::int32_t, std::vector<std::int32_t>> local_edge_dofs;
  for (std::int32_t i = 0; i < num_edges_per_cell; ++i)
    local_edge_dofs[i] = layout0->entity_dofs(1, i);
  // Create local lookup table for local vertex to cell dofs
  const int num_vertices_per_cell
      = mesh::cell_num_entities(mesh->topology().cell_type(), 0);
  std::map<std::int32_t, std::vector<std::int32_t>> local_vertex_dofs;
  for (std::int32_t i = 0; i < num_vertices_per_cell; ++i)
    local_vertex_dofs[i] = layout1->entity_dofs(0, i);

  std::array<std::int32_t, 2> cols;
  for (std::int32_t e = 0; e < num_edges; ++e)
  {
    // Find local index of edge in one of the cells it is part of
    xtl::span<const std::int32_t> cells = e_to_c->links(e);
    assert(cells.size() > 0);
    const std::int32_t cell = cells[0];
    xtl::span<const std::int32_t> edges = c_to_e->links(cell);
    const auto it = std::find(edges.begin(), edges.end(), e);
    assert(it != edges.end());
    const int local_edge = std::distance(edges.begin(), it);

    // Find the dofs located on the edge
    xtl::span<const std::int32_t> dofs0 = dofmap0->cell_dofs(cell);
    std::vector<std::int32_t>& local_dofs = local_edge_dofs[local_edge];
    assert(local_dofs.size() == 1);
    const std::int32_t row = dofs0[local_dofs[0]];
    xtl::span<const std::int32_t> vertices = e_to_v->links(e);
    assert(vertices.size() == 2);
    auto cell_vertices = c_to_v->links(cell);

    // Find local index of each of the vertices and map to local dof
    xtl::span<const std::int32_t> dofs1 = V1.dofmap()->cell_dofs(cell);
    for (std::int32_t i = 0; i < 2; ++i)
    {
      const auto it
          = std::find(cell_vertices.begin(), cell_vertices.end(), vertices[i]);
      assert(it != cell_vertices.end());
      const int local_vertex = std::distance(cell_vertices.begin(), it);
      std::vector<std::int32_t>& local_v_dofs = local_vertex_dofs[local_vertex];

      assert(local_v_dofs.size() == 1);
      cols[i] = dofs1[local_v_dofs[0]];
    }

    pattern.insert(xtl::span<const std::int32_t>(&row, 1), tcb::make_span(cols));
  }
  pattern.assemble();
  return pattern;
}
