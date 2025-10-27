#include "quakedef.h"
#include "server.h"
#include "view.h"
#include "client.h"
#include "r_local.h"
#include "render.h"
#include "host.h"
#include "pr_cmds.h"
#include "cl_main.h"
#include "screen.h"
#include "pm_shared/pm_movevars.h"
#include "cl_demo.h"
#include "chase.h"

#if defined(GLQUAKE)
#include "vid.h"
#include "glquake.h"
#include "gl_screen.h"
#endif

#if defined(GLQUAKE)
float v_blend[4];
byte ramps[3][256];
#endif
int lightgammatable[1024];
float v_lambert1 = 1.4953241f;

cvar_t		lcd_x = { const_cast<char*>("lcd_x"), const_cast<char*>("0") };
cvar_t		lcd_yaw = { const_cast<char*>("lcd_yaw"), const_cast<char*>("0") };

cvar_t	scr_ofsx = { const_cast<char*>("scr_ofsx"), const_cast<char*>("0") };
cvar_t	scr_ofsy = { const_cast<char*>("scr_ofsy"), const_cast<char*>("0") };
cvar_t	scr_ofsz = { const_cast<char*>("scr_ofsz"), const_cast<char*>("0") };

cvar_t	cl_rollspeed = { const_cast<char*>("cl_rollspeed"), const_cast<char*>("200") };
cvar_t	cl_rollangle = { const_cast<char*>("cl_rollangle"), const_cast<char*>("2.0") };

cvar_t	cl_bob = { const_cast<char*>("cl_bob"), const_cast<char*>("0.02") };
cvar_t	cl_bobcycle = { const_cast<char*>("cl_bobcycle"), const_cast<char*>("0.6") };
cvar_t	cl_bobup = { const_cast<char*>("cl_bobup"), const_cast<char*>("0.5") };

cvar_t	v_kicktime = { const_cast<char*>("v_kicktime"), const_cast<char*>("0.5") };
cvar_t	v_kickroll = { const_cast<char*>("v_kickroll"), const_cast<char*>("0.6") };
cvar_t	v_kickpitch = { const_cast<char*>("v_kickpitch"), const_cast<char*>("0.6") };

cvar_t	v_iyaw_cycle = { const_cast<char*>("v_iyaw_cycle"), const_cast<char*>("2") };
cvar_t	v_iroll_cycle = { const_cast<char*>("v_iroll_cycle"), const_cast<char*>("0.5") };
cvar_t	v_ipitch_cycle = { const_cast<char*>("v_ipitch_cycle"), const_cast<char*>("1") };
cvar_t	v_iyaw_level = { const_cast<char*>("v_iyaw_level"), const_cast<char*>("0.3") };
cvar_t	v_iroll_level = { const_cast<char*>("v_iroll_level"), const_cast<char*>("0.1") };
cvar_t	v_ipitch_level = { const_cast<char*>("v_ipitch_level"), const_cast<char*>("0.3") };

cvar_t	v_idlescale = { const_cast<char*>("v_idlescale"), const_cast<char*>("0") };

cvar_t	crosshair = { const_cast<char*>("crosshair"), const_cast<char*>("0"), FCVAR_ARCHIVE };
cvar_t	cl_crossx = { const_cast<char*>("cl_crossx"), const_cast<char*>("0") };
cvar_t	cl_crossy = { const_cast<char*>("cl_crossy"), const_cast<char*>("0") };

cvar_t v_dark = { const_cast<char*>("v_dark"), const_cast<char*>("0") };
cvar_t v_gamma = { const_cast<char*>("v_gamma"), const_cast<char*>("2.5"), FCVAR_ARCHIVE };
cvar_t v_lightgamma = { const_cast<char*>("v_lightgamma"), const_cast<char*>("2.5") };
cvar_t v_texgamma = { const_cast<char*>("v_texgamma"), const_cast<char*>("2.0") };
cvar_t v_brightness = { const_cast<char*>("brightness"), const_cast<char*>("0"), FCVAR_ARCHIVE };
cvar_t v_lambert = { const_cast<char*>("v_lambert"), const_cast<char*>("1.5") };
cvar_t v_direct = { const_cast<char*>("direct"), const_cast<char*>("0.9") };

