#include "Dx9RenderSystem/PrecompiledHeader.h"

#include "Dx9RenderSystem/Dx9LightRenderer.h"
#include "Dx9RenderSystem/Dx9RenderSystem.h"

using namespace Castor3D;

Dx9LightRenderer :: Dx9LightRenderer( SceneManager * p_pSceneManager)
	:	LightRenderer( p_pSceneManager)
	,	m_iIndex( GL_INVALID_INDEX)
{
	m_pfnEnableFunction			= & Dx9LightRenderer::_enable;
	m_pfnDisableFunction		= & Dx9LightRenderer::_disable;
	m_pfnAmbientFunction		= & Dx9LightRenderer::_applyAmbient;
	m_pfnDiffuseFunction		= & Dx9LightRenderer::_applyDiffuse;
	m_pfnSpecularFunction		= & Dx9LightRenderer::_applySpecular;
	m_pfnOrientationFunction	= & Dx9LightRenderer::_applyOrientation;
	m_pfnPositionFunction		= & Dx9LightRenderer::_applyPosition;
	m_pfnConstantAttFunction	= & Dx9LightRenderer::_applyConstantAtt;
	m_pfnLinearAttFunction		= & Dx9LightRenderer::_applyLinearAtt;
	m_pfnQuadraticAttFunction	= & Dx9LightRenderer::_applyQuadraticAtt;
	m_pfnExponentFunction		= & Dx9LightRenderer::_applyExponent;
	m_pfnCutOffFunction			= & Dx9LightRenderer::_applyCutOff;

	memset( & m_dx9Light, 0, sizeof( D3DLIGHT9));
	m_dx9Light.Range	= 1000.0f;
}

void Dx9LightRenderer :: Initialise()
{
	m_pfnEnableFunction			= & Dx9LightRenderer::_enable;
	m_pfnDisableFunction		= & Dx9LightRenderer::_disable;
	m_pfnAmbientFunction		= & Dx9LightRenderer::_applyAmbient;
	m_pfnDiffuseFunction		= & Dx9LightRenderer::_applyDiffuse;
	m_pfnSpecularFunction		= & Dx9LightRenderer::_applySpecular;
	m_pfnOrientationFunction	= & Dx9LightRenderer::_applyOrientation;
	m_pfnPositionFunction		= & Dx9LightRenderer::_applyPosition;
	m_pfnConstantAttFunction	= & Dx9LightRenderer::_applyConstantAtt;
	m_pfnLinearAttFunction		= & Dx9LightRenderer::_applyLinearAtt;
	m_pfnQuadraticAttFunction	= & Dx9LightRenderer::_applyQuadraticAtt;
	m_pfnExponentFunction		= & Dx9LightRenderer::_applyExponent;
	m_pfnCutOffFunction			= & Dx9LightRenderer::_applyCutOff;
}

void Dx9LightRenderer :: Enable()
{
	m_iIndex = RenderSystem::GetSingletonPtr()->LockLight();

	if (m_iIndex >= 0)
	{
		(this->*m_pfnEnableFunction)();
	}
}

void Dx9LightRenderer :: Disable()
{
	RenderSystem::GetSingletonPtr()->UnlockLight( m_iIndex);

	if (m_iIndex >= 0)
	{
		(this->*m_pfnDisableFunction)();
	}
}

void Dx9LightRenderer :: ApplyAmbient()
{
	if (m_iIndex >= 0)
	{
		(this->*m_pfnAmbientFunction)( m_target->GetAmbient());
	}
}

void Dx9LightRenderer :: ApplyDiffuse()
{
	if (m_iIndex >= 0)
	{
		(this->*m_pfnDiffuseFunction)( m_target->GetDiffuse());
	}
}

void Dx9LightRenderer :: ApplySpecular()
{
	if (m_iIndex >= 0)
	{
		(this->*m_pfnSpecularFunction)( m_target->GetSpecular());
	}
}

void Dx9LightRenderer :: ApplyPosition()
{
	if (m_iIndex >= 0)
	{
		(this->*m_pfnPositionFunction)( Point4f( float( m_target->GetPositionType()[0]), float( m_target->GetPositionType()[1]), float( m_target->GetPositionType()[2]), float( m_target->GetPositionType()[3])));
	}
}

