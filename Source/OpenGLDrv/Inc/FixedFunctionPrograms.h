/*=============================================================================
	FixedFunctionPrograms.h: the GLSL programs that draw the passes which carry
	a lightmap, with the lightmap read through a cubic B-spline filter.
	Shared by OpenGLDrv and GL4ESDrv (GL4ESDrv is OpenGLDrv's renamed fork):
	each driver includes it after its GL headers.

	A program does what the pass's texture stages do as GL_COMBINE texture
	environments (ARB_texture_env_combine with ATI_texture_env_combine3's
	operations): the stages' texture coordinates, generation modes and
	matrices, the primary color and linear distance fog. The lightmap stage
	reads its texture as a cubic B-spline made of four bilinear taps (GPU
	Gems 2, chapter 20). The position is ftransform(), the fixed-function
	transform, so a program pass and a fixed-function pass over the same
	surface have the same depth.
=============================================================================*/

#ifndef HEADER_FIXEDFUNCTIONPROGRAMS
#define HEADER_FIXEDFUNCTIONPROGRAMS

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER				0x8B30
#endif
#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER				0x8B31
#endif
#ifndef GL_COMPILE_STATUS
#define GL_COMPILE_STATUS				0x8B81
#endif
#ifndef GL_LINK_STATUS
#define GL_LINK_STATUS					0x8B82
#endif
#ifndef GL_HIGH_FLOAT
#define GL_HIGH_FLOAT					0x8DF2
#endif
#ifndef GL_MODULATE_SIGNED_ADD_ATI
#define GL_MODULATE_SIGNED_ADD_ATI		0x8745
#endif
#ifndef GL_MODULATE_SUBTRACT_ATI
#define GL_MODULATE_SUBTRACT_ATI		0x8746
#endif

#define FF_MAX_STAGES 8

enum EFFTarget
{
	FFT_2D		= 1,
	FFT_Cube	= 2,
};

//
//	FFFStageKey: what one texture stage of a pass does, as the program reproduces it.
//
struct FFFStageKey
{
	DWORD	ColorOp,
			AlphaOp;
	DWORD	ColorSource[3],		// GL_SOURCE0..2_RGB
			AlphaSource[3];		// GL_SOURCE0..2_ALPHA
	DWORD	ColorOperand[3],	// GL_OPERAND0..2_RGB
			AlphaOperand[3];	// GL_OPERAND0..2_ALPHA
	DWORD	TexGenMode;			// GL_OBJECT_LINEAR, GL_REFLECTION_MAP, GL_NORMAL_MAP; 0 = none
	FLOAT	LodBias;			// added by the lookup itself (FFFProgramOptions::ShaderLodBias)
	BYTE	ColorScale,
			AlphaScale,
			Target,				// EFFTarget
			TexGenCount,		// generated components, from s
			Bicubic,			// the lightmap stage: cubic B-spline read
			Pad[3];
};

//
//	FFFProgramKey: a pass, as the program reproduces it. Zero-filled before it is set, so it compares as bytes.
//
struct FFFProgramKey
{
	BYTE		NumStages,
				Fog;			// linear distance fog (GL_FOG on)
	BYTE		Pad[2];
	FFFStageKey	Stages[FF_MAX_STAGES];
};

//
//	FFFProgramOptions: what differs between the drivers' GL implementations.
//
struct FFFProgramOptions
{
	// The source goes through gl4es's converter: no #version or #extension lines (gl4es writes its own).
	UBOOL	Gl4es;
	// The lookups add the stages' LOD bias themselves (gl4es does not apply a unit's GL_TEXTURE_LOD_BIAS
	// to a GLSL program; desktop GL does).
	UBOOL	ShaderLodBias;
	// A reflection-map stage reflects per fragment, from the interpolated eye position and normal, as
	// gl4es's own fixed-function shaders do; otherwise per vertex, as desktop fixed function does.
	UBOOL	FragmentReflection;
	// The bicubic taps read the base level explicitly (texture2DLod): GL_ARB_shader_texture_lod on desktop GL;
	// gl4es maps the call to its ES equivalent. Without it the taps read with implicit derivatives, which jump
	// by a texel where the taps move on (those pixels then read a smaller mip where the lightmap is minified).
	UBOOL	LodLookups;
};

//
//	FFFSource: a growing ANSI string.
//
class FFFSource
{
public:
	TArray<ANSICHAR> Text;

	FFFSource() { Text.AddItem(0); }

	void Add( const ANSICHAR* S )
	{
		INT Length = (INT)strlen( S );
		INT End = Text.Num() - 1;
		Text.Add( Length );
		appMemcpy( &Text(End), S, Length + 1 );
	}
	void Addf( const ANSICHAR* Fmt, ... )
	{
		ANSICHAR Buffer[1024];
		va_list Args;
		va_start( Args, Fmt );
		vsnprintf( Buffer, sizeof(Buffer), Fmt, Args );
		va_end( Args );
		Buffer[sizeof(Buffer)-1] = 0;
		Add( Buffer );
	}
	const ANSICHAR* Str() const { return &Text(0); }
};

