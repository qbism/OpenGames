#ifndef SHADER_H
#define SHADER_H
typedef void (shader_gen_t)(char *name, shader_t*, const void *args);

#define SHADER_TMU_MAX 16
#define SHADER_PASS_MAX	8
#define SHADER_MAX_TC_MODS	8
#define SHADER_DEFORM_MAX	8
#define SHADER_MAX_ANIMFRAMES	8
#define SHADER_ANIM_FRAMES_MAX 16

#define SHADER_PROGPARMS_MAX 16

typedef enum {
	SHADER_BSP,
	SHADER_BSP_VERTEX,
	SHADER_BSP_FLARE,
	SHADER_MD3,
	SHADER_2D
} shadertype_t;

typedef enum {
	MF_NONE			= 1<<0,
	MF_NORMALS		= 1<<1,
	MF_TRNORMALS	= 1<<2,
	MF_COLORS		= 1<<3,
	MF_STCOORDS		= 1<<4,
	MF_LMCOORDS		= 1<<5,
	MF_NOCULL		= 1<<6,
	MF_NONBATCHED	= 1<<7
} meshfeatures_t;

//colour manipulation
typedef struct
{
    enum {
		SHADER_FUNC_SIN,
		SHADER_FUNC_TRIANGLE,
		SHADER_FUNC_SQUARE,
		SHADER_FUNC_SAWTOOTH,
		SHADER_FUNC_INVERSESAWTOOTH,
		SHADER_FUNC_NOISE,
		SHADER_FUNC_CONSTANT
	} type;				// SHADER_FUNC enum
    float			args[4];			// offset, amplitude, phase_offset, rate
} shaderfunc_t;

//tecture coordinate manipulation
typedef struct 
{
	enum {
		SHADER_TCMOD_NONE,		//bug
		SHADER_TCMOD_SCALE,		//some sorta tabled deformation
		SHADER_TCMOD_SCROLL,	//boring moving texcoords with time
		SHADER_TCMOD_STRETCH,	//constant factor
		SHADER_TCMOD_ROTATE,
		SHADER_TCMOD_TRANSFORM,
		SHADER_TCMOD_TURB
	} type;
	float			args[6];
} tcmod_t;

//vertex positioning manipulation.
typedef struct
{
	enum {
		DEFORMV_NONE,		//bug
		DEFORMV_MOVE,
		DEFORMV_WAVE,
		DEFORMV_NORMAL,
		DEFORMV_BULGE,
		DEFORMV_AUTOSPRITE,
		DEFORMV_AUTOSPRITE2,
		DEFORMV_PROJECTION_SHADOW
	} type;
    float			args[4];
    shaderfunc_t	func;
} deformv_t;

enum
{
	/*source and dest factors match each other for easier parsing
	  but they're not meant to ever be set on the shader itself
	  NONE is also invalid, and is used to signify disabled, it should never be set on only one
	*/
	SBITS_SRCBLEND_NONE					= 0x00000000,
	SBITS_SRCBLEND_ZERO					= 0x00000001,
	SBITS_SRCBLEND_ONE					= 0x00000002,
	SBITS_SRCBLEND_DST_COLOR			= 0x00000003,
	SBITS_SRCBLEND_ONE_MINUS_DST_COLOR	= 0x00000004,
	SBITS_SRCBLEND_SRC_ALPHA			= 0x00000005,
	SBITS_SRCBLEND_ONE_MINUS_SRC_ALPHA	= 0x00000006,
	SBITS_SRCBLEND_DST_ALPHA			= 0x00000007,
	SBITS_SRCBLEND_ONE_MINUS_DST_ALPHA	= 0x00000008,
	SBITS_SRCBLEND_SRC_COLOR_INVALID			= 0x00000009,
	SBITS_SRCBLEND_ONE_MINUS_SRC_COLOR_INVALID	= 0x0000000a,
	SBITS_SRCBLEND_ALPHA_SATURATE		= 0x0000000b,
#define SBITS_SRCBLEND_BITS				  0x0000000f