vec3_t forward, right, up;

//-----------------------------------------------------------------------------
// Gamma conversion support
//-----------------------------------------------------------------------------
int			lineargammatable[1024];	// linear (0..1) to texture (0..255)
byte		texgammatable[256];	// palette is sent through this to convert to screen gamma
int			screengammatable[1024];	// linear (0..1) to gamma corrected vertex light (0..255)

struct
{
	float time;
	float duration;
	float amplitude;
	float frequency;
	float nextShake;
	vec3_t offset;
	float angle;
	vec3_t appliedOffset;
	float appliedAngle;
} gVShake;

void FilterLightParams()
{
	if (g_bIsCStrike)
	{
		Cvar_DirectSet(&v_lightgamma, const_cast<char*>("2.5"));
		Cvar_DirectSet(&v_texgamma, const_cast<char*>("2.0"));
	}

	if (Host_GetMaxClients() > 1 && v_brightness.value > 2.0)
		Cvar_DirectSet(&v_brightness, const_cast<char*>("2.0"));

	if (v_gamma.value < 1.8)
		Cvar_DirectSet(&v_gamma, const_cast<char*>("1.8"));
	else if (v_gamma.value > 3.0)
		Cvar_DirectSet(&v_gamma, const_cast<char*>("3.0"));

	if (v_texgamma.value < 1.8)
		Cvar_DirectSet(&v_texgamma, const_cast<char*>("1.8"));
	else if (v_texgamma.value > 3.0)
		Cvar_DirectSet(&v_texgamma, const_cast<char*>("3.0"));

	if (v_lightgamma.value < 1.8)
		Cvar_DirectSet(&v_lightgamma, const_cast<char*>("1.8"));
	else if (v_lightgamma.value > 3.0)
		Cvar_DirectSet(&v_lightgamma, const_cast<char*>("3.0"));

	if (v_brightness.value < 0.0)
		Cvar_DirectSet(&v_lightgamma, const_cast<char*>("100.0"));
	else if (v_brightness.value > 3.0)
		Cvar_DirectSet(&v_lightgamma, const_cast<char*>("3.0"));
}

void BuildGammaTable(float g)
{
	int		i, inf;
	float	g1, g3;

	// Con_Printf("BuildGammaTable %.1f %.1f %.1f\n", g, v_lightgamma.value, v_texgamma.value );

	if (!g)
		g = 2.5;

	g = 1.0 / g;
	g1 = v_texgamma.value * g; 

	if (v_brightness.value <= 0.0) 
	{
		g3 = 0.125;
	}
	else if (v_brightness.value > 1.0)
	{
		g3 = 0.05;
	}
	else 
	{
		g3 = 0.125 - (v_brightness.value * v_brightness.value) * 0.075;
	}

	for (i=0 ; i<256 ; i++)
	{
		inf = 255 * pow ( i/255.f, g1 ); 
		if (inf < 0)
			inf = 0;
		if (inf > 255)
			inf = 255;
		texgammatable[i] = inf;
	}

	for (i=0 ; i<1024 ; i++)
	{
		float f;

		f = pow(i / 1023.0f, v_lightgamma.value);

		// scale up
		if (v_brightness.value > 1.0)
			f = f * v_brightness.value;

		// shift up
		if (f <= g3)
			f = (f / g3) * 0.125;
		else 
			f = 0.125 + ((f - g3) / (1.0 - g3)) * 0.875;

		// convert linear space to desired gamma space
		inf = 1023 * pow ( f, g ); 

		if (inf < 0)
			inf = 0;
		if (inf > 1023)
			inf = 1023;
		lightgammatable[i] = inf;
	}

	
	for (i=0 ; i<1024 ; i++)
	{
		// convert from screen gamma space to linear space
		lineargammatable[i] = 1023 * pow ( i/1023.0f, v_gamma.value );
		// convert from linear gamma space to screen space
		screengammatable[i] = 1023 * pow ( i/1023.0f, 1.0f / v_gamma.value );
	}
}