//
// FFKeyStage: fill a stage key from a driver's material state stage (FOpenGLMaterialStateStage or
// FGL4ESMaterialStateStage). The driver maps ColorArg1, ColorArg2, ColorArg0 to GL_SOURCE0, 1, 2 and
// ColorMod1, ColorMod2, ColorMod0 to GL_OPERAND0, 1, 2 (SetMaterialBlending), and so does the key.
//
template<class TStage> void FFKeyStage( FFFStageKey& Key, const TStage& Stage, UBOOL Bicubic, UBOOL ShaderLodBias )
{
	Key.ColorOp				= Stage.ColorOp;
	Key.AlphaOp				= Stage.AlphaOp;
	Key.ColorSource[0]		= Stage.ColorArg1;
	Key.ColorSource[1]		= Stage.ColorArg2;
	Key.ColorSource[2]		= Stage.ColorArg0;
	Key.AlphaSource[0]		= Stage.AlphaArg1;
	Key.AlphaSource[1]		= Stage.AlphaArg2;
	Key.AlphaSource[2]		= Stage.AlphaArg0;
	Key.ColorOperand[0]		= Stage.ColorMod1;
	Key.ColorOperand[1]		= Stage.ColorMod2;
	Key.ColorOperand[2]		= Stage.ColorMod0;
	Key.AlphaOperand[0]		= Stage.AlphaMod1;
	Key.AlphaOperand[1]		= Stage.AlphaMod2;
	Key.AlphaOperand[2]		= Stage.AlphaMod0;
	Key.ColorScale			= (BYTE)(INT)Stage.ColorScale;
	Key.AlphaScale			= (BYTE)(INT)Stage.AlphaScale;
	Key.Target				= (Stage.Texture && Stage.Texture->IsCubemap) ? FFT_Cube : FFT_2D;
	Key.TexGenMode			= Stage.TexGenCountApplied ? Stage.TexGenModeApplied : 0;
	Key.TexGenCount			= (BYTE)Stage.TexGenCountApplied;
	Key.Bicubic				= Bicubic && Key.Target == FFT_2D;
	Key.LodBias				= ShaderLodBias ? Stage.TextureMipLODBias : 0.f;
}

/*-----------------------------------------------------------------------------
	Source generation.
-----------------------------------------------------------------------------*/

// The value a GL_SOURCEn names, as a vec4 expression; NULL for one the program does not reproduce.
static inline const ANSICHAR* FFSource( DWORD Source, INT Stage, INT NumStages, ANSICHAR* Buffer, INT BufferSize )
{
	if( Source == GL_TEXTURE )
		snprintf( Buffer, BufferSize, "T%i", Stage );
	else if( Source >= GL_TEXTURE0 && Source < GL_TEXTURE0 + (DWORD)NumStages )
		snprintf( Buffer, BufferSize, "T%i", (INT)(Source - GL_TEXTURE0) );
	else if( Source == GL_CONSTANT )
		snprintf( Buffer, BufferSize, "gl_TextureEnvColor[%i]", Stage );
	else if( Source == GL_PRIMARY_COLOR )
		snprintf( Buffer, BufferSize, "gl_Color" );
	else if( Source == GL_PREVIOUS )
		snprintf( Buffer, BufferSize, "%s", Stage ? "Prev" : "gl_Color" );
	else if( Source == GL_ZERO )
		snprintf( Buffer, BufferSize, "vec4(0.0)" );
	else if( Source == GL_ONE )
		snprintf( Buffer, BufferSize, "vec4(1.0)" );
	else
		return NULL;
	return Buffer;
}

// An argument (source with its operand) as a vec3 (Color) or float expression; 0 when not reproduced.
static inline UBOOL FFArgument( FFFSource& Out, DWORD Source, DWORD Operand, UBOOL Color, INT Stage, INT NumStages )
{
	ANSICHAR Buffer[64];
	const ANSICHAR* S = FFSource( Source, Stage, NumStages, Buffer, sizeof(Buffer) );
	if( !S )
		return 0;
	if( Color )
	{
		switch( Operand )
		{
		case GL_SRC_COLOR:				Out.Addf( "%s.rgb", S );				return 1;
		case GL_ONE_MINUS_SRC_COLOR:	Out.Addf( "(1.0 - %s.rgb)", S );		return 1;
		case GL_SRC_ALPHA:				Out.Addf( "vec3(%s.a)", S );			return 1;
		case GL_ONE_MINUS_SRC_ALPHA:	Out.Addf( "vec3(1.0 - %s.a)", S );		return 1;
		}
	}
	else
	{
		switch( Operand )
		{
		case GL_SRC_ALPHA:				Out.Addf( "%s.a", S );					return 1;
		case GL_ONE_MINUS_SRC_ALPHA:	Out.Addf( "(1.0 - %s.a)", S );			return 1;
		}
	}
	return 0;
}

// The arguments an operation reads.
static inline INT FFNumArguments( DWORD Op )
{
	switch( Op )
	{
	case GL_REPLACE:					return 1;
	case GL_MODULATE:
	case GL_ADD:
	case GL_ADD_SIGNED:
	case GL_SUBTRACT:
	case GL_DOT3_RGB:
	case GL_DOT3_RGBA:					return 2;
	case GL_INTERPOLATE:
	case GL_MODULATE_ADD_ATI:
	case GL_MODULATE_SIGNED_ADD_ATI:
	case GL_MODULATE_SUBTRACT_ATI:		return 3;
	}
	return 0;
}

