#include "Castor3D/Model/Mesh/Importer.hpp"

#include "Castor3D/Engine.hpp"

#include "Castor3D/Event/Frame/FrameListener.hpp"
#include "Castor3D/Event/Frame/InitialiseEvent.hpp"
#include "Castor3D/Material/Material.hpp"
#include "Castor3D/Material/Pass/Pass.hpp"
#include "Castor3D/Material/Texture/TextureConfiguration.hpp"
#include "Castor3D/Material/Texture/TextureLayout.hpp"
#include "Castor3D/Material/Texture/TextureUnit.hpp"
#include "Castor3D/Material/Texture/TextureView.hpp"
#include "Castor3D/Model/Mesh/Mesh.hpp"
#include "Castor3D/Model/Mesh/Submesh/Submesh.hpp"
#include "Castor3D/Model/Vertex.hpp"
#include "Castor3D/Scene/Geometry.hpp"
#include "Castor3D/Scene/Scene.hpp"

using namespace castor;

namespace castor3d
{
	MeshImporter::MeshImporter( Engine & engine )
		: OwnedBy< Engine >( engine )
		, m_fileName()
	{
	}

	bool MeshImporter::import( Mesh & mesh
		, Path const & fileName
		, Parameters const & parameters
		, bool initialise )
	{
		bool splitSubmeshes = false;
		m_parameters.get( cuT( "split_mesh" ), splitSubmeshes );
		m_fileName = fileName;
		m_filePath = m_fileName.getPath();
		m_parameters = parameters;
		m_nodes.clear();
		m_geometries.clear();
		bool result = true;

		if ( !mesh.getSubmeshCount() )
		{
			result = doImportMesh( mesh );

			if ( result && initialise )
			{
				float scale = 1.0f;

				if ( m_parameters.get( cuT( "rescale" ), scale )
					&& std::abs( scale - 1.0f ) > 0.01f )
				{
					for ( auto submesh : mesh )
					{
						for ( auto & vertex : submesh->getPoints() )
						{
							vertex.pos *= scale;
						}
					}
				}

				mesh.computeContainers();

				for ( auto submesh : mesh )
				{
					mesh.getScene()->getListener().postEvent( makeGpuInitialiseEvent( *submesh ) );
				}
			}
		}
		else
		{
			for ( auto submesh : mesh )
			{
				submesh->setMaterial( nullptr, submesh->getDefaultMaterial(), false );
			}
		}

		return result;
	}

	TextureUnitSPtr MeshImporter::loadTexture( Path const & path
		, TextureConfiguration const & config )const
	{
		TextureUnitSPtr result;
		Path relative;
		Path folder;

		if ( File::fileExists( path ) )
		{
			relative = path;
		}
		else if ( File::fileExists( m_filePath / path ) )
		{
			auto fullPath = m_filePath / path;
			folder = fullPath.getPath();
			relative = fullPath.getFileName( true );;
		}
		else
		{
			PathArray files;
			String fileName = path.getFileName( true );
			File::listDirectoryFiles( m_filePath, files, true );
			auto it = std::find_if( files.begin()
				, files.end()
				, [&fileName]( Path const & file )
				{
					return file.getFileName( true ) == fileName
						|| file.getFileName( true ).find( fileName ) == 0;
				} );

			folder = m_filePath;

			if ( it != files.end() )
			{
				relative = *it;
				relative = Path{ relative.substr( folder.size() + 1 ) };
			}
			else
			{
				relative = Path{ fileName };
			}
		}

		if ( !File::fileExists( folder / relative ) )
		{
			log::error << cuT( "Couldn't load texture file [" ) << path << cuT( "]: File does not exist." ) << std::endl;
			return nullptr;
		}

		try
		{
			result = std::make_shared< TextureUnit >( *getEngine() );
			result->setTexture( createTextureLayout( *getEngine(), relative, folder ) );
			result->setConfiguration( config );
			return result;
		}
		catch ( castor::Exception & exc )
		{
			log::error << cuT( "Error encountered while loading texture file [" ) << path << cuT( "]: " ) << exc.what() << std::endl;
		}
		catch ( std::exception & exc )
		{
			log::error << cuT( "Error encountered while loading texture file [" ) << path << cuT( "]: " ) << exc.what() << std::endl;
		}
		catch ( ... )
		{
			log::error << cuT( "Error encountered while loading texture file [" ) << path << cuT( "]: Unknown error" ) << std::endl;
		}

		return nullptr;
	}

	TextureUnitSPtr MeshImporter::loadTexture( Path const & path
		, TextureConfiguration const & config
		, Pass & pass )const
	{
		auto result = loadTexture( path
			, config );

		if ( result )
		{
			pass.addTextureUnit( result );
		}

		return result;
	}
}
