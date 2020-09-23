#include "Castor3D/Cache/BillboardUboPools.hpp"

#include "Castor3D/Engine.hpp"
#include "Castor3D/Buffer/UniformBufferPools.hpp"
#include "Castor3D/Material/Material.hpp"
#include "Castor3D/Material/Pass/Pass.hpp"
#include "Castor3D/Material/Texture/TextureUnit.hpp"
#include "Castor3D/Render/RenderPassTimer.hpp"
#include "Castor3D/Render/RenderSystem.hpp"
#include "Castor3D/Scene/BillboardList.hpp"
#include "Castor3D/Scene/SceneNode.hpp"

#include <CastorUtils/Miscellaneous/Hash.hpp>

using namespace castor;

namespace castor3d
{
	namespace
	{
		size_t hash( BillboardBase const & billboard
			, Pass const & pass )
		{
			size_t result = std::hash< BillboardBase const * >{}( &billboard );
			castor::hashCombine( result, pass );
			return result;
		}
	}

	BillboardUboPools::BillboardUboPools( RenderSystem & renderSystem )
		: m_renderSystem{ renderSystem }
	{
	}

	void BillboardUboPools::update()
	{
		for ( auto & pair : m_entries )
		{
			auto & entry = pair.second;
			auto & modelData = entry.modelUbo.getData();
			modelData.shadowReceiver = entry.billboard.isShadowReceiver();
			modelData.materialIndex = entry.pass.getId();
			auto & modelMatrixData = entry.modelMatrixUbo.getData();
			modelMatrixData.prvModel = modelMatrixData.curModel;
			modelMatrixData.curModel = entry.billboard.getNode()->getDerivedTransformationMatrix();
			auto & billboardData = entry.billboardUbo.getData();
			billboardData.dimensions = entry.billboard.getDimensions();
			auto & texturesData = entry.texturesUbo.getData();
			uint32_t index = 0u;

			for ( auto & unit : entry.pass )
			{
				texturesData.indices[index / 4u][index % 4] = unit->getId();
				++index;
			}
		}
	}

	void BillboardUboPools::registerElement( BillboardBase & billboard )
	{
		m_connections.emplace( &billboard, billboard.onMaterialChanged.connect( [this]( BillboardBase const & billboard
			, MaterialSPtr oldMaterial
			, MaterialSPtr newMaterial )
		{
			if ( oldMaterial )
			{
				for ( auto & pass : *oldMaterial )
				{
					doRemoveEntry( billboard, *pass );
				}
			}

			if ( newMaterial )
			{
				for ( auto & pass : *newMaterial )
				{
					m_entries.emplace( hash( billboard, *pass )
						, doCreateEntry( billboard, *pass ) );
				}
			}
		} ) );

		for ( auto & pass : *billboard.getMaterial() )
		{
			m_entries.emplace( hash( billboard, *pass )
				, doCreateEntry( billboard, *pass ) );
		}
	}

	void BillboardUboPools::unregisterElement( BillboardBase & billboard )
	{
		auto it = m_connections.find( &billboard );

		if ( it != m_connections.end() )
		{
			m_connections.erase( it );

			for ( auto & pass : *billboard.getMaterial() )
			{
				doRemoveEntry( billboard, *pass );
			}
		}
	}

	BillboardUboPools::PoolsEntry BillboardUboPools::getUbos( BillboardBase const & billboard, Pass const & pass )const
	{
		return m_entries.at( hash( billboard, pass ) );
	}

	void BillboardUboPools::clear()
	{
		auto & uboPools = *getCurrentRenderDevice( m_renderSystem ).uboPools;
		for ( auto & entry : m_entries )
		{
			uboPools.putBuffer( entry.second.modelUbo );
			uboPools.putBuffer( entry.second.modelMatrixUbo );
			uboPools.putBuffer( entry.second.billboardUbo );
			uboPools.putBuffer( entry.second.pickingUbo );
			uboPools.putBuffer( entry.second.texturesUbo );
		}
	}
	
	BillboardUboPools::PoolsEntry BillboardUboPools::doCreateEntry( BillboardBase const & billboard
		, Pass const & pass )
	{
		auto & uboPools = *getCurrentRenderDevice( m_renderSystem ).uboPools;
		return
		{
			billboard,
			pass,
			uboPools.getBuffer< ModelUboConfiguration >( VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT ),
			uboPools.getBuffer< ModelMatrixUboConfiguration >( VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT ),
			uboPools.getBuffer< BillboardUboConfiguration >( VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT ),
			uboPools.getBuffer< PickingUboConfiguration >( VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT ),
			uboPools.getBuffer< TexturesUboConfiguration >( VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT ),
		};
	}

	void BillboardUboPools::doRemoveEntry( BillboardBase const & billboard
		, Pass const & pass )
	{
		auto & uboPools = *getCurrentRenderDevice( m_renderSystem ).uboPools;
		auto entry = getUbos( billboard, pass );
		m_entries.erase( hash( billboard, pass ) );
		uboPools.putBuffer( entry.modelUbo );
		uboPools.putBuffer( entry.modelMatrixUbo );
		uboPools.putBuffer( entry.billboardUbo );
		uboPools.putBuffer( entry.pickingUbo );
		uboPools.putBuffer( entry.texturesUbo );
	}
}