	/*must match src factors, just shifted 4*/
	SBITS_DSTBLEND_NONE					= 0x00000000,
	SBITS_DSTBLEND_ZERO					= 0x00000010,
	SBITS_DSTBLEND_ONE					= 0x00000020,
	SBITS_DSTBLEND_DST_COLOR_INVALID			= 0x00000030,
	SBITS_DSTBLEND_ONE_MINUS_DST_COLOR_INVALID	= 0x00000040,
	SBITS_DSTBLEND_SRC_ALPHA			= 0x00000050,
	SBITS_DSTBLEND_ONE_MINUS_SRC_ALPHA	= 0x00000060,
	SBITS_DSTBLEND_DST_ALPHA			= 0x00000070,
	SBITS_DSTBLEND_ONE_MINUS_DST_ALPHA	= 0x00000080,
	SBITS_DSTBLEND_SRC_COLOR			= 0x00000090,
	SBITS_DSTBLEND_ONE_MINUS_SRC_COLOR	= 0x000000a0,
	SBITS_DSTBLEND_ALPHA_SATURATE_INVALID		= 0x000000b0,
#define SBITS_DSTBLEND_BITS				  0x000000f0

#define SBITS_BLEND_BITS				(SBITS_SRCBLEND_BITS|SBITS_DSTBLEND_BITS)

	SBITS_MASK_RED						= 0x00000100,
	SBITS_MASK_GREEN					= 0x00000200,
	SBITS_MASK_BLUE						= 0x00000400,
	SBITS_MASK_ALPHA					= 0x00000800,
#define SBITS_MASK_BITS				  0x00000f00

	SBITS_ATEST_NONE					= 0x00000000,
	SBITS_ATEST_GT0						= 0x00001000,
	SBITS_ATEST_LT128					= 0x00002000,
	SBITS_ATEST_GE128					= 0x00003000,
#define SBITS_ATEST_BITS				  0x0000f000

	SBITS_MISC_DEPTHWRITE				= 0x00010000,
	SBITS_MISC_NODEPTHTEST				= 0x00020000,
	SBITS_MISC_DEPTHEQUALONLY			= 0x00040000,
	SBITS_MISC_DEPTHCLOSERONLY			= 0x00080000,
#define SBITS_MISC_BITS				  0x000f0000

	SBITS_TRUFORM						= 0x00100000,
};


