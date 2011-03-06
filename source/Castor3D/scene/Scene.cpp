#include "Castor3D/PrecompiledHeader.h"

#include "Castor3D/scene/Scene.h"
#include "Castor3D/scene/SceneManager.h"
#include "Castor3D/scene/SceneNode.h"
#include "Castor3D/camera/Camera.h"
#include "Castor3D/light/PointLight.h"
#include "Castor3D/light/SpotLight.h"
#include "Castor3D/light/DirectionalLight.h"
#include "Castor3D/geometry/primitives/Geometry.h"
#include "Castor3D/geometry/mesh/Mesh.h"
#include "Castor3D/geometry/mesh/Submesh.h"
#include "Castor3D/geometry/basic/SmoothingGroup.h"
#include "Castor3D/geometry/basic/Vertex.h"
#include "Castor3D/geometry/basic/Face.h"
#include "Castor3D/geometry/mesh/MeshManager.h"
#include "Castor3D/material/MaterialManager.h"
#include "Castor3D/material/Material.h"
#include "Castor3D/render_system/RenderSystem.h"
#include "Castor3D/main/Root.h"
#include "Castor3D/main/Pipeline.h"
#include "Castor3D/camera/Ray.h"
#include "Castor3D/importer/Importer.h"
#include "Castor3D/animation/AnimationManager.h"

using namespace Castor3D;

Scene :: Scene( Manager<Scene> * p_pManager, const String & p_name)
	:	m_pManager	( p_pManager)
	,	m_strName	( p_name)
	,	m_changed	( false)
{
	m_rootNode.reset( new SceneNode( this, CU_T( "RootNode")));
//	SceneNodePtr l_camNode = SceneNodePtr( new SceneNode( this, CU_T( "RootCameraNode")));
//	m_rootCamera.reset( new Camera( this, CU_T( "RootCamera"), 800, 600, l_camNode, Viewport::e3DView));
//	m_rootCamera->GetParent()->Translate( 0.0f, 0.0f, -5.0f);
	m_pAnimationManager = new AnimationManager( this);
}

Scene :: ~Scene()
{
	ClearScene();
	m_rootCamera.reset();
	m_rootNode.reset();
	delete m_pAnimationManager;
}

void Scene :: ClearScene()
{
	CASTOR_MUTEX_AUTO_SCOPED_LOCK();
	if (RenderSystem::GetSingletonPtr() != NULL)
	{
		RenderSystem::GetSingletonPtr()->SetCurrentShaderProgram( NULL);
	}
/*
	RemoveAllGeometries();
	RemoveAllLights();
*/
	LightPtrStrMap::iterator l_itLights = m_addedLights.begin();

	while (m_addedLights.size() > 0)
	{
		l_itLights->second->Detach();
		l_itLights->second.reset();
		m_addedLights.erase( l_itLights);
		l_itLights = m_addedLights.begin();
	}

	GeometryPtrStrMap::iterator l_itGeometries = m_addedPrimitives.begin();

	while (m_addedPrimitives.size() > 0)
	{
		l_itGeometries->second->Detach();
		l_itGeometries->second.reset();
		m_addedPrimitives.erase( l_itGeometries);
		l_itGeometries = m_addedPrimitives.begin();
	}

	_removeNodesAndUnattached( m_addedCameras);
}

void Scene :: Render( ePRIMITIVE_TYPE p_displayMode, real p_tslf)
{
	_deleteToDelete();
	CASTOR_MUTEX_AUTO_SCOPED_LOCK();
	m_pAnimationManager->Update();

	RenderSystem::GetSingletonPtr()->RenderAmbientLight( m_clAmbientLight);

	LightPtrStrMap::iterator l_itEnd = m_addedLights.end();

	for (LightPtrStrMap::iterator l_it = m_addedLights.begin() ; l_it != l_itEnd ; ++l_it)
	{
		l_it->second->Render( p_displayMode);
	}

	m_rootNode->Render( p_displayMode);

	for (LightPtrStrMap::iterator l_it = m_addedLights.begin() ; l_it != l_itEnd ; ++l_it)
	{
		l_it->second->EndRender();
	}
}