// One combiner (color or alpha) of a stage: "Name = clamp(<op> * scale, 0, 1);". 0 when not reproduced.
static inline UBOOL FFCombiner( FFFSource& Out, const FFFStageKey& Key, UBOOL Color, INT Stage, INT NumStages )
{
	DWORD			Op			= Color ? Key.ColorOp : Key.AlphaOp;
	const DWORD*	Sources		= Color ? Key.ColorSource : Key.AlphaSource;
	const DWORD*	Operands	= Color ? Key.ColorOperand : Key.AlphaOperand;
	INT				Scale		= Color ? Key.ColorScale : Key.AlphaScale;
	INT				NumArgs		= FFNumArguments( Op );

	if( !NumArgs || (!Color && (Op == GL_DOT3_RGB || Op == GL_DOT3_RGBA)) || (Scale != 1 && Scale != 2 && Scale != 4) )
		return 0;

	FFFSource A[3];
	for( INT i=0; i<NumArgs; i++ )
		if( !FFArgument( A[i], Sources[i], Operands[i], Color, Stage, NumStages ) )
			return 0;

	const ANSICHAR*	Type = Color ? "vec3" : "float";
	FFFSource		Expr;
	switch( Op )
	{
	case GL_REPLACE:				Expr.Addf( "%s", A[0].Str() ); break;
	case GL_MODULATE:				Expr.Addf( "%s * %s", A[0].Str(), A[1].Str() ); break;
	case GL_ADD:					Expr.Addf( "%s + %s", A[0].Str(), A[1].Str() ); break;
	case GL_ADD_SIGNED:				Expr.Addf( "%s + %s - 0.5", A[0].Str(), A[1].Str() ); break;
	case GL_SUBTRACT:				Expr.Addf( "%s - %s", A[0].Str(), A[1].Str() ); break;
	case GL_DOT3_RGB:
	case GL_DOT3_RGBA:				Expr.Addf( "vec3(4.0 * dot(%s - 0.5, %s - 0.5))", A[0].Str(), A[1].Str() ); break;
	case GL_INTERPOLATE:			Expr.Addf( "%s * %s + %s * (1.0 - %s)", A[0].Str(), A[2].Str(), A[1].Str(), A[2].Str() ); break;
	case GL_MODULATE_ADD_ATI:		Expr.Addf( "%s * %s + %s", A[0].Str(), A[2].Str(), A[1].Str() ); break;
	case GL_MODULATE_SIGNED_ADD_ATI:Expr.Addf( "%s * %s + %s - 0.5", A[0].Str(), A[2].Str(), A[1].Str() ); break;
	case GL_MODULATE_SUBTRACT_ATI:	Expr.Addf( "%s * %s - %s", A[0].Str(), A[2].Str(), A[1].Str() ); break;
	}

	if( Scale != 1 )
		Out.Addf( "\t%s %s%i = clamp((%s) * %i.0, 0.0, 1.0);\n", Type, Color ? "C" : "A", Stage, Expr.Str(), Scale );
	else
		Out.Addf( "\t%s %s%i = clamp(%s, 0.0, 1.0);\n", Type, Color ? "C" : "A", Stage, Expr.Str() );
	return 1;
}

// Whether a stage's texture is read per fragment through a reflection (FFFProgramOptions::FragmentReflection).
static inline UBOOL FFFragmentReflection( const FFFStageKey& Key, const FFFProgramOptions& Options )
{
	return Options.FragmentReflection && Key.TexGenMode == GL_REFLECTION_MAP;
}

// A float literal for GLSL.
static inline void FFFloat( ANSICHAR* Buffer, INT BufferSize, FLOAT Value )
{
	snprintf( Buffer, BufferSize, "%.9g", Value );
	if( !strchr( Buffer, '.' ) && !strchr( Buffer, 'e' ) && !strchr( Buffer, 'n' ) && !strchr( Buffer, 'i' ) )
		strcat( Buffer, ".0" );
}