void Dx9LightRenderer :: ApplyOrientation()
{
	if (m_iIndex >= 0)
	{
		(this->*m_pfnOrientationFunction)( m_target->GetParent()->GetRotationMatrix());
	}
}

void Dx9LightRenderer :: ApplyConstantAtt( float p_constant)
{
	if (m_iIndex >= 0)
	{
		(this->*m_pfnConstantAttFunction)( p_constant);
	}
}

void Dx9LightRenderer :: ApplyLinearAtt( float p_linear)
{
	if (m_iIndex >= 0)
	{
		(this->*m_pfnLinearAttFunction)( p_linear);
	}
}

void Dx9LightRenderer :: ApplyQuadraticAtt( float p_quadratic)
{
	if (m_iIndex >= 0)
	{
		(this->*m_pfnQuadraticAttFunction)( p_quadratic);
	}
}

void Dx9LightRenderer :: ApplyExponent( float p_fExponent)
{
	if (m_iIndex >= 0)
	{
		(this->*m_pfnExponentFunction)( p_fExponent);
	}
}

void Dx9LightRenderer :: ApplyCutOff( float p_fCutOff)
{
	if (m_iIndex >= 0)
	{
		(this->*m_pfnCutOffFunction)( p_fCutOff);
	}
}

void Dx9LightRenderer :: _enable()
{
	CheckDxError( Dx9RenderSystem::GetDevice()->SetLight( m_iIndex, & m_dx9Light), CU_T( "Dx9LightRenderer :: _enable - SetLight"), false);
	CheckDxError( Dx9RenderSystem::GetDevice()->LightEnable( m_iIndex, TRUE), CU_T( "Dx9LightRenderer :: _enable - LightEnable"), false);
}

void Dx9LightRenderer :: _disable()
{
	CheckDxError( Dx9RenderSystem::GetDevice()->SetLight( m_iIndex, & m_dx9Light), CU_T( "Dx9LightRenderer :: _disable - SetLight"), false);
	CheckDxError( Dx9RenderSystem::GetDevice()->LightEnable( m_iIndex, FALSE), CU_T( "Dx9LightRenderer :: _disable - LightEnable"), false);
	m_iIndex = -1;
}

void Dx9LightRenderer :: _applyAmbient( const Colour & p_ambient)
{
	memcpy( & m_dx9Light.Ambient, p_ambient.const_ptr(), 4 * sizeof( float));
}

void Dx9LightRenderer :: _applyDiffuse( const Colour & p_diffuse)
{
	memcpy( & m_dx9Light.Diffuse, p_diffuse.const_ptr(), 4 * sizeof( float));
}

void Dx9LightRenderer :: _applySpecular( const Colour & p_specular)
{
	memcpy( & m_dx9Light.Specular, p_specular.const_ptr(), 4 * sizeof( float));
}

void Dx9LightRenderer :: _applyPosition( const Point4f & p_position)
{
	m_dx9Light.Type = D3dEnum::Get( m_target->GetLightType());

	if (m_target->GetLightType() == Light::eDirectional)
	{
		memcpy( & m_dx9Light.Direction, p_position.GetNormalised().const_ptr(), 3 * sizeof( float));
	}
	else
	{
		memcpy( & m_dx9Light.Position, p_position.const_ptr(), 3 * sizeof( float));
	}
}

void Dx9LightRenderer :: _applyOrientation( const Matrix4x4r & p_orientation)
{
	Point3r l_ptDirection;
	MtxUtils::mult( p_orientation, l_ptDirection);
	memcpy( & m_dx9Light.Direction, l_ptDirection.const_ptr(), 3 * sizeof( float));
}

void Dx9LightRenderer :: _applyConstantAtt( float p_constant)
{
	m_dx9Light.Attenuation0 = p_constant;
}

void Dx9LightRenderer :: _applyLinearAtt( float p_linear)
{
	m_dx9Light.Attenuation1 = p_linear;
}

void Dx9LightRenderer :: _applyQuadraticAtt( float p_quadratic)
{
	m_dx9Light.Attenuation2 = p_quadratic;
}

void Dx9LightRenderer :: _applyExponent( float p_exponent)
{
	m_dx9Light.Falloff = p_exponent;
}

void Dx9LightRenderer :: _applyCutOff( float p_cutOff)
{
	m_dx9Light.Theta = p_cutOff;
}