SceneNodePtr Scene :: CreateSceneNode( const String & p_name, SceneNode * p_parent)
{
	CASTOR_MUTEX_AUTO_SCOPED_LOCK();
	SceneNodePtr l_pReturn;

	if (p_name != CU_T( "RootNode") && m_addedNodes.find( p_name) == m_addedNodes.end())
	{
		l_pReturn = SceneNodePtr( new SceneNode( this, p_name));

		if (p_parent != NULL)
		{
			l_pReturn->AttachTo( p_parent);
		}
		else
		{
			l_pReturn->AttachTo( m_rootNode.get());
		}

		m_addedNodes[p_name] = l_pReturn;
	}
	else
	{
		Logger::LogWarning( CU_T( "CreateSceneNode - Can't create scene node %s - Another scene node with the same name already exists"), p_name.char_str());
		l_pReturn = m_addedNodes.find( p_name)->second;
	}

	return l_pReturn;
}

GeometryPtr Scene :: CreatePrimitive( const String & p_name, eMESH_TYPE p_type, 
									 const String & p_meshName, UIntArray p_faces,
									 FloatArray p_size)
{
	CASTOR_MUTEX_AUTO_SCOPED_LOCK();
	GeometryPtr l_pReturn;

	if (m_addedPrimitives.find( p_name) == m_addedPrimitives.end())
	{
		MeshPtr l_mesh;

		if (static_cast<SceneManager *>( m_pManager)->GetMeshManager()->HasElement( p_meshName))
		{
			l_mesh = static_cast<SceneManager *>( m_pManager)->GetMeshManager()->GetElementByName( p_meshName);
		}
		else
		{
			l_mesh = static_cast<SceneManager *>( m_pManager)->GetMeshManager()->CreateMesh( p_meshName, p_faces, p_size, p_type);
			Logger::LogMessage( CU_T( "CreatePrimitive - Mesh %s created"), p_meshName.char_str());
			l_mesh->ComputeNormals();
			Logger::LogMessage( CU_T( "CreatePrimitive - Normals setting finished"));
		}

		if (l_mesh != NULL)
		{
			l_pReturn = GeometryPtr( new Geometry( this, l_mesh, SceneNodePtr(), p_name));
			Logger::LogMessage( CU_T( "CreatePrimitive - Geometry %s created"), p_name.char_str());
			m_addedPrimitives[p_name] = l_pReturn;
			m_newlyAddedPrimitives[p_name] = l_pReturn;
			m_changed = true;
		}
		else
		{
			Logger::LogMessage( CU_T( "CreatePrimitive - Can't create primitive %s - Mesh creation failed"), p_name.char_str());
		}
	}
	else
	{
		Logger::LogMessage( CU_T( "CreatePrimitive - Can't create primitive %s - Another primitive with the same name already exists"), p_name.char_str());
	}

	return l_pReturn;
}

GeometryPtr Scene :: CreatePrimitive( const String & p_strName)
{
	CASTOR_MUTEX_AUTO_SCOPED_LOCK();
	GeometryPtr l_pReturn;
	GeometryPtrStrMap::iterator l_it = m_addedPrimitives.find( p_strName);

	if (l_it == m_addedPrimitives.end())
	{
		l_pReturn = GeometryPtr( new Geometry( this, MeshPtr(), SceneNodePtr(), p_strName));
	}
	else
	{
		Logger::LogWarning( CU_T( "CreatePrimitive - Can't create primitive %s - Another primitive with the same name already exists"), p_strName.c_str());
		l_pReturn = l_it->second;
	}

	return l_pReturn;
}