typedef struct shaderpass_s {
	int numMergedPasses;

#ifndef NOMEDIA
	struct cin_s *cin;
#endif
	
	unsigned int	shaderbits;

	enum {
		PBM_MODULATE,
		PBM_OVERBRIGHT,
		PBM_DECAL,
		PBM_ADD,
		PBM_DOTPRODUCT,
		PBM_REPLACE,
		PBM_REPLACELIGHT,
		PBM_MODULATE_PREV_COLOUR
	} blendmode;

	enum {
		RGB_GEN_WAVE,
		RGB_GEN_ENTITY,
		RGB_GEN_ONE_MINUS_ENTITY,
		RGB_GEN_VERTEX_LIGHTING,
		RGB_GEN_VERTEX_EXACT,
		RGB_GEN_ONE_MINUS_VERTEX,
		RGB_GEN_IDENTITY_LIGHTING,
		RGB_GEN_IDENTITY,
		RGB_GEN_CONST,
		RGB_GEN_UNKNOWN,
		RGB_GEN_LIGHTING_DIFFUSE,
		RGB_GEN_TOPCOLOR,
		RGB_GEN_BOTTOMCOLOR
	} rgbgen;
	shaderfunc_t rgbgen_func;

	enum {
		ALPHA_GEN_ENTITY,
		ALPHA_GEN_WAVE,
		ALPHA_GEN_PORTAL,
		ALPHA_GEN_SPECULAR,
		ALPHA_GEN_IDENTITY,
		ALPHA_GEN_VERTEX,
		ALPHA_GEN_CONST
	} alphagen;
	shaderfunc_t alphagen_func;

	enum {
		TC_GEN_BASE,	//basic specified texture coords
		TC_GEN_LIGHTMAP,	//use loaded lightmap coords
		TC_GEN_ENVIRONMENT,
		TC_GEN_DOTPRODUCT,
		TC_GEN_VECTOR,

		//these are really for use only in glsl stuff or perhaps cubemaps, as they generate 3d coords.
		TC_GEN_NORMAL,
		TC_GEN_SVECTOR,
		TC_GEN_TVECTOR,
		TC_GEN_SKYBOX,
		TC_GEN_WOBBLESKY,
		TC_GEN_REFLECT,
	} tcgen;
	int numtcmods;
	tcmod_t		tcmods[SHADER_MAX_TC_MODS];

	int anim_numframes;
	texid_t			anim_frames[SHADER_MAX_ANIMFRAMES];
	float anim_fps;
//	unsigned int texturetype;

	enum {
		T_GEN_SINGLEMAP,	//single texture specified in the shader
		T_GEN_ANIMMAP,		//animating sequence of textures specified in the shader
		T_GEN_LIGHTMAP,		//world light samples
		T_GEN_DELUXMAP,		//world light directions
		T_GEN_SHADOWMAP,	//light's depth values.
		T_GEN_LIGHTCUBEMAP,	//light's projected cubemap

		T_GEN_DIFFUSE,		//texture's default diffuse texture
		T_GEN_NORMALMAP,	//texture's default normalmap
		T_GEN_SPECULAR,		//texture's default specular texture
		T_GEN_UPPEROVERLAY,	//texture's default personal colour
		T_GEN_LOWEROVERLAY,	//texture's default team colour
		T_GEN_FULLBRIGHT,	//texture's default fullbright overlay

		T_GEN_CURRENTRENDER,//copy the current screen to a texture, and draw that

		T_GEN_SOURCECOLOUR, //used for render-to-texture targets
		T_GEN_SOURCEDEPTH,	//used for render-to-texture targets

		T_GEN_REFLECTION,	//reflection image (mirror-as-fbo)
		T_GEN_REFRACTION,	//refraction image (portal-as-fbo)
		T_GEN_REFRACTIONDEPTH,	//refraction image (portal-as-fbo)
		T_GEN_RIPPLEMAP,	//ripplemap image (water surface distortions-as-fbo)

		T_GEN_SOURCECUBE,	//used for render-to-texture targets

		T_GEN_VIDEOMAP,		//use the media playback as an image source, updating each frame for which it is visible
		T_GEN_CUBEMAP,		//use a cubemap instead, otherwise like T_GEN_SINGLEMAP
		T_GEN_3DMAP,		//use a 3d texture instead, otherwise T_GEN_SINGLEMAP.
	} texgen;

	enum {
		SHADER_PASS_CLAMP		= 1<<0,	//needed for d3d's sampler states, infects image flags
		SHADER_PASS_NEAREST		= 1<<1,	//needed for d3d's sampler states, infects image flags
		SHADER_PASS_NOMIPMAP    = 1<<2,	//infects image flags
		SHADER_PASS_NOCOLORARRAY = 1<< 3,

		//FIXME: remove these
		SHADER_PASS_VIDEOMAP	= 1 << 4,
		SHADER_PASS_DETAIL		= 1 << 5,
		SHADER_PASS_LIGHTMAP	= 1 << 6,
		SHADER_PASS_DELUXMAP	= 1 << 7,
		SHADER_PASS_ANIMMAP		= 1 << 8
	} flags;
} shaderpass_t;

typedef struct
{
	texid_t			farbox_textures[6];
	texid_t			nearbox_textures[6];
} skydome_t;

enum{
	PERMUTATION_GENERIC = 0,
	PERMUTATION_BUMPMAP = 1,
	PERMUTATION_FULLBRIGHT = 2,
	PERMUTATION_UPPERLOWER = 4,
	PERMUTATION_DELUXE = 8,
	PERMUTATION_SKELETAL = 16,
	PERMUTATION_FOG	= 32,
	PERMUTATION_FRAMEBLEND = 64,
	PERMUTATION_LIGHTSTYLES = 128,
	PERMUTATIONS = 256
};

enum shaderattribs_e
{
	VATTR_LEG_VERTEX,

	VATTR_VERTEX1,
	VATTR_VERTEX2,
	VATTR_COLOUR,
	VATTR_TEXCOORD,
	VATTR_LMCOORD,
	VATTR_NORMALS,
	VATTR_SNORMALS,
	VATTR_TNORMALS,
	VATTR_BONENUMS, /*skeletal only*/
	VATTR_BONEWEIGHTS, /*skeletal only*/
	VATTR_LMCOORD2,
	VATTR_LMCOORD3,
	VATTR_LMCOORD4,

	VATTR_LEG_COLOUR,
	VATTR_LEG_ELEMENTS,
	VATTR_LEG_TMU0,


	VATTR_LEG_FIRST=VATTR_LEG_COLOUR
};