//
// FFBuildSources: the vertex and fragment shader of a key; 0 when a stage does something the program does
// not reproduce (the pass then stays on the fixed-function path).
//
static inline UBOOL FFBuildSources( const FFFProgramKey& Key, const FFFProgramOptions& Options, FFFSource& Vertex, FFFSource& Fragment )
{
	INT		NumStages		= Key.NumStages;
	UBOOL	NeedEye			= Key.Fog,
			NeedNormal		= 0,
			NeedReflection	= 0,
			Bicubic			= 0;
	FLOAT	BicubicBias		= 0.f;

	for( INT s=0; s<NumStages; s++ )
	{
		const FFFStageKey& Stage = Key.Stages[s];
		if( Stage.TexGenMode == GL_REFLECTION_MAP )
		{
			NeedEye = NeedNormal = 1;
			if( !FFFragmentReflection( Stage, Options ) )
				NeedReflection = 1;
		}
		else if( Stage.TexGenMode == GL_NORMAL_MAP )
			NeedNormal = 1;
		else if( Stage.TexGenMode && Stage.TexGenMode != GL_OBJECT_LINEAR )
			return 0;
		if( Stage.TexGenMode == GL_REFLECTION_MAP && Stage.TexGenCount > 3 )
			return 0;
		if( Stage.Bicubic && !Bicubic )
			BicubicBias = Stage.LodBias;
		Bicubic |= Stage.Bicubic;
	}

	// Vertex shader.
	if( !Options.Gl4es )
		Vertex.Add( "#version 120\n" );
	if( Key.Fog )
		Vertex.Add( "varying float FogZ;\n" );
	for( INT s=0; s<NumStages; s++ )
		if( FFFragmentReflection( Key.Stages[s], Options ) )
			Vertex.Addf( "varying vec3 ReflEye%i;\nvarying vec3 ReflNrm%i;\n", s, s );
	Vertex.Add( "void main()\n{\n\tgl_Position = ftransform();\n\tgl_FrontColor = gl_Color;\n" );
	if( NeedEye )
		Vertex.Add( "\tvec4 Eye = gl_ModelViewMatrix * gl_Vertex;\n" );
	// The eye-space depth, signed: a polygon clipped at the near plane interpolates it to the clipped
	// vertices correctly, and the fragment takes its absolute value (GL_EYE_PLANE_ABSOLUTE_NV, the default).
	if( Key.Fog )
		Vertex.Add( "\tFogZ = Eye.z;\n" );
	if( NeedNormal )
		Vertex.Add( "\tvec3 Nrm = normalize(gl_NormalMatrix * gl_Normal);\n" );	// GL_NORMALIZE is on
	if( NeedReflection )
		Vertex.Add( "\tvec3 Refl = reflect(normalize(Eye.xyz), Nrm);\n" );
	static const ANSICHAR Component[4]	= { 'x', 'y', 'z', 'w' };
	static const ANSICHAR Plane[4]		= { 'S', 'T', 'R', 'Q' };
	for( INT s=0; s<NumStages; s++ )
	{
		const FFFStageKey& Stage = Key.Stages[s];
		if( FFFragmentReflection( Stage, Options ) )
		{
			Vertex.Addf( "\tReflEye%i = Eye.xyz;\n\tReflNrm%i = Nrm;\n", s, s );
			continue;
		}
		Vertex.Addf( "\tvec4 Coord%i = gl_MultiTexCoord%i;\n", s, s );
		for( INT c=0; c<Stage.TexGenCount; c++ )
		{
			if( Stage.TexGenMode == GL_OBJECT_LINEAR )
				Vertex.Addf( "\tCoord%i.%c = dot(gl_ObjectPlane%c[%i], gl_Vertex);\n", s, Component[c], Plane[c], s );
			else if( Stage.TexGenMode == GL_REFLECTION_MAP )
				Vertex.Addf( "\tCoord%i.%c = Refl.%c;\n", s, Component[c], Component[c] );
			else if( Stage.TexGenMode == GL_NORMAL_MAP && c < 3 )
				Vertex.Addf( "\tCoord%i.%c = Nrm.%c;\n", s, Component[c], Component[c] );
		}
		Vertex.Addf( "\tgl_TexCoord[%i] = gl_TextureMatrix[%i] * Coord%i;\n", s, s, s );
	}
	Vertex.Add( "}\n" );

	// Fragment shader.
	if( !Options.Gl4es )
	{
		Fragment.Add( "#version 120\n" );
		if( Bicubic && Options.LodLookups )
			Fragment.Add( "#extension GL_ARB_shader_texture_lod : require\n" );
	}
	for( INT s=0; s<NumStages; s++ )
	{
		Fragment.Addf( "uniform %s Tex%i;\n", Key.Stages[s].Target == FFT_Cube ? "samplerCube" : "sampler2D", s );
		if( FFFragmentReflection( Key.Stages[s], Options ) )
			Fragment.Addf( "varying vec3 ReflEye%i;\nvarying vec3 ReflNrm%i;\n", s, s );
	}
	if( Key.Fog )
		Fragment.Add( "varying float FogZ;\n" );
	if( Bicubic )
	{
		// The lightmap as a cubic B-spline over its texels, from four bilinear taps: on each axis the four
		// texel weights pair up (w0+w1, w2+w3) into two taps, each placed between its two texels in the
		// ratio of their weights. The weights are never negative, so the result stays within the texels'
		// range, and they sum to one, so a region's mean is kept.
		Fragment.Add( "uniform vec4 LightmapSize;\n" );		// width, height, 1/width, 1/height
		Fragment.Add( "vec4 LightmapCubic(sampler2D Map, vec2 UV)\n{\n" );
		if( Options.LodLookups )
		{
			// Where a pixel spans more than a texel along either axis (scaled by the stage's LOD bias where the
			// program applies it) the lightmap has no texel edge to smooth: the result is the fixed-function
			// stage's own lookup, read first, outside the branch, so that its derivatives are defined. Where it
			// is magnified the four taps read the base level: a mip level's texels span lightmaps beyond the
			// atlas's one-texel borders, which the taps' wider footprint would reach.
			ANSICHAR Bias[48] = "", Scale[40];
			if( BicubicBias != 0.f && Options.ShaderLodBias )
			{
				ANSICHAR Literal[40];
				FFFloat( Literal, sizeof(Literal), BicubicBias );
				snprintf( Bias, sizeof(Bias), ", %s", Literal );
			}
			FFFloat( Scale, sizeof(Scale), appPow( 2.f, BicubicBias ) );
			Fragment.Addf( "\tvec4 Linear = texture2D(Map, UV%s);\n", Bias );
			Fragment.Addf(
				"\tif( max(length(dFdx(UV) * LightmapSize.xy), length(dFdy(UV) * LightmapSize.xy)) * %s > 1.0 )\n"
				"\t\treturn Linear;\n", Scale );
		}
		Fragment.Add(
			"\tvec2 St = UV * LightmapSize.xy - 0.5;\n"
			"\tvec2 I = floor(St);\n"
			"\tvec2 F = St - I;\n"
			"\tvec2 F2 = F * F;\n"
			"\tvec2 F3 = F2 * F;\n"
			"\tvec2 W0 = (1.0 - 3.0 * F + 3.0 * F2 - F3) / 6.0;\n"
			"\tvec2 W1 = (4.0 - 6.0 * F2 + 3.0 * F3) / 6.0;\n"
			"\tvec2 W3 = F3 / 6.0;\n"
			"\tvec2 G0 = W0 + W1;\n"
			"\tvec2 G1 = 1.0 - G0;\n"
			"\tvec2 P0 = (I - 0.5 + W1 / G0) * LightmapSize.zw;\n"
			"\tvec2 P1 = (I + 1.5 + W3 / G1) * LightmapSize.zw;\n" );
		if( Options.LodLookups )
		{
			Fragment.Add(
				"\treturn G0.y * (G0.x * texture2DLod(Map, P0, 0.0) + G1.x * texture2DLod(Map, vec2(P1.x, P0.y), 0.0))\n"
				"\t     + G1.y * (G0.x * texture2DLod(Map, vec2(P0.x, P1.y), 0.0) + G1.x * texture2DLod(Map, P1, 0.0));\n" );
		}
		else
		{
			Fragment.Add(
				"\treturn G0.y * (G0.x * texture2D(Map, P0) + G1.x * texture2D(Map, vec2(P1.x, P0.y)))\n"
				"\t     + G1.y * (G0.x * texture2D(Map, vec2(P0.x, P1.y)) + G1.x * texture2D(Map, P1));\n" );
		}
		Fragment.Add( "}\n" );
	}
	Fragment.Add( "void main()\n{\n" );

	// The stages' texels, all read first (a stage may name another stage's texture).
	for( INT s=0; s<NumStages; s++ )
	{
		const FFFStageKey& Stage = Key.Stages[s];
		ANSICHAR Bias[48] = "";
		if( Stage.LodBias != 0.f )
		{
			ANSICHAR Literal[40];
			FFFloat( Literal, sizeof(Literal), Stage.LodBias );
			snprintf( Bias, sizeof(Bias), ", %s", Literal );
		}
		if( Stage.Bicubic )
		{
			Fragment.Addf( "\tvec4 T%i = LightmapCubic(Tex%i, gl_TexCoord[%i].xy / gl_TexCoord[%i].w);\n", s, s, s, s );
		}
		else if( FFFragmentReflection( Stage, Options ) )
		{
			Fragment.Addf( "\tvec4 R%i = gl_TextureMatrix[%i] * vec4(reflect(normalize(ReflEye%i), normalize(ReflNrm%i)), 1.0);\n", s, s, s, s );
			if( Stage.Target == FFT_Cube )
				Fragment.Addf( "\tvec4 T%i = textureCube(Tex%i, R%i.xyz%s);\n", s, s, s, Bias );
			else
				Fragment.Addf( "\tvec4 T%i = texture2D(Tex%i, R%i.xy%s);\n", s, s, s, Bias );
		}
		else if( Stage.Target == FFT_Cube )
			Fragment.Addf( "\tvec4 T%i = textureCube(Tex%i, gl_TexCoord[%i].xyz%s);\n", s, s, s, Bias );
		else
			Fragment.Addf( "\tvec4 T%i = texture2DProj(Tex%i, gl_TexCoord[%i]%s);\n", s, s, s, Bias );
	}

	// The combiners, in stage order; GL_PREVIOUS of stage 0 is the primary color.
	Fragment.Add( "\tvec4 Prev = gl_Color;\n" );
	for( INT s=0; s<NumStages; s++ )
	{
		const FFFStageKey& Stage = Key.Stages[s];
		if( !FFCombiner( Fragment, Stage, 1, s, NumStages ) )
			return 0;
		if( Stage.ColorOp == GL_DOT3_RGBA )
			Fragment.Addf( "\tPrev = vec4(C%i, C%i.r);\n", s, s );
		else
		{
			if( !FFCombiner( Fragment, Stage, 0, s, NumStages ) )
				return 0;
			Fragment.Addf( "\tPrev = vec4(C%i, A%i);\n", s, s );
		}
	}
	if( Key.Fog )
		Fragment.Add( "\tPrev.rgb = mix(gl_Fog.color.rgb, Prev.rgb, clamp((gl_Fog.end - abs(FogZ)) * gl_Fog.scale, 0.0, 1.0));\n" );
	Fragment.Add( "\tgl_FragColor = Prev;\n}\n" );
	return 1;
}

