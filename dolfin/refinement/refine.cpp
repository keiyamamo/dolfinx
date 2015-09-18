// Copyright (C) 2010 Garth N. Wells
//
// This file is part of DOLFIN.
//
// DOLFIN is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// DOLFIN is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with DOLFIN. If not, see <http://www.gnu.org/licenses/>.
//
// Modified by Anders Logg, 2010-2011.
//
// First added:  2010-02-10
// Last changed: 2013-01-13

#include <dolfin/parameter/GlobalParameters.h>
#include <dolfin/mesh/Mesh.h>
#include <dolfin/mesh/MeshHierarchy.h>
#include <dolfin/mesh/MeshFunction.h>
#include <dolfin/mesh/MeshEditor.h>
#include <dolfin/mesh/Cell.h>
#include <dolfin/mesh/Edge.h>
#include <dolfin/mesh/Vertex.h>
#include "UniformMeshRefinement.h"
#include "LocalMeshRefinement.h"
#include "PlazaRefinementND.h"
#include "refine.h"

using namespace dolfin;

//-----------------------------------------------------------------------------
dolfin::Mesh dolfin::refine(const Mesh& mesh, bool redistribute)
{
  Mesh refined_mesh;
  refine(refined_mesh, mesh, redistribute);
  return refined_mesh;
}
//-----------------------------------------------------------------------------
std::shared_ptr<const MeshHierarchy> dolfin::refine(
                    const MeshHierarchy& hierarchy,
                    const MeshFunction<bool>& markers)
{
  return hierarchy.refine(markers);
}
//-----------------------------------------------------------------------------
void dolfin::refine(Mesh& refined_mesh, const Mesh& mesh, bool redistribute)
{
  // Topological dimension
  const std::size_t D = mesh.topology().dim();

  const std::string refinement_algorithm = parameters["refinement_algorithm"];
  bool parent_facets = (refinement_algorithm == "plaza_with_parent_facets");
  bool serial = (MPI::size(mesh.mpi_comm()) == 1);

  // Dispatch to appropriate refinement function
  if (serial and (D == 1 or refinement_algorithm == "regular_cut"))
    UniformMeshRefinement::refine(refined_mesh, mesh);
  else if(D == 2 or D == 3)
  {
    if (refinement_algorithm == "regular_cut")
      warning("Using Plaza algorithm in parallel");
    PlazaRefinementND::refine(refined_mesh, mesh, redistribute, parent_facets);
  }
  else
  {
    dolfin_error("refine.cpp",
                 "refine mesh",
                 "Cannot refine mesh of topological dimension %d in parallel. Only 2D and 3D supported", D);
  }
}
//-----------------------------------------------------------------------------
dolfin::Mesh dolfin::refine(const Mesh& mesh,
                            const MeshFunction<bool>& cell_markers,
                            bool redistribute)
{
  Mesh refined_mesh;
  refine(refined_mesh, mesh, cell_markers, redistribute);
  return refined_mesh;
}
//-----------------------------------------------------------------------------
void dolfin::refine(Mesh& refined_mesh, const Mesh& mesh,
                    const MeshFunction<bool>& cell_markers, bool redistribute)
{
  // Topological dimension
  const std::size_t D = mesh.topology().dim();

  const std::string refinement_algorithm = parameters["refinement_algorithm"];
  bool parent_facets = (refinement_algorithm == "plaza_with_parent_facets");
  bool serial = (MPI::size(mesh.mpi_comm()) == 1);

  // Dispatch to appropriate refinement function
  if (serial and (D == 1 or refinement_algorithm == "regular_cut"))
    LocalMeshRefinement::refine(refined_mesh, mesh, cell_markers);
  else if (D == 2 or D == 3)
  {
    if (refinement_algorithm == "regular_cut")
      warning("Using Plaza algorithm in parallel");

    PlazaRefinementND::refine(refined_mesh, mesh, cell_markers,
                              redistribute, parent_facets);
  }
  else
  {
    dolfin_error("refine.cpp",
                 "refine mesh",
                 "Cannot refine mesh of topological dimension %d in parallel. Only 2D and 3D supported", D);
  }
}
//-----------------------------------------------------------------------------
void dolfin::p_refine(Mesh& refined_mesh, const Mesh& mesh)
{
  MeshEditor editor;
  if (mesh.geometry().degree() != 1)
  {
    dolfin_error("refine.cpp",
                 "increase polynomial degree of mesh",
                 "Currently only linear -> quadratic is supported");
  }

  if (mesh.type().cell_type() != CellType::triangle
      and mesh.type().cell_type() != CellType::tetrahedron
      and mesh.type().cell_type() != CellType::interval)
  {
    dolfin_error("refine.cpp",
                 "increase polynomial degree of mesh",
                 "Unsupported cell type");
  }

  const std::size_t tdim = mesh.topology().dim();
  const std::size_t gdim = mesh.geometry().dim();

  editor.open(refined_mesh, tdim, gdim, 2);

  // Copy over mesh
  editor.init_vertices_global(mesh.size(0), mesh.size_global(0));
  for (VertexIterator v(mesh); !v.end(); ++v)
    editor.add_vertex(v->index(), v->point());

  editor.init_cells_global(mesh.size(tdim), mesh.size_global(tdim));
  std::vector<std::size_t> verts(tdim + 1);
  for (CellIterator c(mesh); !c.end(); ++c)
  {
    std::copy(c->entities(0), c->entities(0) + tdim + 1, verts.begin());
    editor.add_cell(c->index(), verts);
  }

  // Initialise edges
  editor.init_entities();

  // Add points at centres of edges
  for (EdgeIterator e(refined_mesh); !e.end(); ++e)
    editor.add_entity_point(1, 0, e->index(), e->midpoint());

  editor.close();
}
//-----------------------------------------------------------------------------