qboolean V_CheckGamma()
{
	static float oldgammavalue, oldlightgamma, oldtexgamma, oldbrightness
#if !defined (GLQUAKE)
		, ambientr, ambientg, ambientb
#endif
		;

	FilterLightParams();

	if (v_gamma.value == oldgammavalue
		&& v_lightgamma.value == oldlightgamma
		&& v_texgamma.value == oldtexgamma
		&& v_brightness.value == oldbrightness
#if !defined(GLQUAKE)
		&& r_ambient_r.value == ambientr
		&& r_ambient_g.value == ambientg
		&& r_ambient_b.value == ambientb
#endif
		)
	{
		return false;
	}

	BuildGammaTable(v_gamma.value);

	oldgammavalue = v_gamma.value;
	oldlightgamma = v_lightgamma.value;
	oldtexgamma = v_texgamma.value;
	oldbrightness = v_brightness.value;

#if !defined(GLQUAKE)
	ambientr = r_ambient_r.value;
	ambientg = r_ambient_g.value;
	ambientb = r_ambient_b.value;
#else
	V_CalcBlend();
#endif

	D_FlushCaches();
	vid.recalc_refdef = 1;				// force a surface cache flush

	return true;

}

#if defined(GLQUAKE)
void V_CalcBlend()
{
	v_blend[0] = 0.0f;
	v_blend[1] = 0.0f;
	v_blend[2] = 0.0f;
	v_blend[3] = 0.0f;
}
#endif

void V_UpdatePalette()
{
#if defined(GLQUAKE)
	if (V_CheckGamma())
	{
		V_CalcBlend();

		float a = v_blend[3];
		float r = 255.0 * v_blend[0] * a;
		float g = 255.0 * v_blend[1] * a;
		float b = 255.0 * v_blend[2] * a;

		a = 1.0 - a;
		for (int i = 0; i < 256; i++)
		{
			int ir = r + i * a;
			int ig = g + i * a;
			int ib = b + i * a;
			if (ir > 255)
				ir = 255;
			if (ig > 255)
				ig = 255;
			if (ib > 255)
				ib = 255;

			ramps[0][i] = texgammatable[ir];
			ramps[1][i] = texgammatable[ig];
			ramps[2][i] = texgammatable[ib];
		}
	}
#else
	V_CheckGamma();
#endif
}

void V_SetRefParams(ref_params_t *pparams)
{
	Q_memset(pparams, 0, sizeof(ref_params_t));
	VectorCopy(r_refdef.vieworg, pparams->vieworg);
	VectorCopy(r_refdef.viewangles, pparams->viewangles);
	VectorCopy(forward, pparams->forward);
	VectorCopy(right, pparams->right);
	VectorCopy(up, pparams->up);
	pparams->time = cl.time;
	pparams->frametime = host_frametime;
	pparams->intermission = cl.intermission != 0;
	pparams->paused = cl.paused != false;
	pparams->spectator = cls.spectator != false;
	pparams->onground = cl.onground != -1;
	pparams->waterlevel = cl.waterlevel;
	VectorCopy(cl.simvel, pparams->simvel);
	VectorCopy(cl.simorg, pparams->simorg);
	VectorCopy(cl.viewheight, pparams->viewheight);
	VectorCopy(cl.viewangles, pparams->cl_viewangles);
	pparams->idealpitch = cl.idealpitch;
	pparams->health = cl.stats[STAT_HEALTH];
	VectorCopy(cl.crosshairangle, pparams->crosshairangle);
	VectorCopy(cl.punchangle, pparams->punchangle);
	pparams->viewsize = scr_viewsize.value;
	pparams->maxclients = cl.maxclients;
	pparams->viewentity = cl.viewentity;
	pparams->playernum = cl.playernum;
	pparams->max_entities = cl.max_edicts;
	pparams->cmd = &cl.cmd;
	pparams->demoplayback = cls.demoplayback;
	pparams->movevars = &movevars;
	pparams->hardware = 1;
	pparams->smoothing = cl.pushmsec;
	pparams->viewport[0] = 0;
	pparams->viewport[1] = 0;
	pparams->viewport[2] = vid.width;
	pparams->viewport[3] = vid.height;
	pparams->nextView = 0;
	pparams->onlyClientDraw = 0;
}