/*-----------------------------------------------------------------------------
	FFFProgramCache.
-----------------------------------------------------------------------------*/

//
//	FFFProgram: a linked program for a key; Program is 0 when its sources did not compile or link.
//
struct FFFProgram
{
	FFFProgramKey	Key;
	DWORD			Hash;
	GLuint			Program;
	GLint			LightmapSizeLoc;
	FLOAT			LightmapWidth,
					LightmapHeight;
};

//
//	FFFProgramCache: one per device; TRenDev is UOpenGLRenderDevice or UGL4ESRenderDevice.
//
class FFFProgramCache
{
public:
	TArray<FFFProgram*>		Programs;
	FFFProgram*				Last;
	GLuint					Current;
	INT						NumFailed;

	FFFProgramCache() : Last(NULL), Current(0), NumFailed(0) {}
	~FFFProgramCache() { Empty(); }

	// Empty: forgets every program; the GL objects went with the context (Flush deletes them while it lives).
	void Empty()
	{
		for( INT i=0; i<Programs.Num(); i++ )
			delete Programs(i);
		Programs.Empty();
		Last		= NULL;
		Current		= 0;
		NumFailed	= 0;
	}

	static DWORD KeyHash( const FFFProgramKey& Key )
	{
		return appMemCrc( &Key, sizeof(Key) );
	}

	// Use: makes Program (0 = the fixed-function path) current.
	template<class TRenDev> void Use( TRenDev* RenDev, GLuint Program )
	{
		if( Current != Program )
		{
			RenDev->glUseProgram( Program );
			Current = Program;
		}
	}

	// Flush: deletes the programs; the device's context is current.
	template<class TRenDev> void Flush( TRenDev* RenDev )
	{
		Use( RenDev, 0 );
		for( INT i=0; i<Programs.Num(); i++ )
			if( Programs(i)->Program )
				RenDev->glDeleteProgram( Programs(i)->Program );
		Empty();
	}