typedef struct {
	enum shaderprogparmtype_e {
		SP_BAD,	//never set (hopefully)

		/*entity properties*/
		SP_E_VBLEND,
		SP_E_LMSCALE,
		SP_E_ORIGIN,
		SP_E_COLOURS,
		SP_E_COLOURSIDENT,
		SP_E_GLOWMOD,
		SP_E_TOPCOLOURS,
		SP_E_BOTTOMCOLOURS,
		SP_E_TIME,
		SP_E_L_DIR, /*these light values are non-dynamic light as in classic quake*/
		SP_E_L_MUL,
		SP_E_L_AMBIENT,
		SP_E_EYEPOS, /*viewer's eyepos, in model space*/
		SP_V_EYEPOS, /*viewer's eyepos, in world space*/
		SP_W_FOG,

		SP_M_ENTBONES,
		SP_M_VIEW,
		SP_M_MODEL,
		SP_M_MODELVIEW,
		SP_M_PROJECTION,
		SP_M_MODELVIEWPROJECTION,
		SP_M_INVVIEWPROJECTION,
		SP_M_INVMODELVIEWPROJECTION,

		SP_RENDERTEXTURESCALE,	/*multiplier for currentrender->texcoord*/

		SP_LIGHTRADIUS, /*these light values are realtime lighting*/
		SP_LIGHTCOLOUR,
		SP_LIGHTCOLOURSCALE,
		SP_LIGHTPOSITION,
		SP_LIGHTSCREEN,
		SP_LIGHTPROJMATRIX,
		SP_LIGHTCUBEMATRIX,
		SP_LIGHTSHADOWMAPINFO,

		//things that are set immediatly
		SP_FIRSTIMMEDIATE,	//never set
		SP_CONSTI,
		SP_CONSTF,
		SP_CVARI,
		SP_CVARF,
		SP_CVAR3F,
		SP_TEXTURE
	} type;
	union
	{
		int ival;
		float fval;
		void *pval;
	};
} shaderprogparm_t;

union programhandle_u
{
	int glsl;
#ifdef D3DQUAKE
	struct
	{
		void *vert;
		void *frag;
		void *ctabf;
		void *ctabv;
		void *layout;
	} hlsl;
#endif
};

typedef struct programshared_s
{
	int refs;
	qboolean nofixedcompat;
	int numparams;
	shaderprogparm_t parm[SHADER_PROGPARMS_MAX];
	struct {
		union programhandle_u handle;
		unsigned int attrmask;
		unsigned int parm[SHADER_PROGPARMS_MAX];
	} permu[PERMUTATIONS];
} program_t;

typedef struct {
	float factor;
	float unit;
} polyoffset_t;
struct shader_s
{
	char name[MAX_QPATH];
	int uses;
	int width;
	int height;
	int numpasses;
	texnums_t defaulttextures;
	struct shader_s *next;
	//end of shared fields.

	byte_vec4_t fog_color;
	float fog_dist;
	float portaldist;

	int numdeforms;
	deformv_t	deforms[SHADER_DEFORM_MAX];

	polyoffset_t polyoffset;

	enum {
		SHADER_SKY				= 1 << 0,
		SHADER_NOMIPMAPS		= 1 << 1,
		SHADER_NOPICMIP			= 1 << 2,
		SHADER_CULL_FRONT		= 1 << 3,
		SHADER_CULL_BACK		= 1 << 4,
		SHADER_DEFORMV_BULGE	= 1 << 5,
		SHADER_AUTOSPRITE		= 1 << 6,
		SHADER_FLARE			= 1 << 7,
		SHADER_NOIMAGE			= 1 << 8,
		SHADER_ENTITY_MERGABLE	= 1 << 9,
		SHADER_VIDEOMAP			= 1 << 10,
		SHADER_DEPTHWRITE		= 1 << 11,	//some pass already wrote depth. not used by the renderer.
		SHADER_AGEN_PORTAL		= 1 << 12,
		SHADER_BLEND			= 1 << 13,	//blend or alphatest (not 100% opaque).
		SHADER_NODRAW			= 1 << 14,	//parsed only to pee off developers when they forget it on no-pass shaders.