void V_GetRefParams(ref_params_t *pparams)
{
	VectorCopy(pparams->vieworg, r_refdef.vieworg);
	VectorCopy(pparams->viewangles, r_refdef.viewangles);
	VectorCopy(pparams->forward, forward);
	VectorCopy(pparams->right, right);
	VectorCopy(pparams->up, up);
	VectorCopy(pparams->simvel, cl.simvel);
	VectorCopy(pparams->simorg, cl.simorg);
	VectorCopy(pparams->cl_viewangles, cl.viewangles);
	VectorCopy(pparams->crosshairangle, cl.crosshairangle);
	VectorCopy(pparams->viewheight, cl.viewheight);
	VectorCopy(pparams->punchangle, cl.punchangle);
	r_refdef.vrect.x = pparams->viewport[0];
	r_refdef.vrect.y = pparams->viewport[1];
	r_refdef.vrect.width = pparams->viewport[2];
	r_refdef.vrect.height = pparams->viewport[3];
	r_refdef.onlyClientDraws = pparams->onlyClientDraw;
}

void V_RenderView(void)
{
	model_t *clmodel;
	ref_params_s angles;
#if defined(GLQUAKE)
	int viewnum;
#endif
	
	r_soundOrigin[0] = r_soundOrigin[1] = r_soundOrigin[2] = 0.0;
	r_playerViewportAngles[0] = r_playerViewportAngles[1] = r_playerViewportAngles[2] = 0.0;

	if (con_forcedup || cls.state != ca_active || cls.signon != SIGNONS)
		return;

	clmodel = CL_GetModelByIndex(cl.stats[STAT_WEAPON]);

	cl.viewent.curstate.frame = 0.0;
	cl.viewent.model = clmodel;
	cl.viewent.curstate.modelindex = cl.stats[STAT_WEAPON];
	cl.viewent.curstate.colormap = 0;
	cl.viewent.index = cl.playernum + 1;

	V_SetRefParams(&angles);

#if defined(GLQUAKE)
	viewnum = 0;
	do
	{
#endif
		ClientDLL_CalcRefdef(&angles);

#if defined(GLQUAKE)
		if (viewnum == 0)
#endif
		if (cls.demoplayback || (CL_SetDemoViewInfo(&angles, cl.viewent.origin, cl.stats[STAT_WEAPON]), cls.demoplayback))
		{
			if (!cls.spectator)
			{
				CL_GetDemoViewInfo(&angles, cl.viewent.origin, &cl.stats[STAT_WEAPON]);
				cl.viewent.angles[0] = -angles.viewangles[0];
				cl.viewent.angles[1] = angles.viewangles[1];
				cl.viewent.angles[2] = angles.viewangles[2];
				cl.viewent.curstate.angles[0] = cl.viewent.angles[0];
				cl.viewent.curstate.angles[1] = angles.viewangles[1];
				cl.viewent.curstate.angles[2] = angles.viewangles[2];
				cl.viewent.latched.prevangles[0] = cl.viewent.angles[0];
				cl.viewent.latched.prevangles[1] = angles.viewangles[1];
				cl.viewent.latched.prevangles[2] = angles.viewangles[2];
				angles.nextView = 0;
			}
		}

		V_GetRefParams(&angles);
		if (angles.intermission)
		{
			cl.viewent.model = 0;
		}
		else if (!angles.paused && chase_active.value != 0.0)
		{
			Chase_Update();
		}
		R_PushDlights();

#if defined(GLQUAKE)
		if (viewnum == 0 && r_refdef.onlyClientDraws)
		{
			qglClearColor(0.0f, 0.0f, 0.0f, 0.0f);
			qglClear(GL_COLOR_BUFFER_BIT);
		}
#endif

		if (lcd_x.value == 0.0)
		{
			R_RenderView();
		}
		else
		{
			vid.rowbytes *= 2;
			vid.aspect /= 2;
			r_refdef.viewangles[YAW] -= lcd_yaw.value;

			for (int i = 0; i < 3; i++)
				r_refdef.vieworg[i] = r_refdef.vieworg[i] - angles.right[i] * lcd_x.value;

			R_RenderView();

			vid.buffer += vid.rowbytes >> 1;

			R_PushDlights();

			r_refdef.viewangles[YAW] += lcd_yaw.value * 2;

			for (int i = 0; i < 3; i++)
				r_refdef.vieworg[i] = (angles.right[i] * 2) * lcd_x.value + r_refdef.vieworg[i];

			R_RenderView();

			r_refdef.vrect.height *= 2;
			vid.rowbytes /= 2;
			vid.buffer -= vid.rowbytes;
			vid.aspect *= 2;
		}

		if (!angles.onlyClientDraw)
		{
			VectorCopy(r_origin, r_soundOrigin);
			VectorCopy(angles.viewangles, r_playerViewportAngles);
		}
#if defined(GLQUAKE)
		viewnum++;
	} while (angles.nextView != 0);
#endif
}

