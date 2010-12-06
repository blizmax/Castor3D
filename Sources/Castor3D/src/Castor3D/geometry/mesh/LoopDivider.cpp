#include "PrecompiledHeader.h"

#include "geometry/Module_Geometry.h"

#include "geometry/mesh/LoopDivider.h"
#include "geometry/mesh/Submesh.h"
#include "geometry/basic/Vertex.h"
#include "geometry/basic/Face.h"
#include "geometry/basic/SmoothingGroup.h"

using namespace Castor3D;

//*********************************************************************************************

#define ALPHA_MAX 20
#define ALPHA_LIMIT 0.469 /* converges to ~ 0.469 */

real ALPHA[] = {	1.13333f, -0.358974f, -0.333333f, 0.129032f, 0.945783f, 2.0f,
					3.19889f, 4.47885f, 5.79946f, 7.13634f, 8.47535f, 9.80865f,
					11.1322f, 12.4441f, 13.7439f, 15.0317f, 16.3082f, 17.574f,
					18.83f, 20.0769f };

#define BETA_MAX 20
#define BETA_LIMIT 0.469 /* converges to ~ 0.469 */

real BETA[] = {0.46875f, 1.21875f, 1.125f, 0.96875f, 0.840932f, 0.75f, 0.686349f,
				0.641085f, 0.60813f, 0.583555f, 0.564816f, 0.55024f, 0.5387f,
				0.529419f, 0.52185f, 0.515601f, 0.510385f, 0.505987f, 0.502247f, 0.49904f };

real beta( unsigned int n)
{
	return (5.0f/4.0f - (3.0f+2.0f*cos( Angle::PiMult2 / n)) * (3.0f+2.0f*cos( Angle::PiMult2 / n)) / 32.0f);
}

real alpha( unsigned int n)
{
	real b;

	if (n <= 20) return ALPHA[n-1];

	b = beta(n);

	return n * (1 - b) / b;
}

//*********************************************************************************************
/*
LoopEdgeList :: LoopEdgeList( Submesh * p_submesh)
	:	m_submesh( p_submesh)
{
	_fill();
}

LoopEdgeList :: ~LoopEdgeList()
{
	m_facesEdges.clear();
	m_allEdges.clear();
	m_vertexEdges.clear();
	m_originalVertices.clear();
}

void LoopEdgeList :: Divide()
{
	FaceEdgesPtrArray l_newFaces;
	VertexArray l_newVertices;
	size_t l_size = m_facesEdges.size();

	for (size_t i = 0 ; i < l_size ; i++)
	{
		l_newFaces.clear();
		m_facesEdges[0]->Divide( 0.5f, m_vertexEdges, m_vertexNeighbourhood, m_allEdges, l_newFaces, l_newVertices);
		m_facesEdges[0].reset();
		m_facesEdges.erase( m_facesEdges.begin());

		for (size_t j = 0 ; j < l_newVertices.size() ; j++)
		{
			if (m_originalVertices.find( l_newVertices[j]) == m_originalVertices.end())
			{
				m_originalVertices.insert( VertexVtxMap::value_type( l_newVertices[j], l_newVertices[j]));
			}
		}
		l_newVertices.clear();
		for (size_t j = 0 ; j < l_newFaces.size() ; j++)
		{
			m_facesEdges.push_back( l_newFaces[j]);
		}
	}
}

void LoopEdgeList :: Average( const Point3r & p_center)
{
	EdgePtrMapVPtrMap::iterator l_it = m_vertexEdges.begin();
	IntVPtrMap::iterator l_nit;
	EdgePtrMap::iterator l_edgeIt;
	int l_nb;
	VertexPtr l_vertex;

	while (l_it != m_vertexEdges.end())
	{
		l_edgeIt = l_it->second.begin();
		l_vertex = l_it->first;
		l_nit = m_vertexNeighbourhood.find( l_vertex);
		l_nb = l_nit->second;

		l_vertex->GetCoords() -= p_center;
		l_vertex->GetCoords() *= alpha( l_nb);

		while (l_edgeIt != l_it->second.end())
		{
			l_vertex->GetCoords() += ((m_originalVertices.find( l_edgeIt->first)->second)->GetCoords() - p_center);
			++l_edgeIt;
		}

		l_vertex->GetCoords() /= (alpha( l_nb) + l_nb);
		l_vertex->GetCoords() += p_center;
		++l_it;
	}
}

void LoopEdgeList :: _fill()
{
	FaceEdgesPtr l_faceEdges;

	for (size_t i = 0 ; i < m_submesh->GetNbSmoothGroups() ; i++)
	{
		FaceArray l_faces;
		SmoothingGroup & l_group = m_submesh->GetSmoothGroup( i);

		for (size_t j = 0 ; j < l_group->m_faces.size() ; j++)
		{
			Face & l_face = l_group.m_faces[j];
			l_faces.push_back( l_face);

			Vertex & l_vertex1 = l_face.m_vertex[0];
			Vertex & l_vertex2 = l_face.m_vertex[1];
			Vertex & l_vertex3 = l_face.m_vertex[2];

			if (m_originalVertices.find( l_vertex1) == m_originalVertices.end())
			{
				m_originalVertices.insert( VertexVtxMap::value_type( l_vertex1, l_vertex1));
			}
			if (m_originalVertices.find( l_vertex2) == m_originalVertices.end())
			{
				m_originalVertices.insert( VertexVtxMap::value_type( l_vertex2, l_vertex2));
			}
			if (m_originalVertices.find( l_vertex3) == m_originalVertices.end())
			{
				m_originalVertices.insert( VertexVtxMap::value_type( l_vertex3, l_vertex3));
			}

			l_faceEdges = FaceEdgesPtr( new FaceEdges( m_submesh, i, l_face, m_vertexEdges, m_vertexNeighbourhood, m_allEdges));
			m_facesEdges.push_back( l_faceEdges);
		}
		m_submesh->GetSmoothGroups()[i]->m_faces.clear();
	}
}
*/
//*********************************************************************************************

LoopSubdiviser :: LoopSubdiviser( Submesh * p_submesh)
	:	Subdiviser( p_submesh),
		m_edges( new LoopEdgeList( p_submesh))
{}
/*
LoopSubdiviser :: ~LoopSubdiviser()
{
	delete m_edges;
}

void LoopSubdiviser :: Subdivide( const Point3r & p_center)
{
	m_edges->Divide();
	m_edges->Average( p_center);
}
*/
//*********************************************************************************************