		SHADER_NODLIGHT			= 1 << 15,	//from surfaceflags
		SHADER_HASLIGHTMAP		= 1 << 16,
		SHADER_HASTOPBOTTOM		= 1 << 17,
		SHADER_STATICDATA		= 1 << 18,	//set if true: no deforms, no tcgen, rgbgen=identitylighting, alphagen=identity, tmu0=st + tmu1=lm(if available) for every pass, no norms
		SHADER_HASREFLECT		= 1 << 19,	//says that we need to generate a reflection image first
		SHADER_HASREFRACT		= 1 << 20,	//says that we need to generate a refraction image first
		SHADER_HASREFRACTDEPTH	= 1 << 21,	//refraction generation needs to generate a depth texture too.
		SHADER_HASNORMALMAP		= 1 << 22,	//says that we need to load a normalmap texture
		SHADER_HASRIPPLEMAP		= 1 << 23,	//water surface disturbances for water splashes
		SHADER_HASGLOSS			= 1 << 24,	//
	} flags;

	program_t *prog;

	shaderpass_t passes[SHADER_PASS_MAX];

	shadersort_t sort;

	skydome_t	*skydome;
	shader_gen_t *generator;
	char	*genargs;

	meshfeatures_t features;
	bucket_t bucket;
};

#define MAX_SHADERS 2048	//fixme: this takes a lot of bss in the r_shaders list
extern shader_t	*r_shaders;
extern int be_maxpasses;


void R_UnloadShader(shader_t *shader);
shader_t *R_RegisterPic (char *name);
shader_t *R_RegisterShader (char *name, const char *shaderscript);
shader_t *R_RegisterShader_Lightmap (char *name);
shader_t *R_RegisterShader_Vertex (char *name);
shader_t *R_RegisterShader_Flare (char *name);
shader_t *R_RegisterSkin  (char *shadername, char *modname);
shader_t *R_RegisterCustom (char *name, shader_gen_t *defaultgen, const void *args);
void R_BuildDefaultTexnums(texnums_t *tn, shader_t *shader);

cin_t *R_ShaderGetCinematic(shader_t *s);
cin_t *R_ShaderFindCinematic(char *name);

void Shader_DefaultSkinShell(char *shortname, shader_t *s, const void *args);
void Shader_DefaultBSPLM(char *shortname, shader_t *s, const void *args);
void Shader_DefaultBSPQ1(char *shortname, shader_t *s, const void *args);
void Shader_DefaultBSPQ2(char *shortname, shader_t *s, const void *args);
void Shader_DefaultSkybox(char *shortname, shader_t *s, const void *args);
void Shader_DefaultCinematic(char *shortname, shader_t *s, const void *args);
void Shader_DefaultScript(char *shortname, shader_t *s, const void *args);

void Shader_DoReload(void);
void Shader_Shutdown (void);
qboolean Shader_Init (void);
void Shader_NeedReload(qboolean rescanfs);
void Shader_WriteOutGenerics_f(void);

mfog_t *CM_FogForOrigin(vec3_t org);

#define BEF_FORCEDEPTHWRITE		1
#define BEF_FORCEDEPTHTEST		2
#define BEF_FORCEADDITIVE		4	//blend dest = GL_ONE
#define BEF_FORCETRANSPARENT	8	//texenv replace -> modulate
#define BEF_FORCENODEPTH		16	//disables any and all depth.
#define BEF_PUSHDEPTH			32	//additional polygon offset
#define BEF_NODLIGHT            64  //don't use a dlight pass
#define BEF_NOSHADOWS			128 //don't appear in shadows
#define BEF_FORCECOLOURMOD		256 //q3 shaders default to 'rgbgen identity', and ignore ent colours. this forces ent colours to be considered
#define BEF_LINES				512	//draw line pairs instead of triangles.