void V_CalcShake()
{
	float	frametime, curtime;
	int		i;
	float	fraction, freq;

	RecEngV_CalcShake();

	if ((cl.time > gVShake.time) ||
		gVShake.duration <= 0 ||
		gVShake.amplitude <= 0 ||
		gVShake.frequency <= 0)
	{
		memset(&gVShake, 0, sizeof(gVShake));
		return;
	}

	curtime = cl.time;
	frametime = curtime - cl.oldtime;

	if (curtime > gVShake.nextShake)
	{
		// Higher frequency means we recalc the extents more often and perturb the display again
		gVShake.nextShake = curtime + (gVShake.duration / gVShake.frequency);

		// Compute random shake extents (the shake will settle down from this)
		for (i = 0; i < 3; i++)
		{
			gVShake.offset[i] = RandomFloat(-gVShake.amplitude, gVShake.amplitude);
		}

		gVShake.angle = RandomFloat(-gVShake.amplitude*0.25, gVShake.amplitude*0.25);
	}

	// Ramp down amplitude over duration (fraction goes from 1 to 0 linearly with slope 1/duration)
	fraction = (curtime - gVShake.time) / gVShake.duration;

	// Ramp up frequency over duration
	if (fraction)
	{
		freq = (gVShake.frequency / fraction);
	}
	else
	{
		freq = 0;
	}

	// square fraction to approach zero more quickly
	fraction *= fraction;

	// Sine wave that slowly settles to zero
	fraction = fraction * sin(curtime * freq);

	// Add to view origin
	for (i = 0; i < 3; i++)
	{
		gVShake.appliedOffset[i] = gVShake.offset[i] * fraction;
	}

	// Add to roll
	gVShake.appliedAngle = gVShake.angle * fraction;

	// Drop amplitude a bit, less for higher frequency shakes
	gVShake.amplitude -= gVShake.amplitude * (frametime / (gVShake.duration * gVShake.frequency));

}

void V_ApplyShake( float* origin, float* angles, float factor )
{
	RecEngV_ApplyShake(origin, angles, factor);
	if (origin)
		VectorMA(origin, factor, gVShake.appliedOffset, origin);
	if (angles)
		angles[ROLL] += gVShake.appliedAngle * factor;
}

int V_ScreenShake( const char* pszName, int iSize, void* pbuf )
{
	gVShake.duration = (float)((word*)pbuf)[1] / 4096.f;
	gVShake.time = gVShake.duration + cl.time;
	gVShake.amplitude = max((float)((word*)pbuf)[0] / 4096.f, gVShake.amplitude);
	gVShake.nextShake = 0.0;
	gVShake.frequency = (float)((word*)pbuf)[2] / 256.f;
	return 1;
}