CameraPtr Scene :: CreateCamera( const String & p_name, int p_ww, int p_wh, SceneNodePtr p_pNode,
							   Viewport::eTYPE p_type)
{
	CASTOR_MUTEX_AUTO_SCOPED_LOCK();
	CameraPtr l_pReturn;

	if (m_addedCameras.find( p_name) != m_addedCameras.end())
	{
		Logger::LogMessage( CU_T( "CreateCamera - Can't create camera %s - A camera with that name already exists"), p_name.char_str());
	}
	else
	{
		Logger::LogMessage( CU_T( "CreateCamera - Creating Camera %s"), p_name.char_str());
		l_pReturn = CameraPtr( new Camera( this, p_name, p_ww, p_wh, p_pNode, p_type));

		if (m_rootCamera == NULL)
		{
			m_rootCamera = l_pReturn;
		}

		m_addedCameras[p_name] = l_pReturn;
	}

	return l_pReturn;
}

void Scene :: CreateList( eNORMALS_MODE p_nm, bool p_showNormals)
{
	CASTOR_MUTEX_AUTO_SCOPED_LOCK();
	m_normalsMode = p_nm;

	m_nbFaces = 0;
	m_nbVertex = 0;

	if (m_newlyAddedPrimitives.size() > 0)
	{
		GeometryPtrStrMap::iterator l_it = m_newlyAddedPrimitives.begin();

		for ( ; l_it != m_newlyAddedPrimitives.end() ; ++l_it)
		{
			l_it->second->CreateBuffers( m_normalsMode, m_nbFaces, m_nbVertex);
		}

		m_newlyAddedPrimitives.clear();
	}
	else
	{
		m_rootNode->CreateList( m_normalsMode, p_showNormals, m_nbFaces, m_nbVertex);
	}

	Logger::LogMessage( CU_T( "CreateList - %s - NbVertex : %d - NbFaces : %d"), m_strName.char_str(), m_nbVertex, m_nbFaces);
	m_changed = false;
}

SceneNodePtr Scene :: GetNode( const String & p_name)const
{
	CASTOR_MUTEX_AUTO_SCOPED_LOCK();
	SceneNodePtr l_pReturn;

	if (p_name == CU_T( "RootNode"))
	{
		l_pReturn = m_rootNode;
	}
	else
	{
		SceneNodePtrStrMap::const_iterator l_it = m_addedNodes.find( p_name);

		if (l_it != m_addedNodes.end())
		{
			l_pReturn = l_it->second;
		}
	}

	return l_pReturn;
}

void Scene :: AddNode( SceneNodePtr p_node)
{
	CASTOR_MUTEX_AUTO_SCOPED_LOCK();
	SceneNodePtrStrMap::const_iterator l_it = m_addedNodes.find( p_node->GetName());

	if (l_it != m_addedNodes.end())
	{
		Logger::LogMessage( CU_T( "AddNode - Can't add node %s - A node with that name already exists"), p_node->GetName().char_str());
	}
	else
	{
		m_addedNodes[p_node->GetName()] = p_node;
	}
}

void Scene :: AddLight( LightPtr p_light)
{
	CASTOR_MUTEX_AUTO_SCOPED_LOCK();
	LightPtrStrMap::const_iterator l_it = m_addedLights.find( p_light->GetName());

	if (l_it != m_addedLights.end())
	{
		Logger::LogMessage( CU_T( "AddLight - Can't add light %s - A light with that name already exists"), p_light->GetName().char_str());
	}
	else
	{
		m_addedLights[p_light->GetName()] = p_light;
	}
}

void Scene :: AddGeometry( GeometryPtr p_geometry)
{
	CASTOR_MUTEX_AUTO_SCOPED_LOCK();
	GeometryPtrStrMap::const_iterator l_it = m_addedPrimitives.find( p_geometry->GetName());

	if (l_it != m_addedPrimitives.end())
	{
		Logger::LogMessage( CU_T( "AddGeometry - Can't add geometry %s - A geometry with that name already exists"), p_geometry->GetName().char_str());
	}
	else
	{
		m_addedPrimitives[p_geometry->GetName()] = p_geometry;
		m_newlyAddedPrimitives[p_geometry->GetName()] = p_geometry;
		m_changed = true;
	}
}