	// Find: the program of a key, compiled on first use.
	template<class TRenDev> FFFProgram* Find( TRenDev* RenDev, const FFFProgramKey& Key, const FFFProgramOptions& Options, const TCHAR* LogName )
	{
		if( Last && appMemcmp( &Last->Key, &Key, sizeof(Key) ) == 0 )
			return Last;
		DWORD Hash = KeyHash( Key );
		for( INT i=0; i<Programs.Num(); i++ )
			if( Programs(i)->Hash == Hash && appMemcmp( &Programs(i)->Key, &Key, sizeof(Key) ) == 0 )
				return Last = Programs(i);

		FFFProgram* P		= new FFFProgram;
		P->Key				= Key;
		P->Hash				= Hash;
		P->Program			= 0;
		P->LightmapSizeLoc	= -1;
		P->LightmapWidth	= 0.f;
		P->LightmapHeight	= 0.f;
		Programs.AddItem( P );
		Last = P;

		FFFSource Vertex, Fragment;
		if( !FFBuildSources( Key, Options, Vertex, Fragment ) )
		{
			NumFailed++;
			if( GUfrontLog )
				debugf( TEXT("%s: a lightmap pass stays fixed function: a stage does what the programs do not reproduce"), LogName );
			return P;
		}
		P->Program = Compile( RenDev, Vertex, Fragment, LogName );
		if( !P->Program )
		{
			NumFailed++;
			return P;
		}

		RenDev->glUseProgram( P->Program );
		for( INT s=0; s<Key.NumStages; s++ )
		{
			ANSICHAR Name[16];
			snprintf( Name, sizeof(Name), "Tex%i", s );
			GLint Loc = RenDev->glGetUniformLocation( P->Program, Name );
			if( Loc >= 0 )
				RenDev->glUniform1i( Loc, s );
		}
		P->LightmapSizeLoc = RenDev->glGetUniformLocation( P->Program, "LightmapSize" );
		RenDev->glUseProgram( Current );

		if( GUfrontLog )
		{
			// Each stage: 2 or C (its texture's target), then B (the bicubic lightmap read), O, R or N (the
			// coordinates generated object-linear, as a reflection or as the normal).
			FString Stages;
			for( INT s=0; s<Key.NumStages; s++ )
			{
				const FFFStageKey& Stage = Key.Stages[s];
				Stages += TEXT(" ");
				Stages += Stage.Target == FFT_Cube ? TEXT("C") : TEXT("2");
				if( Stage.Bicubic )
					Stages += TEXT("B");
				if( Stage.TexGenMode == GL_OBJECT_LINEAR )
					Stages += TEXT("O");
				else if( Stage.TexGenMode == GL_REFLECTION_MAP )
					Stages += TEXT("R");
				else if( Stage.TexGenMode == GL_NORMAL_MAP )
					Stages += TEXT("N");
			}
			debugf( TEXT("%s: lightmap program %i compiled (stages%s%s)"), LogName, Programs.Num(), *Stages, Key.Fog ? TEXT(", fog") : TEXT("") );
		}
		return P;
	}

	// SetLightmapSize: the bicubic filter's texel grid, for the program that is current.
	template<class TRenDev> void SetLightmapSize( TRenDev* RenDev, FFFProgram* P, INT Width, INT Height )
	{
		if( P->LightmapSizeLoc < 0 || Width <= 0 || Height <= 0 )
			return;
		if( P->LightmapWidth != (FLOAT)Width || P->LightmapHeight != (FLOAT)Height )
		{
			RenDev->glUniform4f( P->LightmapSizeLoc, (FLOAT)Width, (FLOAT)Height, 1.f / Width, 1.f / Height );
			P->LightmapWidth	= (FLOAT)Width;
			P->LightmapHeight	= (FLOAT)Height;
		}
	}

private:
	// LogSource: a shader's source, numbered line by line, after its compile error.
	static void LogSource( const FFFSource& Source )
	{
		const ANSICHAR* Line = Source.Str();
		for( INT Number=1; *Line; Number++ )
		{
			const ANSICHAR* End = strchr( Line, '\n' );
			INT Length = End ? (INT)(End - Line) : (INT)strlen( Line );
			ANSICHAR Buffer[512];
			snprintf( Buffer, sizeof(Buffer), "%.*s", Min( Length, (INT)sizeof(Buffer) - 1 ), Line );
			debugf( NAME_Warning, TEXT("%3i: %s"), Number, appFromAnsi(Buffer) );
			Line += End ? Length + 1 : Length;
		}
	}

	template<class TRenDev> static GLuint CompileShader( TRenDev* RenDev, GLenum Type, const FFFSource& Source, const TCHAR* LogName )
	{
		GLuint Shader = RenDev->glCreateShader( Type );
		if( !Shader )
			return 0;
		const ANSICHAR* Text = Source.Str();
		RenDev->glShaderSource( Shader, 1, &Text, NULL );
		RenDev->glCompileShader( Shader );
		GLint Status = 0;
		RenDev->glGetShaderiv( Shader, GL_COMPILE_STATUS, &Status );
		if( !Status )
		{
			ANSICHAR InfoLog[2048] = "";
			RenDev->glGetShaderInfoLog( Shader, sizeof(InfoLog), NULL, InfoLog );
			debugf( NAME_Warning, TEXT("%s: lightmap %s shader does not compile: %s"), LogName, Type == GL_VERTEX_SHADER ? TEXT("vertex") : TEXT("fragment"), appFromAnsi(InfoLog) );
			LogSource( Source );
			RenDev->glDeleteShader( Shader );
			return 0;
		}
		return Shader;
	}

	template<class TRenDev> static GLuint Compile( TRenDev* RenDev, const FFFSource& Vertex, const FFFSource& Fragment, const TCHAR* LogName )
	{
		GLuint VS = CompileShader( RenDev, GL_VERTEX_SHADER, Vertex, LogName );
		GLuint FS = VS ? CompileShader( RenDev, GL_FRAGMENT_SHADER, Fragment, LogName ) : 0;
		if( !VS || !FS )
		{
			if( VS )
				RenDev->glDeleteShader( VS );
			return 0;
		}
		GLuint Program = RenDev->glCreateProgram();
		RenDev->glAttachShader( Program, VS );
		RenDev->glAttachShader( Program, FS );
		RenDev->glLinkProgram( Program );
		RenDev->glDeleteShader( VS );
		RenDev->glDeleteShader( FS );
		GLint Status = 0;
		RenDev->glGetProgramiv( Program, GL_LINK_STATUS, &Status );
		if( !Status )
		{
			ANSICHAR InfoLog[2048] = "";
			RenDev->glGetProgramInfoLog( Program, sizeof(InfoLog), NULL, InfoLog );
			debugf( NAME_Warning, TEXT("%s: lightmap program does not link: %s"), LogName, appFromAnsi(InfoLog) );
			RenDev->glDeleteProgram( Program );
			return 0;
		}
		return Program;
	}
};

/*-----------------------------------------------------------------------------
	BICUBICLIGHTMAPS EDGES: what the filter reads past each lightmap.
-----------------------------------------------------------------------------*/