int V_ScreenFade( const char* pszName, int iSize, void* pbuf )
{
	int flags;
	ScreenFade *sf = (ScreenFade*)pbuf;
	if (!pbuf)
		return 1;

	flags = sf->fadeFlags;

	if (flags & FFADE_LONGFADE)
	{
		cl.sf.fadeEnd = (float)sf->duration / 256.0f;
		cl.sf.fadeReset = (float)sf->holdTime / 256.0f;
	}
	else
	{
		cl.sf.fadeEnd = (float)sf->duration / 4096.0f;
		cl.sf.fadeReset = (float)sf->holdTime / 4096.0f;
	}

	cl.sf.fader = sf->r;
	cl.sf.fadeg = sf->g;
	cl.sf.fadeb = sf->b;
	cl.sf.fadealpha = sf->a;
	cl.sf.fadeSpeed = 0.0f;
	cl.sf.fadeFlags = flags;

	if (!sf->duration)
		return 1;

	if (cl.sf.fadeFlags & FFADE_OUT)
	{
		if (cl.sf.fadeEnd)
		{
			cl.sf.fadeSpeed = -(float)cl.sf.fadealpha / cl.sf.fadeEnd;
		}

		cl.sf.fadeTotalEnd = cl.sf.fadeEnd += cl.time;
		cl.sf.fadeReset += cl.sf.fadeEnd;
	}
	else
	{
		if (cl.sf.fadeEnd)
		{
			cl.sf.fadeSpeed = (float)cl.sf.fadealpha / cl.sf.fadeEnd;
		}

		cl.sf.fadeReset += cl.time;
		cl.sf.fadeEnd += cl.sf.fadeReset;
	}

	return 1;
}

int V_FadeAlpha()
{
	int iFadeAlpha;

	if (cl.sf.fadeReset < cl.time && cl.sf.fadeEnd < cl.time && !(cl.sf.fadeFlags & FFADE_STAYOUT))
		return 0;

	if (!(cl.sf.fadeFlags & FFADE_STAYOUT))
	{
		iFadeAlpha = cl.sf.fadeSpeed * (cl.sf.fadeEnd - cl.time);
		if (cl.sf.fadeFlags & FFADE_OUT)
		{
			iFadeAlpha += cl.sf.fadealpha;
		}
	}
	else
	{
		iFadeAlpha = cl.sf.fadealpha;
		
		if (cl.sf.fadeFlags & FFADE_OUT && cl.sf.fadeTotalEnd > cl.time)
		{
			iFadeAlpha += (cl.sf.fadeTotalEnd - cl.time) * cl.sf.fadeSpeed;
		}
		else
		{
			cl.sf.fadeEnd = cl.time + 0.1;
		}
	}

	iFadeAlpha = min(iFadeAlpha, (int)cl.sf.fadealpha);
	iFadeAlpha = max(0, iFadeAlpha);

	return iFadeAlpha;
}

void V_Init()
{
	Cvar_RegisterVariable(&v_dark);
	Cvar_RegisterVariable(&crosshair);
	Cvar_RegisterVariable(&v_gamma);
	Cvar_RegisterVariable(&v_lightgamma);
	Cvar_RegisterVariable(&v_texgamma);
	Cvar_RegisterVariable(&v_brightness);
	Cvar_RegisterVariable(&v_lambert);
	Cvar_RegisterVariable(&v_direct);
	BuildGammaTable(2.5f);
}

void V_InitLevel()
{
	Q_memset(&gVShake, 0, sizeof(gVShake));

	cl.sf.fader = cl.sf.fadeg = cl.sf.fadeb = 0;
	cl.sf.fadeFlags = 0;

	if (v_dark.value == 0.0)
	{
		cl.sf.fadeSpeed = 0.0;
		cl.sf.fadeEnd = 0.0;
		cl.sf.fadeReset = 0.0;
		cl.sf.fadealpha = 0;
	}
	else
	{
		cl.sf.fadeReset = cl.time + 5.0;
		cl.sf.fadealpha = 255;
		cl.sf.fadeSpeed = 51.0f;
		cl.sf.fadeEnd = cl.sf.fadeReset + 5.0f;
		Cvar_DirectSet(&v_dark, const_cast < char*>("0"));
	}
}
