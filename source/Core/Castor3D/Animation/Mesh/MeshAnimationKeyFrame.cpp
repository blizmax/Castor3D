#include "Castor3D/Animation/Mesh/MeshAnimationKeyFrame.hpp"

#include "Castor3D/Animation/Mesh/MeshAnimation.hpp"
#include "Castor3D/Mesh/Submesh.hpp"

#include <CastorUtils/Design/ArrayView.hpp>

using namespace castor;

namespace castor3d
{
	//*************************************************************************************************

	namespace
	{
		BoundingBox doComputeBoundingBox( InterleavedVertexArray const & points )
		{
			if ( points.empty() )
			{
				return BoundingBox{};
			}

			Point3r min{ points[0].pos };
			Point3r max{ points[0].pos };

			if ( points.size() > 1 )
			{
				for ( auto & vertex : castor::makeArrayView( &points[1], points.size() - 1u ) )
				{
					Point3r cur{ vertex.pos };
					max[0] = std::max( cur[0], max[0] );
					max[1] = std::max( cur[1], max[1] );
					max[2] = std::max( cur[2], max[2] );
					min[0] = std::min( cur[0], min[0] );
					min[1] = std::min( cur[1], min[1] );
					min[2] = std::min( cur[2], min[2] );
				}
			}

			return BoundingBox{ min, max };
		}
	}

	//*************************************************************************************************

	MeshAnimationKeyFrame::MeshAnimationKeyFrame( MeshAnimation & parent
		, Milliseconds const & timeIndex )
		: AnimationKeyFrame{ timeIndex }
		, OwnedBy< MeshAnimation >{ parent }
	{
	}

	SubmeshAnimationBufferMap::const_iterator MeshAnimationKeyFrame::find( Submesh const & submesh )const
	{
		return m_submeshesBuffers.find( submesh.getId() );
	}

	void MeshAnimationKeyFrame::addSubmeshBuffer( Submesh const & submesh
		, InterleavedVertexArray const & buffer )
	{
		auto boundingBox = doComputeBoundingBox( buffer );
		m_submeshesBuffers.emplace( submesh.getId()
			, SubmeshAnimationBuffer
			{
				buffer,
				boundingBox
			} );

		if ( m_submeshesBuffers.size() == 1u )
		{
			m_boundingBox = boundingBox;
		}
		else
		{
			m_boundingBox = m_boundingBox.getUnion( boundingBox );
		}
	}

	//*************************************************************************************************
}