// A line on the console and in the log.
static inline void FFAnswer( FOutputDevice& Ar, const TCHAR* Fmt, ... )
{
	TCHAR Text[1024];
	GET_VARARGS( Text, ARRAY_COUNT(Text), Fmt, Fmt );
	debugf( TEXT("%s"), Text );
	if( &Ar != GLog )
		Ar.Log( Text );
}

// A DXT1 or DXT3 color block as 16 texels (the 4x4 block, row by row).
static inline void FFDecodeDXTColors( const BYTE* Block, UBOOL FourColors, FColor* Out )
{
	_WORD	C[2] = { (_WORD)(Block[0] | (Block[1] << 8)), (_WORD)(Block[2] | (Block[3] << 8)) };
	INT		P[4][3];
	for( INT i=0; i<2; i++ )
	{
		INT R = (C[i] >> 11) & 31, G = (C[i] >> 5) & 63, B = C[i] & 31;
		P[i][0] = (R << 3) | (R >> 2);
		P[i][1] = (G << 2) | (G >> 4);
		P[i][2] = (B << 3) | (B >> 2);
	}
	for( INT c=0; c<3; c++ )
	{
		if( FourColors || C[0] > C[1] )
		{
			P[2][c] = (2 * P[0][c] + P[1][c]) / 3;
			P[3][c] = (P[0][c] + 2 * P[1][c]) / 3;
		}
		else
		{
			P[2][c] = (P[0][c] + P[1][c]) / 2;
			P[3][c] = 0;
		}
	}
	DWORD Bits = Block[4] | (Block[5] << 8) | (Block[6] << 16) | ((DWORD)Block[7] << 24);
	for( INT i=0; i<16; i++ )
	{
		INT Index = (Bits >> (2 * i)) & 3;
		Out[i] = FColor( P[Index][0], P[Index][1], P[Index][2], 255 );
	}
}

static int FFCompareFloats( const void* A, const void* B )
{
	FLOAT X = *(const FLOAT*)A, Y = *(const FLOAT*)B;
	return X < Y ? -1 : X > Y ? 1 : 0;
}

// The cubic B-spline weights of the four texels around a fraction.
static inline void FFCubicWeights( FLOAT F, FLOAT* W )
{
	W[0] = (1.f - F) * (1.f - F) * (1.f - F) / 6.f;
	W[1] = (4.f - 6.f * F * F + 3.f * F * F * F) / 6.f;
	W[3] = F * F * F / 6.f;
	W[2] = 1.f - W[0] - W[1] - W[3];
}

// The filter's value at atlas texel coordinates (X, Y), with the texel indices clamped to [Min, Max] per axis.
static inline void FFCubicAt( const FColor* Atlas, INT Width, FLOAT X, FLOAT Y, INT MinX, INT MaxX, INT MinY, INT MaxY, FLOAT* Out )
{
	FLOAT	SX = X - 0.5f, SY = Y - 0.5f;
	INT		IX = appFloor( SX ), IY = appFloor( SY );
	FLOAT	WX[4], WY[4];
	FFCubicWeights( SX - IX, WX );
	FFCubicWeights( SY - IY, WY );
	Out[0] = Out[1] = Out[2] = 0.f;
	for( INT j=0; j<4; j++ )
		for( INT i=0; i<4; i++ )
		{
			const FColor& T = Atlas[ Clamp( IY - 1 + j, MinY, MaxY ) * Width + Clamp( IX - 1 + i, MinX, MaxX ) ];
			FLOAT W = WX[i] * WY[j];
			Out[0] += W * T.R;
			Out[1] += W * T.G;
			Out[2] += W * T.B;
		}
}