GeometryPtr Scene :: GetGeometry( const String & p_name)
{
	CASTOR_MUTEX_AUTO_SCOPED_LOCK();
	GeometryPtr l_pReturn;

	GeometryPtrStrMap::iterator l_it = m_addedPrimitives.find( p_name);

	if (l_it != m_addedPrimitives.end())
	{
		l_pReturn = l_it->second;
	}

	return l_pReturn;
}

void Scene :: RemoveGeometry( GeometryPtr p_geometry)
{
	CASTOR_MUTEX_AUTO_SCOPED_LOCK();

	if (p_geometry != NULL)
	{
		GeometryPtrStrMap::iterator l_it = m_addedPrimitives.find( p_geometry->GetName());

		if (l_it != m_addedPrimitives.end())
		{
			m_addedPrimitives.erase( l_it);
		}

		m_arrayPrimitivesToDelete.push_back( p_geometry);
	}
}

void Scene :: RemoveLight( LightPtr p_pLight)
{
	CASTOR_MUTEX_AUTO_SCOPED_LOCK();

	if (p_pLight != NULL)
	{
		LightPtrStrMap::iterator l_it = m_addedLights.find( p_pLight->GetName());

		if (l_it != m_addedLights.end())
		{
			m_addedLights.erase( l_it);
		}

		m_arrayLightsToDelete.push_back( p_pLight);
	}
}

void Scene :: RemoveNode( SceneNodePtr p_pNode)
{
	CASTOR_MUTEX_AUTO_SCOPED_LOCK();

	if (p_pNode != NULL)
	{
		SceneNodePtrStrMap::iterator l_it = m_addedNodes.find( p_pNode->GetName());

		if (l_it != m_addedNodes.end())
		{
			m_addedNodes.erase( l_it);
		}

		m_arrayNodesToDelete.push_back( p_pNode);
	}
}

void Scene :: RemoveCamera( CameraPtr p_pCamera)
{
	CASTOR_MUTEX_AUTO_SCOPED_LOCK();

	if (p_pCamera != NULL)
	{
		CameraPtrStrMap::iterator l_it = m_addedCameras.find( p_pCamera->GetName());

		if (l_it != m_addedCameras.end())
		{
			m_addedCameras.erase( l_it);
		}

		m_arrayCamerasToDelete.push_back( p_pCamera);
	}
}

void Scene :: RemoveAllNodes()
{
	CASTOR_MUTEX_AUTO_SCOPED_LOCK();
	SceneNodePtrStrMap::iterator l_it = m_addedNodes.begin();

	while (m_addedNodes.size() > 0)
	{
		l_it->second->Detach();
		m_arrayNodesToDelete.push_back( l_it->second);
		m_addedNodes.erase( l_it);
		l_it = m_addedNodes.begin();
	}

	m_addedNodes.clear();
}

void Scene :: RemoveAllLights()
{
	CASTOR_MUTEX_AUTO_SCOPED_LOCK();
	LightPtrStrMap::iterator l_it = m_addedLights.begin();
	SceneNodePtrStrMap::iterator l_itNodes;

	while (m_addedLights.size() > 0)
	{
		l_it->second->Detach();
		m_arrayLightsToDelete.push_back( l_it->second);
		m_addedLights.erase( l_it);
		l_it = m_addedLights.begin();
	}

	m_addedLights.clear();
}

void Scene :: RemoveAllGeometries()
{
	CASTOR_MUTEX_AUTO_SCOPED_LOCK();
	GeometryPtrStrMap::iterator l_it = m_addedPrimitives.begin();
	SceneNodePtrStrMap::iterator l_itNodes;

	while (m_addedPrimitives.size() > 0)
	{
		l_it->second->Detach();
		m_arrayPrimitivesToDelete.push_back( l_it->second);
		m_addedPrimitives.erase( l_it);
		l_it = m_addedPrimitives.begin();
	}

	m_addedPrimitives.clear();
}