#ifdef GLQUAKE
void GLBE_Init(void);
void GLBE_Shutdown(void);
void GLBE_SelectMode(backendmode_t mode);
void GLBE_DrawMesh_List(shader_t *shader, int nummeshes, mesh_t **mesh, vbo_t *vbo, texnums_t *texnums, unsigned int beflags);
void GLBE_DrawMesh_Single(shader_t *shader, mesh_t *meshchain, vbo_t *vbo, texnums_t *texnums, unsigned int beflags);
void GLBE_SubmitBatch(batch_t *batch);
batch_t *GLBE_GetTempBatch(void);
void GLBE_GenBrushModelVBO(model_t *mod);
void GLBE_ClearVBO(vbo_t *vbo);
void GLBE_UploadAllLightmaps(void);
void GLBE_DrawWorld (qboolean drawworld, qbyte *vis);
qboolean GLBE_LightCullModel(vec3_t org, model_t *model);
void GLBE_SelectEntity(entity_t *ent);
void GLBE_SelectDLight(dlight_t *dl, vec3_t colour);
void GLBE_SubmitMeshes (qboolean drawworld, int start, int stop);
void GLBE_RenderToTexture(texid_t sourcecol, texid_t sourcedepth, texid_t destcol, texid_t destdepth, qboolean usedepth);
#endif
#ifdef D3D9QUAKE
void D3D9BE_Init(void);
void D3D9BE_Shutdown(void);
void D3D9BE_SelectMode(backendmode_t mode);
void D3D9BE_DrawMesh_List(shader_t *shader, int nummeshes, mesh_t **mesh, vbo_t *vbo, texnums_t *texnums, unsigned int beflags);
void D3D9BE_DrawMesh_Single(shader_t *shader, mesh_t *meshchain, vbo_t *vbo, texnums_t *texnums, unsigned int beflags);
void D3D9BE_SubmitBatch(batch_t *batch);
batch_t *D3D9BE_GetTempBatch(void);
void D3D9BE_GenBrushModelVBO(model_t *mod);
void D3D9BE_ClearVBO(vbo_t *vbo);
void D3D9BE_UploadAllLightmaps(void);
void D3D9BE_DrawWorld (qboolean drawworld, qbyte *vis);
qboolean D3D9BE_LightCullModel(vec3_t org, model_t *model);
void D3D9BE_SelectEntity(entity_t *ent);
void D3D9BE_SelectDLight(dlight_t *dl, vec3_t colour);

qboolean D3D9Shader_CreateProgram (program_t *prog, char *sname, int permu, char **precompilerconstants, char *vert, char *frag);
int D3D9Shader_FindUniform(union programhandle_u *h, int type, char *name);
void D3D9Shader_Init(void);
void D3D9BE_Reset(qboolean before);
#endif
#ifdef D3D11QUAKE
void D3D11BE_Init(void);
void D3D11BE_Shutdown(void);
void D3D11BE_SelectMode(backendmode_t mode);
void D3D11BE_DrawMesh_List(shader_t *shader, int nummeshes, mesh_t **mesh, vbo_t *vbo, texnums_t *texnums, unsigned int beflags);
void D3D11BE_DrawMesh_Single(shader_t *shader, mesh_t *meshchain, vbo_t *vbo, texnums_t *texnums, unsigned int beflags);
void D3D11BE_SubmitBatch(batch_t *batch);
batch_t *D3D11BE_GetTempBatch(void);
void D3D11BE_GenBrushModelVBO(model_t *mod);
void D3D11BE_ClearVBO(vbo_t *vbo);
void D3D11BE_UploadAllLightmaps(void);
void D3D11BE_DrawWorld (qboolean drawworld, qbyte *vis);
qboolean D3D11BE_LightCullModel(vec3_t org, model_t *model);
void D3D11BE_SelectEntity(entity_t *ent);
void D3D11BE_SelectDLight(dlight_t *dl, vec3_t colour);

qboolean D3D11Shader_CreateProgram (program_t *prog, const char *name, int permu, char **precompilerconstants, char *vert, char *frag);
int D3D11Shader_FindUniform(union programhandle_u *h, int type, char *name);
void D3D11Shader_Init(void);
void D3D11BE_Reset(qboolean before);
void D3D11BE_SetupViewCBuffer(void);
void D3D11_UploadLightmap(lightmapinfo_t *lm);
#endif

//Asks the backend to invoke DrawMeshChain for each surface, and to upload lightmaps as required
void BE_DrawNonWorld (void);

//Builds a hardware shader from the software representation
void BE_GenerateProgram(shader_t *shader);

#ifdef RTLIGHTS
//
void GLBE_PushOffsetShadow(qboolean foobar);
//sets up gl for depth-only FIXME
void GLBE_SetupForShadowMap(texid_t shadowmaptex, int texwidth, int texheight, float shadowscale);
//Called from shadowmapping code into backend
void GLBE_BaseEntTextures(void);
void D3D9BE_BaseEntTextures(void);
//prebuilds shadow volumes
void Sh_PreGenerateLights(void);
//Draws lights, called from the backend
void Sh_DrawLights(qbyte *vis);
void SH_FreeShadowMesh(struct shadowmesh_s *sm);
//frees all memory
void Sh_Shutdown(void);
//resize any textures to match new screen resize
void Sh_Reset(void);
#endif

struct shader_field_names_s
{
	char *name;
	enum shaderprogparmtype_e ptype;
};
extern struct shader_field_names_s shader_field_names[];
#endif
