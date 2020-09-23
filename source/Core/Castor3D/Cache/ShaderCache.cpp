#include "Castor3D/Cache/ShaderCache.hpp"

#include "Castor3D/Engine.hpp"
#include "Castor3D/Event/Frame/FunctorEvent.hpp"
#include "Castor3D/Render/RenderPass.hpp"
#include "Castor3D/Render/RenderSystem.hpp"
#include "Castor3D/Shader/Program.hpp"

#include <ShaderWriter/Source.hpp>

#include <ashespp/Core/Device.hpp>

namespace castor3d
{
	namespace
	{
		void initialiseProgram( Engine & engine
			, ShaderProgramSPtr program )
		{
			engine.sendEvent( makeFunctorEvent( EventType::ePreRender
				, [program]()
				{
					program->initialise();
				} ) );
		}
	}

	ShaderProgramCache::ShaderProgramCache( Engine & engine )
		: OwnedBy< Engine >( engine )
	{
	}

	ShaderProgramCache::~ShaderProgramCache()
	{
	}

	void ShaderProgramCache::cleanup()
	{
		auto lock( castor::makeUniqueLock( m_mutex ) );

		for ( auto & program : m_programs )
		{
			getEngine()->postEvent( makeFunctorEvent( EventType::ePreRender
				, [&program]()
				{
					program->cleanup();
				} ) );
		}
	}

	void ShaderProgramCache::clear()
	{
		auto lock( castor::makeUniqueLock( m_mutex ) );
		m_autogenerated.clear();
		m_programs.clear();
	}

	ShaderProgramSPtr ShaderProgramCache::getNewProgram( castor::String const & name
		, bool initialise )
	{
		auto result = std::make_shared< ShaderProgram >( name , *getEngine()->getRenderSystem() );
		auto lock( castor::makeUniqueLock( m_mutex ) );
		doAddProgram( result );

		if ( initialise )
		{
			initialiseProgram( *getEngine(), result );
		}

		return result;
	}

	ShaderProgramSPtr ShaderProgramCache::doFindAutomaticProgram( PipelineFlags const & flags )
	{
		auto lock( castor::makeUniqueLock( m_mutex ) );
		auto it = m_autogenerated.find( flags );

		if ( it != m_autogenerated.end() )
		{
			return it->second;
		}

		return nullptr;
	}

	ShaderProgramSPtr ShaderProgramCache::getAutomaticProgram( RenderPass const & renderPass
		, PipelineFlags const & flags )
	{
		auto result = doFindAutomaticProgram( flags );

		if ( result )
		{
			return result;
		}

		result = doCreateAutomaticProgram( renderPass, flags );
		CU_Require( result );
		doAddAutomaticProgram( result, flags );
		initialiseProgram( *getEngine(), result );
		return result;
	}

	ShaderProgramSPtr ShaderProgramCache::doCreateAutomaticProgram( RenderPass const & renderPass
		, PipelineFlags const & flags )const
	{
		ShaderProgramSPtr result = std::make_shared< ShaderProgram >( renderPass.getName(), *getEngine()->getRenderSystem() );
		result->setSource( VK_SHADER_STAGE_VERTEX_BIT
			, renderPass.getVertexShaderSource( flags ) );
		result->setSource( VK_SHADER_STAGE_FRAGMENT_BIT
			, renderPass.getPixelShaderSource( flags ) );
		auto geometry = renderPass.getGeometryShaderSource( flags );

		if ( geometry )
		{
			result->setSource( VK_SHADER_STAGE_GEOMETRY_BIT
				, std::move( geometry ) );
		}

		return result;
	}

	void ShaderProgramCache::doAddAutomaticProgram( ShaderProgramSPtr program
		, PipelineFlags const & flags )
	{
		auto lock( castor::makeUniqueLock( m_mutex ) );
		m_autogenerated.insert( { flags, program } );
		doAddProgram( program );
	}

	void ShaderProgramCache::doAddProgram( ShaderProgramSPtr program )
	{
		m_programs.push_back( std::move( program ) );
	}
}