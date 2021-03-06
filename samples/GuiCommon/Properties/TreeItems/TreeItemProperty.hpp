/*
See LICENSE file in root folder
*/
#ifndef ___GUICOMMON_TREE_ITEM_PROPERTY_DATA_H___
#define ___GUICOMMON_TREE_ITEM_PROPERTY_DATA_H___

#include "GuiCommon/GuiCommonPrerequisites.hpp"

#include "GuiCommon/Properties/Math/MatrixProperties.hpp"
#include "GuiCommon/Properties/Math/PointProperties.hpp"
#include "GuiCommon/Properties/Math/PositionProperties.hpp"
#include "GuiCommon/Properties/Math/QuaternionProperties.hpp"
#include "GuiCommon/Properties/Math/RectangleProperties.hpp"
#include "GuiCommon/Properties/Math/SizeProperties.hpp"

#include <Castor3D/Render/RenderModule.hpp>

#include <CastorUtils/Graphics/Font.hpp>
#include <CastorUtils/Math/RangedValue.hpp>

#include <wx/treectrl.h>
#include <wx/propgrid/propgrid.h>
#include <wx/propgrid/advprops.h>

namespace GuiCommon
{
	/**
	\version	0.8.0
	\brief		The supported object types, for display, and properties
	*/
	typedef enum ePROPERTY_DATA_TYPE
	{
		ePROPERTY_DATA_TYPE_SCENE,
		ePROPERTY_DATA_TYPE_RENDER_TARGET,
		ePROPERTY_DATA_TYPE_VIEWPORT,
		ePROPERTY_DATA_TYPE_RENDER_WINDOW,
		ePROPERTY_DATA_TYPE_BILLBOARD,
		ePROPERTY_DATA_TYPE_CAMERA,
		ePROPERTY_DATA_TYPE_GEOMETRY,
		ePROPERTY_DATA_TYPE_SUBMESH,
		ePROPERTY_DATA_TYPE_LIGHT,
		ePROPERTY_DATA_TYPE_NODE,
		ePROPERTY_DATA_TYPE_OVERLAY,
		ePROPERTY_DATA_TYPE_MATERIAL,
		ePROPERTY_DATA_TYPE_PASS,
		ePROPERTY_DATA_TYPE_TEXTURE,
		ePROPERTY_DATA_TYPE_ANIMATED_OBJECT_GROUP,
		ePROPERTY_DATA_TYPE_ANIMATED_OBJECT,
		ePROPERTY_DATA_TYPE_ANIMATION,
		ePROPERTY_DATA_TYPE_POST_EFFECT,
		ePROPERTY_DATA_TYPE_TONE_MAPPING,
		ePROPERTY_DATA_TYPE_BONE,
		ePROPERTY_DATA_TYPE_SKELETON,
		ePROPERTY_DATA_TYPE_SKELETON_ANIMATION,
		ePROPERTY_DATA_TYPE_BACKGROUND,
	}	ePROPERTY_DATA_TYPE;
	/**
	\version	0.8.0
	\brief		Helper class to communicate between Scene objects or Materials lists and PropertiesContainer
	*/
	class TreeItemProperty
		: public wxTreeItemData
	{
	public:
		/**
		 *\~english
		 *\param[in]	engine	The engine, to post events to.
		 *\param[in]	editable	Tells if the properties are modifiable
		 *\param[in]	type		The object type
		 */
		TreeItemProperty( castor3d::Engine * engine
			, bool editable
			, ePROPERTY_DATA_TYPE type );
		/**
		 *\~english
		 *\brief		Destructor
		 *\~french
		 *\brief		Destructeur
		 */
		virtual ~TreeItemProperty();
		/**
		 *\brief		Displays the wxTree item menu, at given coordinates
		 *\param[in]	window	The wxWindow that displays the menu
		 *\param[in]	x, y	The coordinates
		 */
		void DisplayTreeItemMenu( wxWindow * window, wxCoord x, wxCoord y );
		/**
		 *\~english
		 *\brief		Creates and fills the item properties, in the given wxPropertyGrid
		 *\param[in]	editor	The button editor, for properties that need it
		 *\param[in]	grid	The target wxPropertyGrid
		 */
		void CreateProperties( wxPGEditor * editor, wxPropertyGrid * grid );
		/**
		 *\brief		Call when a property grid property is changed
		 *\param[in]	event	The event
		 */
		void onPropertyChange( wxPropertyGridEvent & event );
		/**
		 *\brief		Retrieves the object type
		 *\return		The value
		 */
		inline ePROPERTY_DATA_TYPE getType()const
		{
			return m_type;
		}
		/**
		 *\brief		Retrieves the editable status
		 *\return		The value
		 */
		inline bool IsEditable()const
		{
			return m_editable;
		}

	protected:
		/**
		 *\return		The materials names.
		 */
		wxArrayString getMaterialsList();
		/**
		 *\brief		Creates the menu displayed for the wxTree item, available if m_editable is true
		 */
		void CreateTreeItemMenu();
		/**
		 *\brief		Creates the menu displayed for the wxTree item, available if m_editable is true
		 */
		virtual void doCreateTreeItemMenu();
		/**
		 *\brief		Creates and fills the overlay properties, in the given wxPropertyGrid
		 *\param[in]	editor	The button editor, for properties that need it
		 *\param[in]	grid	The target wxPropertyGrid
		 */
		virtual void doCreateProperties( wxPGEditor * editor, wxPropertyGrid * grid ) = 0;

	public:
		using PropertyChangeHandler = std::function< void ( wxVariant const & ) >;
		static PropertyChangeHandler const EmptyHandler;

		template< typename ObjectT, typename ValueT >
		using ValueRefSetterT = void ( ObjectT:: * )( ValueT const & );

		template< typename ObjectT, typename ValueT >
		using ValueSetterT = void ( ObjectT:: * )( ValueT );

		wxPGProperty * addProperty( wxPropertyGrid * grid
			, wxString const & name );
		template< typename MyValueT >
		wxPGProperty * createProperty( wxPropertyGrid * grid
			, wxString const & name
			, MyValueT && value
			, PropertyChangeHandler handler );
		template< typename EnumT, typename FuncT >
		wxPGProperty * addPropertyE( wxPropertyGrid * grid
			, wxString const & name
			, wxArrayString const & choices
			, FuncT func );
		template< typename EnumT, typename FuncT >
		wxPGProperty * addPropertyE( wxPropertyGrid * grid
			, wxString const & name
			, wxArrayString const & choices
			, EnumT selected
			, FuncT func );
		wxPGProperty * addProperty( wxPropertyGrid * grid
			, wxString const & name
			, wxArrayString const & choices
			, wxString const & selected
			, PropertyChangeHandler handler );
		template< typename ValueT >
		wxPGProperty * addProperty( wxPropertyGrid * grid
			, wxString const & name
			, ValueT const & value
			, PropertyChangeHandler handler );
		template< typename ValueT >
		wxPGProperty * addProperty( wxPropertyGrid * grid
			, wxString const & name
			, ValueT const & value
			, ValueT const & step
			, PropertyChangeHandler handler );
		wxPGProperty * addProperty( wxPropertyGrid * grid
			, wxString const & name
			, wxPGEditor * editor
			, PropertyChangeHandler handler );

		template< typename ValueT >
		wxPGProperty * addPropertyT( wxPropertyGrid * grid
			, wxString const & name
			, castor::RangedValue< ValueT > * value );
		template< typename ValueT >
		wxPGProperty * addPropertyT( wxPropertyGrid * grid
			, wxString const & name
			, castor::ChangeTracked< castor::RangedValue< ValueT > > * value );
		template< typename ValueT >
		wxPGProperty * addPropertyT( wxPropertyGrid * grid
			, wxString const & name
			, ValueT * value );
		template< typename ValueT >
		wxPGProperty * addPropertyT( wxPropertyGrid * grid
			, wxString const & name
			, ValueT * value
			, ValueT step );
		template< typename ObjectT, typename ObjectU, typename ValueT >
		wxPGProperty * addPropertyT( wxPropertyGrid * grid
			, wxString const & name
			, ValueT value
			, ObjectT * object
			, ValueSetterT< ObjectU, ValueT > setter );
		template< typename ObjectT, typename ObjectU, typename ValueT >
		wxPGProperty * addPropertyT( wxPropertyGrid * grid
			, wxString const & name
			, ValueT  value
			, ValueT step
			, ObjectT * object
			, ValueSetterT< ObjectU, ValueT > setter );
		template< typename ObjectT, typename ObjectU, typename ValueT >
		wxPGProperty * addPropertyT( wxPropertyGrid * grid
			, wxString const & name
			, ValueT const & value
			, ObjectT * object
			, ValueRefSetterT< ObjectU, ValueT > setter );
		template< typename ObjectT, typename ObjectU, typename ValueT >
		wxPGProperty * addPropertyT( wxPropertyGrid * grid
			, wxString const & name
			, castor::RangedValue< ValueT > const & value
			, ObjectT * object
			, ValueSetterT< ObjectU, ValueT > setter );
		template< typename ObjectT, typename ObjectU, typename EnumT >
		wxPGProperty * addPropertyET( wxPropertyGrid * grid
			, wxString const & name
			, wxArrayString const & choices
			, ObjectT * object
			, ValueSetterT< ObjectU, EnumT > setter );
		template< typename ObjectT, typename ObjectU, typename EnumT >
		wxPGProperty * addPropertyET( wxPropertyGrid * grid
			, wxString const & name
			, wxArrayString const & choices
			, EnumT selected
			, ObjectT * object
			, ValueSetterT< ObjectU, EnumT > setter );
		template< typename EnumT >
		wxPGProperty * addPropertyET( wxPropertyGrid * grid
			, wxString const & name
			, wxArrayString const & choices
			, EnumT * value );
		template< typename ObjectT, typename ObjectU, typename EnumT >
		wxPGProperty * addPropertyT( wxPropertyGrid * grid
			, wxString const & name
			, wxArrayString const & choices
			, wxString const & selected
			, ObjectT * object
			, ValueSetterT< ObjectU, EnumT > setter );

	protected:
		wxMenu * m_menu;

	private:
		ePROPERTY_DATA_TYPE m_type;
		bool m_editable;
		castor3d::Engine * m_engine;
		std::map< wxString, PropertyChangeHandler > m_handlers;
	};
}

#include "TreeItemProperty.inl"

#endif