void Scene :: RemoveAllCameras()
{
	CASTOR_MUTEX_AUTO_SCOPED_LOCK();
	CameraPtrStrMap::iterator l_it = m_addedCameras.begin();

	while (m_addedCameras.size() > 0)
	{
		l_it->second->Detach();
		m_arrayCamerasToDelete.push_back( l_it->second);
		m_addedCameras.erase( l_it);
		l_it = m_addedCameras.begin();
	}

	m_addedCameras.clear();
}

BoolStrMap Scene :: GetGeometriesVisibility()
{
	CASTOR_MUTEX_AUTO_SCOPED_LOCK();
	BoolStrMap l_mapReturn;
	GeometryPtrStrMap::iterator l_it = m_addedPrimitives.begin();

	while (l_it != m_addedPrimitives.end())
	{
		l_mapReturn[l_it->first] = l_it->second->GetParent()->IsVisible();
		++l_it;
	}

	return l_mapReturn;
}

bool Scene :: Write( File & p_file)const
{
	CASTOR_MUTEX_AUTO_SCOPED_LOCK();
	bool l_bReturn = true;

	Logger::LogMessage( CU_T( "Writing Scene Name"));

	if (l_bReturn)
	{
		Logger::LogMessage( CU_T( "Writing Root Camera"));
		l_bReturn = m_rootCamera->Write( p_file);
	}

	if (l_bReturn)
	{
		Logger::LogMessage( CU_T( "Writing Root Node"));
		l_bReturn = m_rootNode->Write( p_file);
	}

	if (l_bReturn)
	{
		Logger::LogMessage( CU_T( "Writing Lights"));

		l_bReturn = _writeLights( p_file);
	}

	if (l_bReturn)
	{
		Logger::LogMessage( CU_T( "Writing Geometries"));
		l_bReturn = _writeGeometries( p_file);
	}

	return l_bReturn;
}

bool Scene :: Save( File & p_file)const
{
	CASTOR_MUTEX_AUTO_SCOPED_LOCK();
	bool l_bReturn = true;

	Logger::LogMessage( CU_T( "Writing Scene Name"));

	if (l_bReturn)
	{
		Logger::LogMessage( CU_T( "Writing Root Camera"));
		l_bReturn = m_rootCamera->Save( p_file);
	}

	if (l_bReturn)
	{
		Logger::LogMessage( CU_T( "Writing Root Node"));
		l_bReturn = m_rootNode->Save( p_file);
	}

	if (l_bReturn)
	{
		Logger::LogMessage( CU_T( "Writing Lights"));

		l_bReturn = _saveLights( p_file);
	}

	if (l_bReturn)
	{
		Logger::LogMessage( CU_T( "Writing Geometries"));
		l_bReturn = _saveGeometries( p_file);
	}

	return l_bReturn;
}

bool Scene :: Load( File & p_file)
{
	CASTOR_MUTEX_AUTO_SCOPED_LOCK();
	bool l_bReturn = true;

	Logger::LogMessage( CU_T( "Saving Scene Name"));

	if (l_bReturn)
	{
		Logger::LogMessage( CU_T( "Saving Root Camera"));
		l_bReturn = m_rootCamera->Load( p_file);
	}

	if (l_bReturn)
	{
		Logger::LogMessage( CU_T( "Saving Root Node"));
		l_bReturn = m_rootNode->Load( p_file);
	}

	if (l_bReturn)
	{
		Logger::LogMessage( CU_T( "Saving Lights"));

		l_bReturn = _loadLights( p_file);
	}

	if (l_bReturn)
	{
		Logger::LogMessage( CU_T( "Saving Geometries"));
		l_bReturn = _loadGeometries( p_file);
	}

	return l_bReturn;
}

