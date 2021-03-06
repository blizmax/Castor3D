/*
See LICENSE file in root folder
*/
#ifndef ___C3D_FrameEventModule_H___
#define ___C3D_FrameEventModule_H___

#include "Castor3D/Castor3DModule.hpp"

namespace castor3d
{
	/**@name Event */
	//@{
	/**@name Frame */
	//@{

	/**
	*\~english
	*\brief
	*	Frame Event Type enumeration
	*\~french
	*\brief
	*	Enumération des types d'évènement de frame
	*/
	enum class EventType
		: uint8_t
	{
		//!\~english	This kind of event happens before any render, device context is active (so be fast !!).
		//!\~french		Ce type d'évènement est traité avant le rendu, le contexte de rendu est actif (donc soyez rapide !!)
		ePreRender,
		//!\~english	This kind of event happens after the render, before buffers' swap.
		//!\~french		Ce type d'évènement est traité après le rendu, avant l'échange des tampons.
		eQueueRender,
		//!\~english	This kind of event happens after the buffer' swap.
		//!\~french		Ce type d'évènement est traité après l'échange des tampons.
		ePostRender,
		CU_ScopedEnumBounds( ePreRender )
	};
	C3D_API castor::String getName( EventType value );
	/**
	*\~english
	*\brief
	*	The interface which represents a frame event.
	*\remarks
	*	Basically a frame event has a EventType to know when it must be applied.
	*	<br />It can be applied, so the function must be implemented by children classes
	*\~french
	*\brief
	*	Interface représentant un évènement de frame
	*\remarks
	*	Un évènement a un EventType pour savoir quand il doit être traité.
	*	<br />La fonction de traitement doit être implémentée par les classes filles.
	*/
	class CpuFrameEvent;
	/**
	*\~english
	*\brief
	*	Functor event
	*\remarks
	*	Executes a function when processed
	*\~french
	*\brief
	*	Evènement foncteur
	*\remarks
	*	Excécute une fonction lorsqu'il est traité
	*/
	class CpuFunctorEvent;
	/**
	*\~english
	*\brief
	*	User event synchronisation class.
	*\remarks
	*	The handler of the frame events. It can add frame events and applies them at the wanted times.
	*\~french
	*\brief
	*	Classe de synchronisation des évènements.
	*\remarks
	*	Le gestionnaire des évènements de frame, on peut y ajouter des évènements à traiter, qui le seront au moment voulu (en fonction de leur EventType).
	*/
	class FrameListener;
	/**
	*\~english
	*\brief
	*	The interface which represents a frame event.
	*\remarks
	*	Basically a frame event has a EventType to know when it must be applied.
	*	<br />It can be applied, so the function must be implemented by children classes
	*\~french
	*\brief
	*	Interface représentant un évènement de frame
	*\remarks
	*	Un évènement a un EventType pour savoir quand il doit être traité.
	*	<br />La fonction de traitement doit être implémentée par les classes filles.
	*/
	class GpuFrameEvent;
	/**
	*\~english
	*\brief
	*	Functor event
	*\remarks
	*	Executes a function when processed
	*\~french
	*\brief
	*	Evènement foncteur
	*\remarks
	*	Excécute une fonction lorsqu'il est traité
	*/
	class GpuFunctorEvent;

	CU_DeclareSmartPtr( CpuFrameEvent );
	CU_DeclareSmartPtr( FrameListener );
	CU_DeclareSmartPtr( GpuFrameEvent );
	CU_DeclareSmartPtr( CpuFunctorEvent );
	CU_DeclareSmartPtr( GpuFunctorEvent );

	CU_DeclareVector( CpuFrameEventUPtr, CpuFrameEventPtr );
	CU_DeclareVector( GpuFrameEventUPtr, GpuFrameEventPtr );

	CU_DeclareMap( castor::String, FrameListenerSPtr, FrameListenerPtrStr );

	//@}
	//@}
}

#endif