//
// FFLogLightmapEdges: for the current level's lightmaps, the filter's value along each surface edge (a
// lightmap's texel coordinates 1 and Size-1) as drawn, against the same filter with every tap clamped to the
// lightmap's own texels: the bleed from the neighbouring texels of the atlas. The atlases are the ones BSP
// draws: the compressed static textures where they are current, otherwise the lightmaps as computed.
//
static inline void FFLogLightmapEdges( UBOOL UseCompressedLightmaps, FOutputDevice& Ar )
{
	ULevel* Level = NULL;
	for( TObjectIterator<UViewport> It; It && !Level; ++It )
		if( It->Actor && It->Actor->GetLevel() )
			Level = It->Actor->GetLevel();
	if( !Level || !Level->Model )
	{
		FFAnswer( Ar, TEXT("BICUBICLIGHTMAPS EDGES: no level") );
		return;
	}
	UModel*			Model = Level->Model;
	TArray<FLOAT>	Diffs;
	FLOAT			Worst = -1.f;
	INT				WorstLightMap = INDEX_NONE;
	FVector			WorstPos( 0, 0, 0 );
	FLOAT			WorstOwn[3] = { 0, 0, 0 }, WorstDrawn[3] = { 0, 0, 0 };
	INT				NumStatic = 0, NumComputed = 0, NumLightMaps = 0;
	const INT		Width = LIGHTMAP_TEXTURE_WIDTH, Height = LIGHTMAP_TEXTURE_HEIGHT;
	TArray<FColor>	Atlas;
	Atlas.Add( Width * Height );

	for( INT TextureIndex=0; TextureIndex<Model->LightMapTextures.Num(); TextureIndex++ )
	{
		FLightMapTexture&			Texture = Model->LightMapTextures(TextureIndex);
		FStaticLightMapTexture&		Static	= Texture.StaticTexture;
		appMemzero( &Atlas(0), Width * Height * sizeof(FColor) );

		UBOOL DrawsStatic = UseCompressedLightmaps && Static.Revision == Texture.Revision && Static.Width == Width && Static.Height == Height
						&& (Static.Format == TEXF_DXT1 || Static.Format == TEXF_DXT3);
		if( DrawsStatic )
		{
			const BYTE*	Data		= (const BYTE*) Static.GetRawTextureData( 0 );
			INT			BlockSize	= Static.Format == TEXF_DXT1 ? 8 : 16;
			for( INT BY=0; BY<Height/4; BY++ )
				for( INT BX=0; BX<Width/4; BX++ )
				{
					FColor Block[16];
					const BYTE* Src = Data + (BY * (Width / 4) + BX) * BlockSize;
					FFDecodeDXTColors( Src + BlockSize - 8, Static.Format == TEXF_DXT3, Block );
					for( INT i=0; i<16; i++ )
						Atlas( (BY * 4 + i / 4) * Width + BX * 4 + i % 4 ) = Block[i];
				}
			Static.UnloadRawTextureData( 0 );
			NumStatic++;
		}
		else
		{
			for( INT ChildIndex=0; ChildIndex<Texture.GetNumChildren(); ChildIndex++ )
			{
				INT ChildX = 0, ChildY = 0;
				FTexture* Child = Texture.GetChild( ChildIndex, &ChildX, &ChildY );
				if( Child && ChildX + Child->GetWidth() <= Width && ChildY + Child->GetHeight() <= Height )
					Child->GetTextureData( 0, &Atlas( ChildY * Width + ChildX ), Width * sizeof(FColor), TEXF_RGBA8, 0 );
			}
			NumComputed++;
		}

		for( INT Index=0; Index<Texture.LightMaps.Num(); Index++ )
		{
			INT			LightMapIndex = Texture.LightMaps(Index);
			FLightMap&	LightMap = Model->LightMaps(LightMapIndex);
			INT			MinX = LightMap.OffsetX, MaxX = LightMap.OffsetX + LightMap.SizeX - 1,
						MinY = LightMap.OffsetY, MaxY = LightMap.OffsetY + LightMap.SizeY - 1;
			if( LightMap.SizeX < 3 || LightMap.SizeY < 3 || MaxX >= Width || MaxY >= Height )
				continue;
			NumLightMaps++;
			// The edges, every half texel: u = 1 and SizeX-1 along v, v = 1 and SizeY-1 along u.
			for( INT Edge=0; Edge<4; Edge++ )
			{
				INT Steps = 2 * ((Edge < 2 ? LightMap.SizeY : LightMap.SizeX) - 2);
				for( INT Step=0; Step<=Steps; Step++ )
				{
					FLOAT Along = 1.f + 0.5f * Step;
					FLOAT U = Edge < 2 ? (Edge == 0 ? 1.f : LightMap.SizeX - 1.f) : Along;
					FLOAT V = Edge < 2 ? Along : (Edge == 2 ? 1.f : LightMap.SizeY - 1.f);
					FLOAT Drawn[3], Own[3];
					FFCubicAt( &Atlas(0), Width, MinX + U, MinY + V, 0, Width - 1, 0, Height - 1, Drawn );
					FFCubicAt( &Atlas(0), Width, MinX + U, MinY + V, MinX, MaxX, MinY, MaxY, Own );
					FLOAT Diff = Max( Max( Abs(Drawn[0] - Own[0]), Abs(Drawn[1] - Own[1]) ), Abs(Drawn[2] - Own[2]) );
					Diffs.AddItem( Diff );
					if( Diff > Worst )
					{
						Worst			= Diff;
						WorstLightMap	= LightMapIndex;
						WorstPos		= LightMap.LightMapBase + LightMap.LightMapX * U + LightMap.LightMapY * V;
						for( INT c=0; c<3; c++ )
						{
							WorstOwn[c]		= Own[c];
							WorstDrawn[c]	= Drawn[c];
						}
					}
				}
			}
		}
	}

	if( !Diffs.Num() )
	{
		FFAnswer( Ar, TEXT("BICUBICLIGHTMAPS EDGES: %s has no lightmaps"), Level->GetPathName() );
		return;
	}
	qsort( &Diffs(0), Diffs.Num(), sizeof(FLOAT), FFCompareFloats );
	DOUBLE	Sum = 0.0;
	INT		Above[3] = { 0, 0, 0 };
	for( INT i=0; i<Diffs.Num(); i++ )
	{
		Sum += Diffs(i);
		Above[0] += Diffs(i) > 0.5f;
		Above[1] += Diffs(i) > 1.f;
		Above[2] += Diffs(i) > 2.f;
	}
	FLOAT Percent = 100.f / Diffs.Num();
	FFAnswer( Ar, TEXT("BICUBICLIGHTMAPS EDGES: %s: %i lightmaps in %i atlases (%i compressed, %i computed), %i edge samples"),
		Level->GetOuter()->GetName(), NumLightMaps, Model->LightMapTextures.Num(), NumStatic, NumComputed, Diffs.Num() );
	FFAnswer( Ar, TEXT("BICUBICLIGHTMAPS EDGES: bleed in lightmap levels (0-255, before the 2x modulation): mean %.2f, median %.2f, p99 %.2f, max %.2f; above 0.5: %.1f%%, above 1: %.1f%%, above 2: %.2f%%"),
		(FLOAT)(Sum / Diffs.Num()), Diffs(Diffs.Num() / 2), Diffs( Min( Diffs.Num() - 1, (INT)(Diffs.Num() * 0.99f) ) ), Diffs(Diffs.Num() - 1),
		Above[0] * Percent, Above[1] * Percent, Above[2] * Percent );
	if( WorstLightMap != INDEX_NONE )
		FFAnswer( Ar, TEXT("BICUBICLIGHTMAPS EDGES: largest at surface %i, %.1f %.1f %.1f: own texels %.1f %.1f %.1f, drawn %.1f %.1f %.1f"),
			Model->LightMaps(WorstLightMap).iSurf, WorstPos.X, WorstPos.Y, WorstPos.Z,
			WorstOwn[0], WorstOwn[1], WorstOwn[2], WorstDrawn[0], WorstDrawn[1], WorstDrawn[2] );
}

#endif

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