void Scene :: Merge( ScenePtr p_pScene)
{
	{
		CASTOR_MUTEX_AUTO_SCOPED_LOCK();
		CASTOR_MUTEX_SCOPED_LOCK( p_pScene->m_mutex);

		String l_strName;

		_merge<SceneNodePtrStrMap>( p_pScene, p_pScene->m_addedNodes, m_addedNodes);
		_merge<LightPtrStrMap>( p_pScene, p_pScene->m_addedLights, m_addedLights);
		_merge<CameraPtrStrMap>( p_pScene, p_pScene->m_addedCameras, m_addedCameras);
		_merge<GeometryPtrStrMap>( p_pScene, p_pScene->m_addedPrimitives, m_addedPrimitives);
		_merge<GeometryPtrStrMap>( p_pScene, p_pScene->m_newlyAddedPrimitives, m_newlyAddedPrimitives);
		m_changed = true;

		m_clAmbientLight = p_pScene->GetAmbientLight();
	}

	p_pScene->ClearScene();
}

bool Scene :: ImportExternal( const String & p_fileName, Importer * p_importer)
{
	CASTOR_MUTEX_AUTO_SCOPED_LOCK();
	bool l_bReturn = true;

	if (p_importer->Import( p_fileName))
	{
		for (GeometryPtrStrMap::iterator l_it = p_importer->m_geometries.begin() ; l_it != p_importer->m_geometries.end() ; ++l_it)
		{
			if (m_addedPrimitives.find( l_it->first) == m_addedPrimitives.end())
			{
				m_addedPrimitives.insert( GeometryPtrStrMap::value_type( l_it->first, l_it->second));
				m_newlyAddedPrimitives.insert( GeometryPtrStrMap::value_type( l_it->first, l_it->second));
				Logger::LogMessage( CU_T( "_importExternal - Geometry %s added"), l_it->first.char_str());
			}
			else
			{
				l_it->second.reset();
			}
		}

		m_changed = true;
		l_bReturn = true;
	}

	return l_bReturn;
}

