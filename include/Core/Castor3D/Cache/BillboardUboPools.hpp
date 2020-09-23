/*
See LICENSE file in root folder
*/
#ifndef ___C3D_BillboardUboPools_H___
#define ___C3D_BillboardUboPools_H___

#include "CacheModule.hpp"

#include "Castor3D/Buffer/UniformBufferOffset.hpp"
#include "Castor3D/Shader/Ubos/UbosModule.hpp"

namespace castor3d
{
	class BillboardUboPools
	{
	public:
		struct PoolsEntry
		{
			BillboardBase const & billboard;
			Pass const & pass;
			UniformBufferOffsetT< ModelUboConfiguration > modelUbo;
			UniformBufferOffsetT< ModelMatrixUboConfiguration > modelMatrixUbo;
			UniformBufferOffsetT< BillboardUboConfiguration > billboardUbo;
			UniformBufferOffsetT< PickingUboConfiguration > pickingUbo;
			UniformBufferOffsetT< TexturesUboConfiguration > texturesUbo;
		};

	public:
		/**
		 *\~english
		 *\brief		Constructor.
		 *\param[in]	renderSystem	The RenderSystem.
		 *\~french
		 *\brief		Constructeur.
		 *\param[in]	renderSystem	Le RenderSystem.
		 */
		C3D_API explicit BillboardUboPools( RenderSystem & renderSystem );
		/**
		 *\~english
		 *\brief		Updates the UBO pools data.
		 *\~french
		 *\brief		Met à jour le contenu des pools d'UBO.
		 */
		C3D_API void update();
		/**
		 *\~english
		 *\brief		Register the entries for given billboard.
		 *\~french
		 *\brief		Enregistre les entrées pour le billboard donné.
		 */
		void registerElement( BillboardBase & billboard );
		/**
		 *\~english
		 *\brief		Removes the entries for given billboard.
		 *\~french
		 *\brief		Supprime les entrées pour le billboard donné.
		 */
		void unregisterElement( BillboardBase & billboard );
		/**
		 *\~english
		 *\brief		Cleans up the UBO pools.
		 *\remarks		Assumes the entry has been previously created.
		 *\~french
		 *\brief		Nettoie les pools d'UBO.
		 *\remarks		Considère que l'entrée a été préalablement créée.
		 */
		C3D_API PoolsEntry getUbos( BillboardBase const & billboard, Pass const & pass )const;
		/**
		 *\~english
		 *\brief		Flushes the pools.
		 *\~french
		 *\brief		Vide les pools.
		 */
		C3D_API void clear();

	private:
		PoolsEntry doCreateEntry( BillboardBase const & billboard
			, Pass const & pass );
		void doRemoveEntry( BillboardBase const & billboard
			, Pass const & pass );

	private:
		RenderSystem & m_renderSystem;
		std::map< size_t, PoolsEntry > m_entries;
		std::map< BillboardBase *, OnBillboardMaterialChangedConnection > m_connections;
		RenderPassTimerSPtr m_updateTimer;
	};
}

#endif