void Scene :: Select( Ray * p_ray, GeometryPtr & p_geo, SubmeshPtr & p_submesh, FacePtr * p_face, Vertex * p_vertex)
{
	CASTOR_MUTEX_AUTO_SCOPED_LOCK();
	Point3r l_min;
	Point3r l_max;
	GeometryPtr l_geo, l_selectedGeo;
	MeshPtr l_mesh;
	SubmeshPtr l_submesh, l_selectedSubmesh;
	unsigned int l_nbSubmeshes;
	SmoothGroupPtrUIntMap::const_iterator l_itGroupsEnd;
	FacePtr l_face, l_selectedFace;
	Vertex l_selectedVertex;
	real l_geoDist = 10e6, l_faceDist = 10e6, l_vertexDist = 10e6;
	real l_curgeoDist, l_curfaceDist, l_curvertexDist;
	CubeBox l_box;
	SphereBox l_sphere;
	SmoothingGroupPtr l_group;

	for (GeometryPtrStrMap::iterator l_it = m_addedPrimitives.begin() ; l_it != m_addedPrimitives.end() ; ++l_it)
	{
		l_geo = l_it->second;

		if (l_geo->IsVisible())
		{
			l_mesh = l_geo->GetMesh();
			l_sphere = l_mesh->GetSphere();

			if ((l_curgeoDist = p_ray->Intersects( l_sphere)) > 0.0f)
			{
				l_box = l_mesh->GetCubeBox();

				if (p_ray->Intersects( l_box) > 0.0f)
				{
					l_geoDist = l_curgeoDist;
					l_nbSubmeshes = l_mesh->GetNbSubmeshes();

					for (unsigned int i = 0 ; i < l_nbSubmeshes ; i++)
					{
						l_submesh = l_mesh->GetSubmesh( i);
						l_sphere = l_submesh->GetSphere();

						if (p_ray->Intersects( l_sphere) >= 0.0f)
						{
							l_box = l_submesh->GetCubeBox();

							if (p_ray->Intersects( l_box) >= 0.0f)
							{
								for (size_t i = 0 ; i < l_submesh->GetNbSmoothGroups() ; i++)
								{
									l_group = l_submesh->GetSmoothGroup( i);

									for (size_t k = 0 ; k < l_group->GetNbFaces() ; k++)
									{
										l_face = l_group->GetFace( k);

										if ((l_curfaceDist = p_ray->Intersects( *l_face)) < l_faceDist)
										{
											if ((l_curvertexDist = p_ray->Intersects( l_face->GetVertex( 0).GetCoords())) < l_vertexDist)
											{
												l_vertexDist = l_curvertexDist;
												l_geoDist = l_curgeoDist;
												l_faceDist = l_curfaceDist;
												l_selectedGeo = l_geo;
												l_selectedSubmesh = l_submesh;
												l_selectedFace = l_face;
												l_selectedVertex = l_face->GetVertex( 0);
											}

											if ((l_curvertexDist = p_ray->Intersects( l_face->GetVertex( 1).GetCoords())) < l_vertexDist)
											{
												l_vertexDist = l_curvertexDist;
												l_geoDist = l_curgeoDist;
												l_faceDist = l_curfaceDist;
												l_selectedGeo = l_geo;
												l_selectedSubmesh = l_submesh;
												l_selectedFace = l_face;
												l_selectedVertex = l_face->GetVertex( 1);
											}

											if ((l_curvertexDist = p_ray->Intersects( l_face->GetVertex( 2).GetCoords())) < l_vertexDist)
											{
												l_vertexDist = l_curvertexDist;
												l_geoDist = l_curgeoDist;
												l_faceDist = l_curfaceDist;
												l_selectedGeo = l_geo;
												l_selectedSubmesh = l_submesh;
												l_selectedFace = l_face;
												l_selectedVertex = l_face->GetVertex( 2);
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if (p_geo != NULL)
	{
		p_geo = l_selectedGeo;
	}

	if (p_submesh != NULL)
	{
		p_submesh = l_selectedSubmesh;
	}

	if (p_face != NULL)
	{
		p_face = & l_selectedFace;
	}

	if (p_vertex != NULL)
	{
		p_vertex = & l_selectedVertex;
	}
}

void Scene :: _deleteToDelete()
{
	CASTOR_MUTEX_AUTO_SCOPED_LOCK();
	for (size_t i = 0 ; i < m_arrayLightsToDelete.size() ; i++)
	{
//		delete m_arrayLightsToDelete[i];
		m_arrayLightsToDelete[i].reset();
	}

	m_arrayLightsToDelete.clear();

	for (size_t i = 0 ; i < m_arrayPrimitivesToDelete.size() ; i++)
	{
		m_arrayPrimitivesToDelete[i]->Detach();
//		delete m_arrayPrimitivesToDelete[i];
		m_arrayPrimitivesToDelete[i].reset();
	}

	m_arrayPrimitivesToDelete.clear();
	
	for (size_t i = 0 ; i < m_arrayNodesToDelete.size() ; i++)
	{
		m_arrayNodesToDelete[i].reset();
//		delete m_arrayNodesToDelete[i];
	}

	m_arrayNodesToDelete.clear();

	for (size_t i = 0 ; i < m_arrayCamerasToDelete.size() ; i++)
	{
//		delete m_arrayCamerasToDelete[i];
		m_arrayCamerasToDelete[i].reset();
	}

	m_arrayCamerasToDelete.clear();
}

bool Scene :: _writeLights( File & p_file)const
{
	CASTOR_MUTEX_AUTO_SCOPED_LOCK();
	bool l_bReturn = true;
	size_t l_nbLights = m_addedLights.size();

	LightPtrStrMap::const_iterator l_it = m_addedLights.begin();

	while (l_bReturn && l_it != m_addedLights.end())
	{
		l_bReturn = l_it->second->Write( p_file);
		++l_it;
	}

	return l_bReturn;
}

bool Scene :: _writeGeometries( Castor::Utils::File & p_file)const
{
	CASTOR_MUTEX_AUTO_SCOPED_LOCK();
	bool l_bReturn = true;
	size_t l_nbGeometries = m_addedPrimitives.size();

	GeometryPtrStrMap::const_iterator l_it = m_addedPrimitives.begin();

	while (l_bReturn && l_it != m_addedPrimitives.end())
	{
		l_bReturn = l_it->second->Write( p_file);
		++l_it;
	}

	return l_bReturn;
}

bool Scene :: _saveLights( File & p_file)const
{
	CASTOR_MUTEX_AUTO_SCOPED_LOCK();
	bool l_bReturn = p_file.Write( String( CU_T( "Lights")));

	if (l_bReturn)
	{
		l_bReturn = p_file.Write( m_addedLights.size()) == sizeof( size_t);
	}

	LightPtrStrMap::const_iterator l_it = m_addedLights.begin();

	while (l_bReturn && l_it != m_addedLights.end())
	{
		l_bReturn = l_it->second->Save( p_file);
		++l_it;
	}

	return l_bReturn;
}

bool Scene :: _loadLights( File & p_file)
{
	CASTOR_MUTEX_AUTO_SCOPED_LOCK();
	size_t l_uiSize;
	bool l_bReturn = p_file.Read( l_uiSize) == sizeof( size_t);
	Light::eLIGHT_TYPE l_eLightType;
	LightPtr l_pLight;
	String l_strName;

	for (size_t i = 0 ; l_bReturn && i < l_uiSize ; i++)
	{
		l_bReturn = p_file.Read( l_eLightType) == sizeof( Light::eLIGHT_TYPE);

		if (l_bReturn)
		{
			l_bReturn = p_file.Read( l_strName);
		}

		if (l_bReturn)
		{
			switch (l_eLightType)
			{
			case Light::eDirectional:
				l_pLight = CreateLight<DirectionalLight>( l_strName, m_rootNode);
				break;

			case Light::ePoint:
				l_pLight = CreateLight<PointLight>( l_strName, m_rootNode);
				break;

			case Light::eSpot:
				l_pLight = CreateLight<SpotLight>( l_strName, m_rootNode);
				break;

			default:
				l_pLight.reset();
				l_bReturn = false;
			}
		}

		if (l_bReturn)
		{
			l_bReturn = l_pLight->Save( p_file);
		}
	}

	return l_bReturn;
}

bool Scene :: _saveGeometries( File & p_file)const
{
	CASTOR_MUTEX_AUTO_SCOPED_LOCK();
	bool l_bReturn = p_file.Write( String( CU_T( "Geometries")));

	if (l_bReturn)
	{
		l_bReturn = p_file.Write( m_addedPrimitives.size()) == sizeof( size_t);
	}

	GeometryPtrStrMap::const_iterator l_it = m_addedPrimitives.begin();

	while (l_bReturn && l_it != m_addedPrimitives.end())
	{
		l_bReturn = l_it->second->Save( p_file);
		++l_it;
	}

	return l_bReturn;
}

bool Scene :: _loadGeometries( File & p_file)
{
	CASTOR_MUTEX_AUTO_SCOPED_LOCK();
	size_t l_uiSize;
	bool l_bReturn = p_file.Read( l_uiSize) == sizeof( size_t);
	GeometryPtr l_pGeometry;
	String l_strName;

	for (size_t i = 0 ; l_bReturn && i < l_uiSize ; i++)
	{
		if (l_bReturn)
		{
			l_bReturn = p_file.Read( l_strName);
		}

		if (l_bReturn)
		{
			l_pGeometry = CreatePrimitive( l_strName);
			l_bReturn = l_pGeometry->Save( p_file);
		}
	}

	return l_bReturn;
}