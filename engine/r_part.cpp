#include "quakedef.h"
#include "r_part.h"
#include "render.h"
#include "server.h"
#include "r_local.h"
#include "beamdef.h"
#include "client.h"
#include "host.h"
#include "pr_cmds.h"
#include "cl_main.h"
#include "r_studioint.h"
#include "cl_tent.h"
#include "customentity.h"
#include "r_triangle.h"
#include "pm_movevars.h"
#include "draw.h"

#define MAX_PARTICLES			2048	// default max # of particles at one
#define NOISE_DIVS				128
#define MAX_BEAMS				128
//  time
#define ABSOLUTE_MIN_PARTICLES	512		// no fewer than this no matter what's
										//  on the command line

int		ramp1[8] = { 0x6f, 0x6d, 0x6b, 0x69, 0x67, 0x65, 0x63, 0x61 };
int		ramp2[8] = { 0x6f, 0x6e, 0x6d, 0x6c, 0x6b, 0x6a, 0x68, 0x66 };
int		ramp3[8] = { 0x6d, 0x6b, 6, 5, 4, 3 };
int		gSparkRamp[9] = { 0xFE, 0xFD, 0xFC, 0x6F, 0x6E, 0x6D, 0x6C, 0x67, 0x60 };
color24 gTracerColors[] =
{
	{ 255, 255, 255 },		// White
	{ 255, 0, 0 },			// Red
	{ 0, 255, 0 },			// Green
	{ 0, 0, 255 },			// Blue
	{ 0, 0, 0 },			// Tracer default, filled in from cvars, etc.
	{ 255, 167, 17 },		// Yellow-orange sparks
	{ 255, 130, 90 },		// Yellowish streaks (garg)
	{ 55, 60, 144 },		// Blue egon streak
	{ 255, 130, 90 },		// More Yellowish streaks (garg)
	{ 255, 140, 90 },		// More Yellowish streaks (garg)
	{ 200, 130, 90 },		// More red streaks (garg)
	{ 255, 120, 70 },		// Darker red streaks (garg)
};
const float gTracerSize[10] = {
	1.5, 0.5, 1, 1, 1, 1, 1, 1, 1, 1
};

particle_t* active_particles, * free_particles;
particle_t *gpActiveTracers;
particle_t* particles;
int			r_numparticles;

BEAM* gBeams, *gpFreeBeams, *gpActiveBeams;
cl_entity_t* cl_beamentities[MAX_MODEL_BEAMS];
int cl_numbeamentities;

EXTERN_C
{
vec3_t			r_pright, r_pup, r_ppn;
};
vec_t avelocities[NUMVERTEXNORMALS][3];
float beamlength = 16;
float gNoise[NOISE_DIVS + 1];

void SetBeamAttributes(BEAM* pbeam, float r, float g, float b, float framerate, int startFrame);
int R_BeamCull(vec_t* start, vec_t* end, int pvsOnly);
void R_FreeDeadParticles(particle_t** ppparticles);

particle_t* R_AllocParticle( void( *callback )( particle_t*, float ) )
{
	particle_t *ap, *head;

	head = free_particles;

	if (free_particles == NULL)
	{
		Con_Printf(const_cast<char*>("Not enough free particles\n"));
		return nullptr;
	}

	free_particles->type = pt_clientcustom;
	free_particles = free_particles->next;
	ap = active_particles;
	active_particles = head;
	head->next = ap;
	head->die = cl.time;
	head->color = 0;
	head->packedColor = 0;
	head->callback = callback;
	head->deathfunc = NULL;
	head->ramp = 0.0;
	VectorCopy(vec3_origin, head->org);
	VectorCopy(vec3_origin, head->vel);

	return head;
}

void R_BlobExplosion( vec_t* org )
{
	particle_t* ap;
	for (int i = 0; i < 1024 && free_particles != NULL; i++, free_particles = free_particles->next)
	{
		ap = active_particles;
		active_particles = free_particles;
		active_particles->next = ap;
		active_particles->die = RandomFloat(1.0, 1.4) + cl.time;

		if (i & 1)
		{
			active_particles->type = pt_blob;
			active_particles->color = RandomLong(66, 71);
		}
		else
		{
			active_particles->type = pt_blob2;
			active_particles->color = RandomLong(150, 155);
		}
#if defined( GLQUAKE )
		active_particles->packedColor = 0;
#else
		active_particles->packedColor = hlRGB(host_basepal, active_particles->color);
#endif
	
		for (int j = 0; j < 3; j++)
		{
			active_particles->org[j] = RandomFloat(-16.0, 16.0) + org[j];
			active_particles->vel[j] = RandomFloat(-256.0, 256.0);
		}
	}
}

void R_Blood( vec_t* org, vec_t* dir, int pcolor, int speed )
{
	vec3_t dirCopy, dorg;
	particle_t* fp;

	for (int count = 0, pspeed = speed * 3; count < speed / 2 && free_particles != NULL; count++, pspeed -= speed)
	{
		dirCopy[0] = RandomFloat(-0.06, 0.06) + dir[0];
		dirCopy[1] = RandomFloat(-0.06, 0.06) + dir[1];
		dirCopy[2] = RandomFloat(-0.06, 0.06) + dir[2];

		dorg[0] = RandomFloat(-3.0, 3.0) + org[0];
		dorg[1] = RandomFloat(-3.0, 3.0) + org[1];
		dorg[2] = RandomFloat(-3.0, 3.0) + org[2];

		for (int i = 0; i < 8 && free_particles != NULL; i++, free_particles = free_particles->next)
		{
			fp = free_particles;
			fp->next = active_particles;
			active_particles = fp;

			fp->die = cl.time + 1.5;
			fp->type = pt_vox_grav;
			fp->color = pcolor + RandomLong(0, 9);
#if defined( GLQUAKE )
			fp->packedColor = 0;
#else
			fp->packedColor = hlRGB(host_basepal, fp->color);
#endif

			fp->org[0] = RandomFloat(-1.0, 1.0) + dorg[0];
			fp->org[1] = RandomFloat(-1.0, 1.0) + dorg[1];
			fp->org[2] = RandomFloat(-1.0, 1.0) + dorg[2];

			VectorScale(dirCopy, pspeed, fp->vel);
		}

	}
}

void R_BloodStream( vec_t* org, vec_t* dir, int pcolor, int speed )
{
	// stopline
	vec3_t dirCopy, dorg;
	particle_t *fp, *ap;
	int num, count;
	float arc, lHigh, fRand;

	VectorNormalize(dir);
	arc = 0.05;
	lHigh = (float)speed;

	for (num = 0; num < 100; num++)
	{
		fp = free_particles;

		if (free_particles == NULL)
			return;

		free_particles = free_particles->next;
		fp->next = active_particles;
		active_particles = fp;
		fp->die = cl.time + 2.0;

		fp->type = pt_vox_grav;
		fp->color = pcolor + RandomLong(0, 9);
#if defined( GLQUAKE )
		fp->packedColor = 0;
#else
		fp->packedColor = hlRGB(host_basepal, fp->color);
#endif

		VectorCopy(org, fp->org);
		VectorCopy(dir, dirCopy);
		dirCopy[2] -= arc;

		arc -= 0.005;
		VectorScale(dirCopy, lHigh, fp->vel);
		lHigh -= 0.00001;
	}

	arc = 0.075;
	fp = free_particles;
	for (count = 0; count < speed / 5 && free_particles != NULL; count++)
	{
		free_particles = fp->next;
		fp->next = active_particles;
		active_particles = fp;

		fp->die = cl.time + 3.0;
		fp->type = pt_vox_slowgrav;
		fp->color = pcolor + RandomLong(0, 9);

#if defined( GLQUAKE )
		fp->packedColor = 0;
#else
		fp->packedColor = hlRGB(host_basepal, fp->color);
#endif

		VectorCopy(org, fp->org);
		VectorCopy(dir, dirCopy);
		dirCopy[2] -= arc;

		arc -= 0.005;

		fRand = RandomFloat(0.0, 1.0);
		VectorScale(dirCopy, 1.7 * fRand, dirCopy);
		lHigh = (float)speed * fRand;
		VectorScale(dirCopy, lHigh, fp->vel);

		for (int i = 0; i < 2; i++)
		{
			if (free_particles == NULL)
				return;

			fp = free_particles;
			free_particles = free_particles->next;
			fp->next = active_particles;
			active_particles = fp;

			fp->die = cl.time + 3.0;
			fp->type = pt_vox_slowgrav;
			fp->color = pcolor + RandomLong(0, 9);
#if defined( GLQUAKE )
			fp->packedColor = 0;
#else
			fp->packedColor = hlRGB(host_basepal, fp->color);
#endif

			fp->org[0] = RandomFloat(-1.0, 1.0) + org[0];
			fp->org[1] = RandomFloat(-1.0, 1.0) + org[1];
			fp->org[2] = RandomFloat(-1.0, 1.0) + org[2];

			VectorCopy(dir, dirCopy);
			dirCopy[2] -= arc;

			VectorScale(dirCopy, 1.7 * fRand, dirCopy);
			VectorScale(dirCopy, lHigh, fp->vel);
		}
	}
}

void R_BulletImpactParticles( vec_t* pos )
{
	vec_t flHigh;
	vec3_t dir;
	float time, len;
	int idist, cycles, ramp;
	particle_t *fp;

	VectorSubtract(pos, r_origin, dir);

	len = Length(dir);
	if (len > 1000)
		len = 1000;

	idist = int((1000.f - len) / 100.f);
	if (!idist)
		idist = 1;

	cycles = 4 * idist;
	ramp = 3 - (30 * idist / 1000);

	R_SparkStreaks(pos, 2, -200, 200);

	for (int i = 0; i < cycles; i++)
	{
		if (free_particles == NULL)
			break;

		fp = free_particles;
		free_particles = free_particles->next;
		fp->next = active_particles;
		active_particles = fp;

		VectorCopy(pos, fp->org);

		dir[0] = RandomFloat(-1.0, 1.0);
		dir[1] = RandomFloat(-1.0, 1.0);
		dir[2] = RandomFloat(-1.0, 1.0);

		VectorCopy(dir, fp->vel);
		VectorScale(fp->vel, RandomFloat(50.0, 100.0), fp->vel);

		fp->type = pt_grav;
		fp->die = cl.time + 0.5;
		fp->color = 3 - ramp;
#if defined( GLQUAKE )
		fp->packedColor = 0;
#else
		fp->packedColor = hlRGB(host_basepal, fp->color);
#endif
	}
}

void R_EntityParticles( cl_entity_t* ent )
{
	int i;
	float sx, sy, sz, cx, cy, cz;
	particle_t *fp;

	if (avelocities[0][0] == 0.0)
	{
		for (i = 0; i < NUMVERTEXNORMALS * 3; i++)
			avelocities[0][i] = RandomFloat(0.0, 2.55);
	}

	for (i = 0; i < NUMVERTEXNORMALS; i++)
	{
		sx = sin(avelocities[i][0]);
		cx = cos(avelocities[i][0]);
		sy = sin(avelocities[i][1]);
		cy = cos(avelocities[i][1]);
		sz = sin(avelocities[i][2]);
		cz = cos(avelocities[i][2]);

		if (free_particles == NULL)
			return;

		fp = free_particles;
		free_particles = free_particles->next;
		fp->die = cl.time + 0.01;
		fp->next = active_particles;
		active_particles = fp;

		fp->color = 111;
#if defined( GLQUAKE )
		fp->packedColor = 0;
#else
		fp->packedColor = hlRGB(host_basepal, fp->color);
#endif
		fp->type = pt_explode;
		
		fp->org[0] = r_avertexnormals[i][0] * 64.0 + ent->origin[0] + cx * cy * beamlength;
		fp->org[1] = r_avertexnormals[i][1] * 64.0 + ent->origin[1] + sx * cy * beamlength;
		fp->org[2] = r_avertexnormals[i][2] * 64.0 + ent->origin[2] - sy * beamlength;
	}
}

void R_FlickerParticles( vec_t* org )
{
	particle_t *fp;

	for (int i = 0; i < 15; i++)
	{
		fp = free_particles;
		free_particles = free_particles->next;
		fp->next = active_particles;
		active_particles = fp;

		VectorCopy(org, fp->org);
		fp->vel[0] = RandomFloat(-32.0, 32.0);
		fp->vel[1] = RandomFloat(-32.0, 32.0);
		fp->vel[2] = RandomFloat(-32.0, 32.0);

		fp->vel[2] = RandomFloat(80.0, 143.0);
		fp->type = pt_blob2;
		fp->color = 254;
#if defined( GLQUAKE )
		fp->packedColor = 0;
#else
		fp->packedColor = hlRGB(host_basepal, fp->color);
#endif
		fp->die = cl.time + 2.0;
		fp->ramp = 0.0;
	}
}

void R_Implosion( vec_t* end, float radius, int count, float life )
{
	vec3_t temp, org;
	float r, scalar;
	int i;

	r = radius / 100;
	for (i = 0; i < count; i++)
	{
		temp[0] = RandomFloat(-100.0, 100.0) * r;
		temp[1] = RandomFloat(-100.0, 100.0) * r;
		temp[2] = RandomFloat(0.0, 100.0) * r;

		VectorAdd(temp, end, org);

		scalar = -1 / life;
		VectorScale(temp, scalar, temp);
		R_TracerParticles(org, temp, life);
	}
}

void R_LargeFunnel( vec_t* org, int reverse )
{
	particle_t *fp;
	float flDist, vel, fRand;
	vec3_t dir, temp;

	for (int j = -8; j < 8; j++)
		for (int i = -8; i < 8; i++)
		{
			fp = free_particles;
			free_particles = free_particles->next;
			fp->next = active_particles;
			active_particles = fp;

			temp[0] = float(32 * i) + org[0];
			temp[1] = float(32 * j) + org[1];
			temp[2] = RandomFloat(100.0, 800.0) + org[2];

			// реверс движка да
			if (reverse)
			{
				VectorCopy(org, fp->org);
				VectorSubtract(temp, fp->org, dir);
			}
			else
			{
				VectorCopy(temp, fp->org);
				VectorSubtract(org, fp->org, dir);
			}

			vel = temp[2] / 8.0f;

			fp->color = 244;
#if defined( GLQUAKE )
			fp->packedColor = 0;
#else
			fp->packedColor = hlRGB(host_basepal, fp->color);
#endif
			fp->type = pt_static;

			flDist = VectorNormalize(dir);

			vel = max(vel, 64.0f) + RandomFloat(64, 128);
			VectorScale(dir, vel, fp->vel);
			fp->die = flDist / vel + cl.time;
		}
}

void R_LavaSplash( vec_t* org )
{
	int			i, j, k;
	particle_t	*p;
	float		vel;
	vec3_t		dir;

	for (i=-16 ; i<16 ; i++)
		for (j=-16 ; j<16 ; j++)
			for (k=0 ; k<1 ; k++)
			{
				if (!free_particles)
					return;
				p = free_particles;
				free_particles = p->next;
				p->next = active_particles;
				active_particles = p;
		
				p->die = cl.time + RandomFloat(2.0, 2.62);
				p->color = RandomLong(224, 231);
#if defined( GLQUAKE )
				p->packedColor = 0;
#else
				p->packedColor = hlRGB(host_basepal, p->color);
#endif
				p->type = pt_slowgrav;
				
				dir[0] = j*8 + RandomFloat(0.0, 7.0);
				dir[1] = i*8 + RandomFloat(0.0, 7.0);
				dir[2] = 256;
	
				p->org[0] = org[0] + dir[0];
				p->org[1] = org[1] + dir[1];
				p->org[2] = org[2] + RandomFloat(0.0, 63.0);
	
				VectorNormalize (dir);						
				vel = RandomFloat(50.0, 113.0);
				VectorScale (dir, vel, p->vel);
			}

}

void R_ParticleBurst( vec_t* pos, int size, int color, float life )
{
	int			i, j, k;
	particle_t	*p;
	float		vel;
	vec3_t		dir, temp;

	for (i=0 ; i<32 ; i++)
		for (j=0 ; j<32 ; j++)
			for (k=0 ; k<1 ; k++)
				{
					if (!free_particles)
						return;

					p = free_particles;
					free_particles = p->next;
					p->next = active_particles;
					active_particles = p;

					p->color = color + RandomLong(0, 10);
#if defined( GLQUAKE )
					p->packedColor = 0;
#else
					p->packedColor = hlRGB(host_basepal, p->color);
#endif
					p->type = pt_static;
					VectorCopy(pos, p->org);

					temp[0] = RandomFloat(-size, size) + pos[0];
					temp[1] = RandomFloat(-size, size) + pos[1];
					temp[2] = RandomFloat(-size, size) + pos[2];

					VectorSubtract(p->org, temp, dir);

					p->die = life + cl.time + RandomFloat(-0.5, 0.5);
					VectorScale(dir, VectorNormalize(dir) / life, p->vel);
				}
}

void R_ParticleExplosion( vec_t* org )
{
	particle_t	*p;

	for (int i = 0; i < 1024; i++)
	{
		if (!free_particles)
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = cl.time + 5.0;
		p->color = ramp1[0];
#if defined( GLQUAKE )
		p->packedColor = 0;
#else
		p->packedColor = hlRGB(host_basepal, p->color);
#endif

		p->ramp = RandomLong(0, 3);
		p->type = (i & 1) ? pt_explode : pt_explode2;

		do
		{
			for (int j = 0; j < 3; j++)
				p->vel[j] = RandomFloat(-512.0, 512.0);
		} while (DotProduct(p->vel, p->vel) > 256 * 1024);

		p->org[0] = org[0] + p->vel[0] / 4;
		p->org[1] = org[1] + p->vel[1] / 4;
		p->org[2] = org[2] + p->vel[2] / 4;
	}
}

void R_ParticleExplosion2( vec_t* org, int colorStart, int colorLength )
{
	particle_t	*p;

	for (int i = 0; i < 512; i++)
	{
		if (!free_particles)
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = cl.time + 0.3;
		p->color = i % colorLength + colorStart;
#if defined( GLQUAKE )
		p->packedColor = 0;
#else
		p->packedColor = hlRGB(host_basepal, p->color);
#endif
		p->type = pt_blob;

		for (int j = 0; j<3; j++)
		{
			p->org[j] = org[j] + RandomFloat(-16.0, 16.0);
			p->vel[j] = RandomFloat(-256.0, 256.0);
		}
	}
}

void R_RocketTrail( vec_t* start, vec_t* end, int type )
{
	vec3_t	vec, right, up;
	float	len, flScale, flTrigAngle, flTrigCoeff, flSin, flCos;
	int			j;
	particle_t	*p;

	VectorSubtract(end, start, vec);
	len = VectorNormalize(vec);

	if (type == 7)
	{
		VectorMatrix(vec, right, up);
		flScale = 1;
	}
	else
	{
		flScale = 3;
		if (type > 127)
		{
			type -= 128;
			flScale = 1;
		}
	}

	VectorScale(vec, flScale, vec);

	while (len > 0)
	{
		len -= flScale;

		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		VectorCopy(vec3_origin, p->vel);
		p->die = cl.time + 2;

		switch (type)
		{
		case 0:
			// rocket trail
			p->ramp = RandomLong(0, 3);
			p->color = ramp3[(int)p->ramp];
			p->type = pt_fire;
			for (j = 0; j<3; j++)
				p->org[j] = start[j] + RandomFloat(-3.0, 3.0);
			break;
		case 1:
			// smoke smoke
			p->ramp = RandomLong(2, 5);
			p->color = ramp3[(int)p->ramp];
			p->type = pt_fire;
			for (j = 0; j<3; j++)
				p->org[j] = start[j] + RandomFloat(-3.0, 3.0);
			break;
		case 2:
			// blood
			p->type = pt_grav;
			p->color = RandomLong(67, 74);
			for (j = 0; j<3; j++)
				p->org[j] = start[j] + RandomFloat(-3.0, 3.0);
		case 3:
		case 5:
			// tracer
			static int tracercount;

			p->die = cl.time + 0.5;
			p->type = pt_static;
			if (type == 3)
				p->color = 52 + ((tracercount & 4) << 1);
			else
				p->color = 230 + ((tracercount & 4) << 1);

			tracercount++;

			VectorCopy(start, p->org);
			if (tracercount & 1)
			{
				p->vel[0] = 30 * vec[1];
				p->vel[1] = 30 * -vec[0];
			}
			else
			{
				p->vel[0] = 30 * -vec[1];
				p->vel[1] = 30 * vec[0];
			}
			break;
		case 4:
			// slight blood
			p->type = pt_grav;
			p->color = RandomLong(67, 70);
			for (j = 0; j<3; j++)
				p->org[j] = start[j] + RandomFloat(-3.0, 3.0);
			len -= 3;
		case 6:
			// voor trail
			p->ramp = RandomLong(0, 3);
			p->color = ramp3[(int)p->ramp];
			p->type = pt_fire;
			VectorCopy(start, p->org);
			break;
		case 7:
			flTrigAngle = (float)RandomLong(0, 0xFFFF);
			flTrigCoeff = (float)RandomLong(8, 16);

			flSin = sin(flTrigAngle) * flTrigCoeff;
			flCos = cos(flTrigAngle) * flTrigCoeff;

			p->org[0] = up[0] * flCos + right[0] * flSin + start[0];
			p->org[1] = up[1] * flCos + right[1] * flSin + start[1];
			p->org[2] = up[2] * flCos + right[2] * flSin + start[2];
		
			VectorSubtract(start, p->org, p->vel);
			VectorScale(p->vel, 2, p->vel);
			VectorMA(p->vel, RandomFloat(96.0, 111.0), vec, p->vel);

			p->die = cl.time + 2;
			p->type = pt_explode2;
			p->ramp = RandomLong(0, 3);
			p->color = ramp3[(int)p->ramp];
			break;
		default:
			break;
		}

#if defined( GLQUAKE )
		p->packedColor = 0;
#else
		p->packedColor = hlRGB(host_basepal, p->color);
#endif
		VectorAdd(start, vec, start);
	}

}

BEAM *R_BeamAlloc()
{
	BEAM *p;

	if (!gpFreeBeams)
		return nullptr;

	p = gpFreeBeams;
	gpFreeBeams = p->next;
	p->next = gpActiveBeams;
	gpActiveBeams = p;

	return p;
}

void R_BeamSetup(BEAM *pbeam, vec_t *start, vec_t *end, int modelIndex, float life, float width, float amplitude, float brightness, float speed)
{
	model_t* mod = CL_GetModelByIndex(modelIndex);

	if (mod == NULL)
		return;

	pbeam->type = TE_BEAMPOINTS;
	pbeam->modelIndex = modelIndex;
	pbeam->frame = 0.0;
	pbeam->frameRate = 0.0;
	pbeam->frameCount = ModelFrameCount(mod);
	VectorCopy(start, pbeam->source);
	VectorCopy(end, pbeam->target);
	VectorSubtract(end, start, pbeam->delta);
	pbeam->freq = speed * cl.time;
	pbeam->die = life + cl.time;
	pbeam->width = width;
	pbeam->amplitude = amplitude;
	pbeam->speed = speed;
	pbeam->brightness = brightness;

	if (amplitude >= 0.5)
		pbeam->segments = int(Length(pbeam->delta) * 0.25 + 3.0);
	else
		pbeam->segments = int(Length(pbeam->delta) * 0.075 + 3.0);

	pbeam->flags = 0;
	pbeam->pFollowModel = NULL;
}

cl_entity_t* R_GetBeamAttachmentEntity(int index)
{
	if (index < 0)
		return ClientDLL_GetUserEntity(BEAMENT_ENTITY(-index));
	else
	{
	//	Con_SafePrintf(__FUNCTION__ " accesing cl_entities [ %d ] as model %s\r\n", index, cl_entities[index].model->name);
		return &cl_entities[BEAMENT_ENTITY(index)];
	}
}

void R_RunParticleEffect( vec_t* org, vec_t* dir, int color, int count )
{
	int			i, j;
	particle_t	*p;

	for (i = 0; i<count; i++)
	{
		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		if (count == 1024)
		{
			p->die = cl.time + 5;
			p->color = ramp1[0];
#if defined( GLQUAKE )
			p->packedColor = 0;
#else
			p->packedColor = hlRGB(host_basepal, p->color);
#endif
			p->type = (i & 1) ? pt_explode : pt_explode2;
			for (int j = 0; j<3; j++)
			{
				p->org[j] = org[j] + RandomFloat(-16.0, 16.0);
				p->vel[j] = RandomFloat(-256.0, 256.0);
			}
		}
		else
		{
			p->die = cl.time + RandomFloat(0.0, 0.4);
			p->color = (color&~7) + RandomLong(0, 7);
#if defined( GLQUAKE )
			p->packedColor = 0;
#else
			p->packedColor = hlRGB(host_basepal, p->color);
#endif
			p->type = pt_slowgrav;
			for (j = 0; j < 3; j++)
			{
				p->org[j] = org[j] + RandomFloat(-8.0, 8.0);
				p->vel[j] = dir[j] * 15;// + (rand()%300)-150;
			}
		}
	}
}

void R_ShowLine( vec_t* start, vec_t* end )
{
	int			i;
	particle_t	*p;
	vec3_t vec;
	float len;

	VectorSubtract(end, start, vec);
	len = VectorNormalize(vec);
	VectorScale(vec, 5, vec);

	while (len > 0)
	{
		len -= 5;

		if (!free_particles)
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		VectorCopy(vec3_origin, p->vel);
		p->die = cl.time + 30;
		p->color = 75;
#if defined( GLQUAKE )
		p->packedColor = 0;
#else
		p->packedColor = hlRGB(host_basepal, p->color);
#endif
		p->type = pt_static;
		VectorCopy(start, p->org);
		VectorAdd(start, vec, start);
	}
}

void R_SparkStreaks( vec_t* pos, int count, int velocityMin, int velocityMax )
{
	particle_t *fp;

	for (int i = 0; i < count && free_particles != NULL; i++)
	{
		fp = free_particles;
		free_particles = fp->next;
		fp->next = gpActiveTracers;
		gpActiveTracers = fp;

		fp->die = RandomFloat(0.1, 0.5) + cl.time;

		fp->color = 5;
		fp->packedColor = 255;
		fp->type = pt_grav;
		fp->ramp = 0.5;
		VectorCopy(pos, fp->org);
		fp->vel[0] = RandomFloat(velocityMin, velocityMax);
		fp->vel[1] = RandomFloat(velocityMin, velocityMax);
		fp->vel[2] = RandomFloat(velocityMin, velocityMax);
	}
}

void R_StreakSplash( vec_t* pos, vec_t* dir, int color, int count,
					 float speed, int velocityMin, int velocityMax )
{
	vec3_t initialVelocity;
	particle_t *p;
	int i, j;
	VectorScale(dir, speed, initialVelocity);
	for (i = 0; i < count; i++)
	{
		if (!free_particles)
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = gpActiveTracers;
		gpActiveTracers = p;

		p->die = RandomFloat(0.1, 0.5) + cl.time;
		p->color = color;
		p->packedColor = 255;

		p->type = pt_grav;
		p->ramp = 1;

		for (j = 0; j < 3; j++)
		{
			p->org[j] = pos[j];
			p->vel[j] = initialVelocity[j] + RandomFloat(velocityMin, velocityMax);
		}
	}
}

void R_UserTracerParticle( vec_t* org, vec_t* vel,
						   float life, int colorIndex, float length,
						   byte context, void( *deathfunc )( particle_t* ) )
{
	if (colorIndex < 0)
	{
		Con_Printf(const_cast<char*>("UserTracer with color < 0\n"));
	}
	else if (colorIndex > 12)
	{
		Con_Printf(const_cast<char*>("UserTracer with color > %d\n"), 12);
	}
	else
	{
		particle_t *p;
		int j;

		if (!free_particles)
			return;

		p = free_particles;
		free_particles = p->next;
		p->next = gpActiveTracers;
		gpActiveTracers = p;

		p->die = cl.time + life;
		p->color = colorIndex;
		p->packedColor = 255;
		p->type = pt_static;
		p->ramp = length;
		for (j = 0; j < 3; j++)
		{
			p->org[j] = org[j];
			p->vel[j] = vel[j];
		}
		p->deathfunc = deathfunc;
		p->context = context;
	}
}

particle_t* R_TracerParticles(vec_t* org, vec_t* vel, float life)
{
	particle_t* part;

	if (free_particles == NULL)
		return nullptr;

	part = free_particles;
	free_particles = free_particles->next;

	// добавление эл-та в односвязный список
	part->next = gpActiveTracers;
	gpActiveTracers = part;

	part->die = life + cl.time;
	part->color = 4;
	part->packedColor = 255;
	part->type = pt_static;
	part->ramp = tracerLength.value;
	
	VectorCopy(org, part->org);
	VectorCopy(vel, part->vel);

	return part;
}

void R_TeleportSplash( vec_t* org )
{
	int			i, j, k;
	particle_t	*p;
	float		vel;
	vec3_t		dir;

	for (i=-16 ; i<16 ; i+=4)
		for (j=-16 ; j<16 ; j+=4)
			for (k=-24 ; k<32 ; k+=4)
			{
				if (!free_particles)
					return;
				p = free_particles;
				free_particles = p->next;
				p->next = active_particles;
				active_particles = p;
		
				p->die = cl.time + RandomFloat(0.2, 0.34);
				p->color = RandomLong(7, 14);
#if defined( GLQUAKE )
				p->packedColor = 0;
#else
				p->packedColor = hlRGB(host_basepal, p->color);
#endif
				p->type = pt_slowgrav;
				
				dir[0] = j*8;
				dir[1] = i*8;
				dir[2] = k*8;
	
				p->org[0] = org[0] + i + RandomFloat(0.0, 3.0);
				p->org[1] = org[1] + j + RandomFloat(0.0, 3.0);
				p->org[2] = org[2] + k + RandomFloat(0.0, 3.0);
	
				VectorNormalize (dir);						
				vel = RandomFloat(50.0, 113.0);
				VectorScale (dir, vel, p->vel);
			}

}

BEAM* R_BeamCirclePoints( int type, vec_t* start, vec_t* end,
						  int modelIndex, float life, float width, float amplitude,
						  float brightness, float speed, int startFrame, float framerate,
						  float r, float g, float b )
{
	BEAM *p = R_BeamLightning(start, end, modelIndex, life, width, amplitude, brightness, speed);

	if (p == NULL)
		return NULL;

	p->type = type;

	if (life == 0)
		p->flags |= FBEAM_FOREVER;

	SetBeamAttributes(p, r, g, b, framerate, startFrame);

	return p;
}

BEAM* R_BeamEntPoint( int startEnt, vec_t* end,
					  int modelIndex, float life, float width, float amplitude,
					  float brightness, float speed, int startFrame, float framerate,
					  float r, float g, float b )
{
	BEAM *p;
	cl_entity_t *ent = R_GetBeamAttachmentEntity(startEnt);
	if (ent == NULL)
		return nullptr;

	if (ent->model == NULL && life != 0.0)
		return NULL;

	p = R_BeamLightning(vec3_origin, end, modelIndex, life, width, amplitude, brightness, speed);

	if (p == NULL)
		return NULL;

	p->type = TE_BEAMPOINTS;
	p->flags = FBEAM_STARTENTITY;

	if (life == 0)
		p->flags |= FBEAM_FOREVER | FBEAM_STARTENTITY;

	p->startEntity = startEnt;
	p->endEntity = 0;

	SetBeamAttributes(p, r, g, b, framerate, startFrame);

	return p;
}

BEAM* R_BeamEnts( int startEnt, int endEnt,
				  int modelIndex, float life, float width, float amplitude,
				  float brightness, float speed, int startFrame, float framerate,
				  float r, float g, float b )
{
	BEAM *pbeam;
	cl_entity_t *ent = R_GetBeamAttachmentEntity(startEnt), *ent2 = R_GetBeamAttachmentEntity(endEnt);
	if (ent == NULL || ent2 == NULL)
		return nullptr;

	if ((ent->model == NULL || ent2->model == NULL) && life != 0.0)
		return NULL;

	pbeam = R_BeamLightning(vec3_origin, vec3_origin, modelIndex, life, width, amplitude, brightness, speed);

	if (pbeam == NULL)
		return NULL;

	pbeam->type = TE_BEAMPOINTS;
	pbeam->flags = FBEAM_STARTENTITY | FBEAM_ENDENTITY;

	if (life == 0)
		pbeam->flags |= FBEAM_FOREVER | FBEAM_STARTENTITY | FBEAM_ENDENTITY;

	pbeam->startEntity = startEnt;
	pbeam->endEntity = endEnt;

	SetBeamAttributes(pbeam, r, g, b, framerate, startFrame);

	return pbeam;
}

BEAM* R_BeamFollow( int startEnt,
					int modelIndex, float life, float width,
					float r, float g, float b,
					float brightness )
{
	BEAM *pbeam = R_BeamLightning(vec3_origin, vec3_origin, modelIndex, life, width, life, brightness, 1.0);

	if (pbeam == NULL)
		return NULL;

	pbeam->startEntity = startEnt;
	pbeam->type = TE_BEAMFOLLOW;
	pbeam->flags = FBEAM_STARTENTITY;

	SetBeamAttributes(pbeam, r, g, b, 1.0, 0);

	return pbeam;
}

void R_BeamKill( int deadEntity )
{
	BEAM *pbeam;

	for (pbeam = gpActiveBeams; pbeam != NULL; pbeam = pbeam->next)
	{
		if (pbeam->flags & FBEAM_STARTENTITY && pbeam->startEntity == deadEntity)
		{
			pbeam->flags &= ~(FBEAM_FOREVER | FBEAM_STARTENTITY);
			if (pbeam->type != TE_BEAMFOLLOW)
				pbeam->die = cl.time;
		}

		if (pbeam->flags & FBEAM_ENDENTITY && pbeam->endEntity == deadEntity)
		{
			pbeam->flags &= ~(FBEAM_FOREVER | FBEAM_ENDENTITY);
			pbeam->die = cl.time;
		}
	}
}

BEAM* R_BeamLightning( vec_t* start, vec_t* end,
					   int modelIndex, float life, float width, float amplitude,
					   float brightness, float speed )
{
	BEAM *p = R_BeamAlloc();

	if (p == NULL)
		return p;

	p->die = cl.time;

	if (modelIndex < 0)
		return NULL;

	R_BeamSetup(p, start, end, modelIndex, life, width, amplitude, brightness, speed);

	return p;
}

BEAM* R_BeamPoints( vec_t* start, vec_t* end,
					int modelIndex, float life, float width, float amplitude,
					float brightness, float speed, int startFrame, float framerate,
					float r, float g, float b )
{
	BEAM *pbeam;

	if (life != 0.0 && R_BeamCull(start, end, TRUE) == FALSE)
		return nullptr;

	pbeam = R_BeamLightning(start, end, modelIndex, life, width, amplitude, brightness, speed);

	if (pbeam == NULL)
		return NULL;

	if (life == 0)
		pbeam->flags |= FBEAM_FOREVER;

	SetBeamAttributes(pbeam, r, g, b, framerate, startFrame);

	return pbeam;
}

BEAM* R_BeamRing( int startEnt, int endEnt,
				  int modelIndex, float life, float width, float amplitude,
				  float brightness, float speed, int startFrame, float framerate,
				  float r, float g, float b )
{
	BEAM *pbeam;
	cl_entity_t *ent = R_GetBeamAttachmentEntity(startEnt), *ent2 = R_GetBeamAttachmentEntity(endEnt);
	if (ent == NULL || ent2 == NULL)
		return nullptr;

	if ((ent->model == NULL || ent2->model == NULL) && life != 0.0)
		return NULL;

	pbeam = R_BeamLightning(vec3_origin, vec3_origin, modelIndex, life, width, amplitude, brightness, speed);

	if (pbeam == NULL)
		return NULL;

	pbeam->type = TE_BEAMRING;

	if (life == 0)
		pbeam->flags |= FBEAM_FOREVER;

	pbeam->startEntity = startEnt;
	pbeam->endEntity = endEnt;

	SetBeamAttributes(pbeam, r, g, b, framerate, startFrame);

	return pbeam;
}

void SetBeamAttributes(BEAM *pbeam, float r, float g, float b, float framerate, int startFrame)
{
	pbeam->frameRate = framerate;
	pbeam->r = r;
	pbeam->frame = startFrame;
	pbeam->g = g;
	pbeam->b = b;
}

void SineNoise(float *noise, int divs)
{
	float step = M_PI / (float)divs;

	for (int i = 0; i < divs; i++)
		noise[i] = sin(step * (float)i);
}

// SH_CODE: review. two recursive calls, no cycle
void Noise(float *noise, int divs)
{	
	for (int i = divs >> 1; i > 1; i >>= 1)
	{
		noise[i] = RandomFloat(-0.125f, 0.125f) * (float)divs + (noise[divs] + noise[0]) * 0.5f;
		Noise(&noise[i], i);
	}
}

float* R_BeamGetAttachmentPoint(cl_entity_t *ent, int attachment)
{
	int ptidx;

	if (attachment < 0)
		attachment = -attachment;

	ptidx = BEAMENT_ATTACHMENT(attachment);

	if (ptidx != 0)
		return ent->attachment[ptidx - 1];

	if (ent->index != cl.playernum + 1)
		return ent->origin;

	return cl.simorg;
}

int R_BeamCull(vec_t *start, vec_t *end, int pvsOnly)
{
	vec3_t mins, maxs;
	int i;

	if (!cl.worldmodel)
		return 0;

	for (i = 0; i < 3; i++)
	{
		if (start[i] < end[i])
		{
			mins[i] = start[i];
			maxs[i] = end[i];
		}
		else
		{
			mins[i] = end[i];
			maxs[i] = start[i];
		}

		// Don't let it be zero sized
		if (mins[i] == maxs[i])
		{
			maxs[i] += 1;
		}
	}

	// Check bbox
	if (PVSNode(cl.worldmodel->nodes, mins, maxs))
	{
		if (pvsOnly || !R_CullBox(mins, maxs))
		{
			// Beam is visible
			return 1;
		}
	}

	// Beam is not visible
	return 0;
}

void R_DrawSegs(vec_t *source, vec_t *delta, float width, float scale, float freq, float speed, int segments, int flags)
{
	float length, div, vStep, vLast, brightness, fraction, v;
	vec3_t screenLast, point, screen, tmp, normal, last1, last2;
	int noiseidx, seg;

	if (segments <= 1)
		return;

	if (segments > NOISE_DIVS)
		segments = NOISE_DIVS;

	length = Length(delta) * .01f;

	if (length < 0.5)
		length = 0.5;

	div = 1.0f / float(segments - 1);
	vStep = length * div;

	vLast = fmod(freq * speed, 1.0f);

	if (flags & FBEAM_SINENOISE)
	{
		if (segments <= 15)
			segments = 16;

		length = (float)segments / 10.f;
		scale *= 100;
	}
	else
		scale *= length;

	ScreenTransform(source, screenLast);
	VectorMA(source, div, delta, point);
	ScreenTransform(point, screen);
	VectorSubtract(screen, screenLast, tmp);
	tmp[2] = 0;
	VectorNormalize(tmp);
	VectorScale(vup, tmp[0], normal);
	VectorMA(normal, -tmp[1], vright, normal);
	VectorMA(source, width, normal, last1);
	VectorMA(source, -width, normal, last2);

	brightness = 1.f;

	if (flags & FBEAM_SINENOISE)
		noiseidx = int(div * 128.f * 0x10000);
	else
		noiseidx = 0;

	if (flags & FBEAM_SHADEIN)
		brightness = .0f;

	for (seg = 1; seg < segments; seg++)
	{
		fraction = seg * div;
		tri.Brightness(brightness);
		tri.TexCoord2f(0.0, vLast);
		tri.Vertex3fv(last1);
		tri.Brightness(brightness);
		tri.TexCoord2f(1.0, vLast);
		tri.Vertex3fv(last2);

		if (flags & FBEAM_SHADEIN)
			brightness = fraction;
		else if (flags & FBEAM_SHADEOUT)
			brightness = 1.f - fraction;

		VectorMA(source, fraction, delta, point);

		if (scale != .0f)
		{
			if (flags & FBEAM_SINENOISE)
			{
				v = M_PI * fraction * length + freq;
				VectorMA(point, sin(v) * scale * gNoise[noiseidx >> 16], vup, point);
				VectorMA(point, cos(v) * scale * gNoise[noiseidx >> 16], vright, point);
			}
			else
			{
				VectorMA(point, scale * gNoise[noiseidx >> 16], vup, point);
				VectorMA(point, cos(M_PI * fraction * 3.f * freq) * scale * gNoise[noiseidx >> 16], vright, point);
			}
		}

		ScreenTransform(point, screen);
		VectorSubtract(screen, screenLast, tmp);
		tmp[2] = 0;
		VectorNormalize(tmp);
		VectorScale(vup, tmp[0], normal);
		VectorMA(normal, -tmp[1], vright, normal);
		VectorMA(point, width, normal, last1);
		VectorMA(point, -width, normal, last2);

		vLast += vStep;

		tri.Brightness(brightness);
		tri.TexCoord2f(1.0, vLast);
		tri.Vertex3fv(last2);
		tri.Brightness(brightness);
		tri.TexCoord2f(0.0, vLast);
		tri.Vertex3fv(last1);

		VectorCopy(screen, screenLast);

		vLast = fmod(vLast, 1.f);
		noiseidx += int(div * 128.f * 0x10000);
	}
}

void R_DrawTorus(vec_t *source, vec_t *delta, float width, float scale, float freq, float speed, int segments)
{
	float length, div, vStep, vLast, brightness, worldPnt, v, dx, dy;
	vec3_t screenLast, point, screen, tmp, normal, last1, last2;
	int noiseidx, seg;

	if (segments <= 1)
		return;

	if (segments > NOISE_DIVS)
		segments = NOISE_DIVS;

	length = Length(delta) * .01f;

	if (length < 0.5)
		length = 0.5;

	div = 1.0f / float(segments - 1);
	vStep = length * div;

	vLast = fmod(freq * speed, 1.0f);

	noiseidx = 0;
	scale *= length;

	for (seg = 0; seg <= segments; seg++)
	{
		worldPnt = seg * div;

		point[0] = sin(2 * M_PI * worldPnt) * freq * delta[2] + source[0];
		point[1] = cos(2 * M_PI * worldPnt) * freq * delta[2] + source[1];
		point[2] = source[2];

		VectorMA(point, scale * gNoise[noiseidx >> 16], vup, point);
		VectorMA(point, cos(3 * M_PI * worldPnt + freq) * scale * gNoise[noiseidx >> 16], vup, point);

		ScreenTransform(point, screenLast);

		if (seg)
		{
			tmp[2] = 0.0;
			tmp[0] = screenLast[0] - dx;
			tmp[1] = screenLast[1] - dy;

			VectorNormalize(tmp);
			VectorScale(vup, tmp[0], normal);
			VectorMA(normal, -tmp[1], vright, normal);
			VectorMA(point, width, normal, last1);
			VectorMA(point, -width, normal, last2);
			vLast = vLast + vStep;
			tri.TexCoord2f(1.0, vLast);
			tri.Vertex3fv(last2);
			tri.TexCoord2f(0.0, vLast);
			tri.Vertex3fv(last1);
		}

		dx = screenLast[0];
		dy = screenLast[1];

		vLast = fmod(vLast, 1.f);
		noiseidx += int(div * 128.f * 0x10000);
	}
}

void R_DrawDisk(vec_t *source, vec_t *delta, float width, float scale, float freq, float speed, int segments)
{
	float length, div, vStep, vLast, brightness, worldPnt, v;
	vec3_t screenLast, point, screen, tmp, normal, last1, last2;
	int seg;

	if (segments <= 1)
		return;

	if (segments > NOISE_DIVS)
		segments = NOISE_DIVS;

	length = Length(delta) * .01f;

	if (length < 0.5)
		length = 0.5;

	div = 1.0f / float(segments - 1);
	vStep = length * div;

	vLast = fmod(freq * speed, 1.0f);

	for (seg = 0; seg < segments; seg++)
	{
		VectorCopy(source, point);
		worldPnt = seg * div;

		tri.Brightness(1.0);
		tri.TexCoord2f(1.0, vLast);
		tri.Vertex3fv(point);

		point[0] = sin(2 * M_PI * worldPnt) * freq * delta[2] + source[0];
		point[1] = cos(2 * M_PI * worldPnt) * freq * delta[2] + source[1];
		point[2] = source[2];

		tri.Brightness(1.0);
		tri.TexCoord2f(0.0, vLast);
		tri.Vertex3fv(point);

		vLast = fmod(vLast, 1.f);
	}
}

void R_DrawCylinder(vec_t *source, vec_t *delta, float width, float scale, float freq, float speed, int segments)
{
	float length, div, vStep, vLast, brightness, worldPnt, v;
	vec3_t screenLast, point, screen, tmp, normal, last1, last2;
	int seg;

	if (segments <= 1)
		return;

	if (segments > NOISE_DIVS)
		segments = NOISE_DIVS;

	length = Length(delta) * .01f;

	if (length < 0.5)
		length = 0.5;

	div = 1.0f / float(segments - 1);
	vStep = length * div;

	vLast = fmod(freq * speed, 1.0f);

	for (seg = 0; seg < segments; seg++)
	{
		worldPnt = seg * div;
		v = 2 * M_PI * worldPnt;

		point[0] = sin(v) * freq * delta[2] + source[0];
		point[1] = cos(v) * freq * delta[2] + source[1];
		point[2] = source[2] + width;

		tri.Brightness(0.0);
		tri.TexCoord2f(1.0, vLast);
		tri.Vertex3fv(point);

		point[0] = sin(v) * freq * (width + delta[2]) + source[0];
		point[1] = cos(v) * freq * (width + delta[2]) + source[1];
		point[2] = source[2] - width;

		tri.Brightness(1.0);
		tri.TexCoord2f(0.0, vLast);
		tri.Vertex3fv(point);

		vLast = fmod(vLast, 1.f);
	}
}

void R_DrawBeamFollow(BEAM *pbeam)
{
	float length = 0.0, div, vStep = 1.0, vLast, brightness, worldPnt, v, fraction;
	vec3_t screenLast, point, screen, tmp, normal, last1, last2, delta;
	int noiseidx, seg;
	particle_t *fp = NULL, *pHead;

	R_FreeDeadParticles(&pbeam->particles);

	pHead = pbeam->particles;

	if (pbeam->flags & FBEAM_STARTENTITY)
	{
		if (pHead != NULL)
		{
			VectorSubtract(pHead->org, pbeam->source, delta);
			length = Length(delta);
		}
		else
			length = 0;

		if ((length >= 32 || pHead == NULL) && free_particles)
		{
			fp = free_particles;
			free_particles = free_particles->next;

			VectorCopy(pbeam->source, fp->org);
			fp->die = pbeam->amplitude + cl.time;
			fp->vel[0] = fp->vel[1] = fp->vel[2] = 0;
			pbeam->die = fp->die;
			fp->next = pHead;
			pbeam->particles = fp;
		}
	}

	if (pHead == NULL)
		return;

	if (fp != NULL || length == 0.0)
	{
		if (pHead->next == NULL)
			return;

		VectorCopy(pHead->org, delta);
		ScreenTransform(pHead->org, screenLast);
		ScreenTransform(pHead->next->org, screen);
		pHead = pHead->next;
	}
	else
	{
		VectorCopy(pbeam->source, delta);
		ScreenTransform(pbeam->source, screenLast);
		ScreenTransform(pHead->org, screen);
	}

	// Build world-space normal to screen-space direction vector
	VectorSubtract(screen, screenLast, tmp);
	// We don't need Z, we're in screen space
	tmp[2] = 0;

	VectorNormalize(tmp);
	VectorScale(vup, tmp[0], normal);	// Build point along noraml line (normal is -y, x)
	VectorMA(normal, -tmp[1], vright, normal);

	// Make a wide line
	VectorMA(delta, pbeam->width, normal, last1);
	VectorMA(delta, -pbeam->width, normal, last2);

	if (pHead != NULL)
	{
		vLast = 0.0;
		fraction = (pbeam->die - cl.time) / pbeam->amplitude;
		div = 1.0f / pbeam->amplitude;

		while (pHead)
		{
			tri.Brightness(fraction);
			tri.TexCoord2f(0.0, 0.0);
			tri.Vertex3fv(last1);
			tri.Brightness(fraction);
			tri.TexCoord2f(1.0, 0.0);
			tri.Vertex3fv(last2);
			
			// Transform point into screen space
			ScreenTransform(pHead->org, screen);
			// Build world-space normal to screen-space direction vector
			VectorSubtract(screen, screenLast, tmp);
			// We don't need Z, we're in screen space
			tmp[2] = 0;
			VectorNormalize(tmp);
			VectorScale(vup, tmp[0], normal);	// Build point along noraml line (normal is -y, x)
			VectorMA(normal, -tmp[1], vright, normal);

			// Make a wide line
			VectorMA(pHead->org, pbeam->width, normal, last1);
			VectorMA(pHead->org, -pbeam->width, normal, last2);

			vLast += vStep;	// Advance texture scroll (v axis only)

			if (pHead->next != NULL)
				fraction = (pHead->die - cl.time) * div;
			else
				fraction = 0.0f;

			tri.Brightness(fraction);
			tri.TexCoord2f(1.0, 1.0);
			tri.Vertex3fv(last2);
			tri.Brightness(fraction);
			tri.TexCoord2f(0.0, 1.0);
			tri.Vertex3fv(last1);

			VectorCopy(screen, screenLast);
			
			vLast = fmod(vLast, 1.0f);
			
			pHead = pHead->next;
		}
	}
	
	pHead = pbeam->particles;
	for (fraction = cl.time - cl.oldtime; pHead != NULL; pHead = pHead->next)
		VectorMA(pHead->org, fraction, pHead->vel, pHead->org);

}

void R_DrawRing(vec_t *source, vec_t *delta, float width, float amplitude, float freq, float speed, int segments)
{
	int				i, j, noiseIndex, noiseStep;
	float			div, length, fraction, factor, vLast, vStep;
	vec3_t			last1, last2, point, screen, screenLast{}, tmp, normal;
	vec3_t			center, xaxis, yaxis, zaxis;
	float			radius, x, y, scale;
	vec3_t			d;

	VectorCopy(delta, d);

	if (segments < 2)
		return;

	segments = segments * M_PI;

	if (segments > NOISE_DIVS * 8)		// UNDONE: Allow more segments?
		segments = NOISE_DIVS * 8;

	length = Length(d) * 0.01 * M_PI;
	if (length < 0.5)	// Don't lose all of the noise/texture on short beams
		length = 0.5;
	div = 1.0 / (segments - 1);

	// UNDONE: Expose texture length scale factor to control "fuzziness"
	vStep = length*div / 8.0;	// Texture length texels per space pixel

	// UNDONE: Expose this paramter as well(3.5)?  Texture scroll rate along beam
	vLast = fmod(freq*speed, 1);	// Scroll speed 3.5 -- initial texture position, scrolls 3.5/sec (1.0 is entire texture)
	scale = amplitude * length / 8.0;

	// Iterator to resample noise waveform (it needs to be generated in powers of 2)
	noiseStep = (int)(NOISE_DIVS * div * 65536.0) * 8;
	noiseIndex = 0;

	VectorScale(d, 0.5, d);

	VectorAdd(source, d, center);
	zaxis[0] = 0; zaxis[1] = 0; zaxis[2] = 1;

	VectorCopy(d, xaxis);
	radius = Length(xaxis);

	// cull beamring
	// --------------------------------
	// Compute box center +/- radius
	last1[0] = radius;
	last1[1] = radius;
	last1[2] = scale;
	VectorAdd(center, last1, tmp);	// maxs
	VectorSubtract(center, last1, screen); // mins

	if (cl.worldmodel == NULL)
		return;

	// Is that box in PVS && frustum?
	if (!PVSNode(cl.worldmodel->nodes, screenLast, tmp) && R_CullBox(screenLast, tmp))
	{
		return;
	}

	yaxis[0] = xaxis[1]; yaxis[1] = -xaxis[0]; yaxis[2] = 0;
	VectorNormalize(yaxis);
	VectorScale(yaxis, radius, yaxis);

	j = segments / 8;

	for (i = 0; i < segments + 1; i++)
	{
		fraction = i * div;

		x = sin(fraction * 2 * M_PI);
		y = cos(fraction * 2 * M_PI);

		point[0] = xaxis[0] * x + yaxis[0] * y + center[0];
		point[1] = xaxis[1] * x + yaxis[1] * y + center[1];
		point[2] = xaxis[2] * x + yaxis[2] * y + center[2];

		// Distort using noise
		factor = gNoise[(noiseIndex >> 16) & (NOISE_DIVS - 1)] * scale;
		VectorMA(point, factor, vup, point);

		// Rotate the noise along the perpendicluar axis a bit to keep the bolt from looking diagonal
		factor = gNoise[(noiseIndex >> 16) & (NOISE_DIVS - 1)] * scale * cos(fraction * M_PI * 3 * 8 + freq);
		VectorMA(point, factor, vright, point);

		// Transform point into screen space
		ScreenTransform(point, screen);

		if (i != 0)
		{
			// Build world-space normal to screen-space direction vector
			VectorSubtract(screen, screenLast, tmp);
			// We don't need Z, we're in screen space
			tmp[2] = 0;
			VectorNormalize(tmp);
			VectorScale(vup, tmp[0], normal);	// Build point along noraml line (normal is -y, x)
			VectorMA(normal, -tmp[1], vright, normal);

			// Make a wide line
			VectorMA(point, width, normal, last1);
			VectorMA(point, -width, normal, last2);

			vLast += vStep;	// Advance texture scroll (v axis only)

			tri.TexCoord2f(1.0, vLast);
			tri.Vertex3fv(last2);
			tri.TexCoord2f(0.0, vLast);
			tri.Vertex3fv(last1);
		}

		VectorCopy(screen, screenLast);
		noiseIndex += noiseStep;

		j--;

		if (j == 0 && amplitude != 0)
		{
			j = segments / 8;
			Noise(gNoise, NOISE_DIVS);
		}
	}
}

void R_BeamDraw(BEAM* pbeam, float frametime)
{
	model_t* mod;
	cl_entity_t* ent;
	float* pt, len;
	vec3_t difference, org, speed;
	particle_t* ppart;

	if (pbeam->modelIndex < 0)
	{
		pbeam->die = cl.time;
		return;
	}

	mod = CL_GetModelByIndex(pbeam->modelIndex);

	if (mod == NULL)
		return;

	if ((pbeam->flags & FBEAM_SOLID) != 0)
		tri.RenderMode(kRenderNormal);
	else
		tri.RenderMode(kRenderTransAdd);

	pbeam->freq += frametime;

	gNoise[0] = gNoise[NOISE_DIVS] = 0;

	if (pbeam->amplitude != 0.0)
	{
		if (pbeam->flags & FBEAM_SINENOISE)
			SineNoise(gNoise, NOISE_DIVS);
		else
			Noise(gNoise, NOISE_DIVS);
	}


	if (pbeam->flags & FBEAM_STARTENTITY | FBEAM_ENDENTITY)
	{
		if (pbeam->flags & FBEAM_STARTENTITY)
		{
			ent = R_GetBeamAttachmentEntity(pbeam->startEntity);
			if (ent == NULL)
				return;

			if (ent->model && (pbeam->pFollowModel == NULL || ent->model == pbeam->pFollowModel))
			{
				pt = R_BeamGetAttachmentPoint(ent, pbeam->startEntity);
				VectorCopy(pt, pbeam->source);
				pbeam->flags |= FBEAM_STARTVISIBLE;
				if (pbeam->pFollowModel == NULL)
					pbeam->pFollowModel = ent->model;
			}
			else
			{
				if (pbeam->flags >= 0)
					pbeam->flags &= ~FBEAM_STARTENTITY;
			}
		}

		if (pbeam->flags & FBEAM_ENDENTITY)
		{
			ent = R_GetBeamAttachmentEntity(pbeam->endEntity);
			if (ent == NULL)
				return;

			if (ent->model == NULL)
			{
				if (pbeam->flags >= 0)
				{
					pbeam->die = cl.time;
					pbeam->flags &= ~FBEAM_ENDENTITY;
				}
				return;
			}

			pt = R_BeamGetAttachmentPoint(ent, pbeam->endEntity);
			VectorCopy(pt, pbeam->target);
			pbeam->flags |= FBEAM_ENDVISIBLE;
		}

		if ((pbeam->flags & FBEAM_STARTENTITY) && !(pbeam->flags & FBEAM_STARTVISIBLE))
			return;

		VectorSubtract(pbeam->target, pbeam->source, difference);

		if (Length(difference) > 0.0000001)
			VectorCopy(difference, pbeam->delta);

		if (pbeam->amplitude < 0.5)
			pbeam->segments = int(Length(pbeam->delta) * 0.075 + 3.0);
		else
			pbeam->segments = int(Length(pbeam->delta) * 0.25 + 3.0);
	}

	if (pbeam->type != TE_EXPLFLAG_NONE || R_BeamCull(pbeam->source, pbeam->target, 0))
	{
		if (tri.SpriteTexture(mod, int(pbeam->frameRate * cl.time + pbeam->frame) % pbeam->frameCount))
		{
			if (pbeam->flags & FBEAM_SINENOISE && egon_amplitude.value > 0.0)
			{
				VectorMA(pbeam->target, sin(pbeam->freq * 10.0f) * (egon_amplitude.value * pbeam->amplitude), vup, org);
				VectorMA(org, cos(pbeam->freq * 10.0f) * (egon_amplitude.value * pbeam->amplitude), vright, org);

				VectorSubtract(pbeam->source, org, speed);

				len = Length(speed);

				if (len != 0.0f)
					VectorScale(speed, 1000.0f / len, speed);

				ppart = R_TracerParticles(org, speed, len * 0.001f);
				if (ppart != NULL)
					ppart->color = 7;
			}

			pbeam->t = pbeam->die - cl.time + pbeam->freq;
			if (pbeam->t != .0f)
				pbeam->t = 1.f - pbeam->freq / pbeam->t;

			if (pbeam->flags & FBEAM_FADEIN)
				tri.Color4f(pbeam->r, pbeam->g, pbeam->b, pbeam->brightness * pbeam->t);
			else if (pbeam->flags & FBEAM_FADEOUT)
				tri.Color4f(pbeam->r, pbeam->g, pbeam->b, pbeam->brightness * (1.f - pbeam->t));
			else
				tri.Color4f(pbeam->r, pbeam->g, pbeam->b, pbeam->brightness);

			switch (pbeam->type)
			{
			case TE_EXPLFLAG_NONE:
				tri.Begin(TRI_QUADS);
				R_DrawSegs(pbeam->source, pbeam->delta, pbeam->width, pbeam->amplitude, pbeam->freq, pbeam->speed, pbeam->segments, pbeam->flags);
				tri.End();
				break;
			case TE_BEAMTORUS:
				tri.Begin(TRI_QUAD_STRIP);
				R_DrawTorus(pbeam->source, pbeam->delta, pbeam->width, pbeam->amplitude, pbeam->freq, pbeam->speed, pbeam->segments);
				tri.End();
				break;
			case TE_BEAMDISK:
				tri.Begin(TRI_QUAD_STRIP);
				R_DrawDisk(pbeam->source, pbeam->delta, pbeam->width, pbeam->amplitude, pbeam->freq, pbeam->speed, pbeam->segments);
				tri.End();
				break;
			case TE_BEAMCYLINDER:
				tri.Begin(TRI_QUAD_STRIP);
				R_DrawCylinder(pbeam->source, pbeam->delta, pbeam->width, pbeam->amplitude, pbeam->freq, pbeam->speed, pbeam->segments);
				tri.End();
				break;
			case TE_BEAMFOLLOW:
				tri.Begin(TRI_QUADS);
				R_DrawBeamFollow(pbeam);
				tri.End();
				break;
			case TE_BEAMRING:
				tri.Begin(TRI_QUAD_STRIP);
				R_DrawRing(pbeam->source, pbeam->delta, pbeam->width, pbeam->amplitude, pbeam->freq, pbeam->speed, pbeam->segments);
				tri.End();
				break;
			default:

				break;
			}
		}
	}

}

void R_DrawBeamEntList(float frametime)
{
	float brightness, speed, amplitude;
	BEAM beam;
	int beamfx;

	for (int i = 0; i < cl_numbeamentities; i++)
	{
		beamfx = cl_beamentities[i]->curstate.rendermode & 0xF;

		R_BeamSetup(&beam, cl_beamentities[i]->origin, cl_beamentities[i]->curstate.angles, cl_beamentities[i]->curstate.movetype, 0.0, cl_beamentities[i]->curstate.scale,
			(float)cl_beamentities[i]->curstate.body * 0.01, (float)CL_FxBlend(cl_beamentities[i]) / 255.0f, cl_beamentities[i]->curstate.animtime);

		SetBeamAttributes(&beam, (float)cl_beamentities[i]->curstate.rendercolor.r / 255.0f, (float)cl_beamentities[i]->curstate.rendercolor.g / 255.0f,
			(float)cl_beamentities[i]->curstate.rendercolor.b / 255.0f, 0, cl_beamentities[i]->curstate.frame);


		if (beamfx == BEAM_ENTPOINT)
		{
			beam.type = 0;
			beam.flags = FBEAM_ENDENTITY;
			beam.startEntity = 0;
			beam.endEntity = cl_beamentities[i]->curstate.skin;
		}
		else if (beamfx == BEAM_ENTS)
		{
			beam.type = 0;
			beam.flags = FBEAM_ENDENTITY | FBEAM_STARTENTITY;
			beam.startEntity = cl_beamentities[i]->curstate.sequence;
			beam.endEntity = cl_beamentities[i]->curstate.skin;
		}

		if ((cl_beamentities[i]->curstate.rendermode & BEAM_FSINE) != 0)
			beam.flags |= BEAM_FSINE;
		if ((cl_beamentities[i]->curstate.rendermode & BEAM_FSOLID) != 0)
			beam.flags |= BEAM_FSOLID;
		if ((cl_beamentities[i]->curstate.rendermode & BEAM_FSHADEIN) != 0)
			beam.flags |= BEAM_FSHADEIN;
		if ((cl_beamentities[i]->curstate.rendermode & BEAM_FSHADEOUT) != 0)
			beam.flags |= BEAM_FSHADEOUT;

		beam.pFollowModel = 0;

		R_BeamDraw(&beam, frametime);
	}
}

void R_BeamDrawList(void)
{
	float	frametime;
	BEAM* pbeam, * pkill;

	if (!gpActiveBeams && cl_numbeamentities == 0)
		return;

	frametime = cl.time - cl.oldtime;
#if defined ( GLQUAKE )
	qglDisable(GL_ALPHA_TEST);
	qglDepthMask(GL_FALSE);
#endif
	tri.CullFace(TRI_NONE);

	for (;;)
	{
		pkill = gpActiveBeams;
		if (pkill && pkill->die < cl.time && !(pkill->flags & FBEAM_FOREVER))
		{
			gpActiveBeams = pkill->next;
			pkill->next = gpFreeBeams;
			gpFreeBeams = pkill;
			continue;
		}

		break;
	}

	for (pbeam = gpActiveBeams; pbeam; pbeam = pbeam->next)
	{
		for (;;)
		{
			pkill = pbeam->next;

			if (pkill && pkill->die < cl.time && !(pkill->flags & FBEAM_FOREVER))
			{
				pbeam->next = pkill->next;
				pkill->next = gpFreeBeams;
				gpFreeBeams = pkill;
				continue;
			}

			break;
		}

		R_BeamDraw(pbeam, frametime);
	}

	R_DrawBeamEntList(frametime);

#if defined ( GLQUAKE )
	qglDepthMask(GL_TRUE);
#endif
	tri.CullFace(TRI_FRONT);
	tri.RenderMode(kRenderNormal);
}

void R_GetPackedColor( short* packed, short color )
{
	if (packed == NULL)
	{
		Con_Printf(const_cast<char*>("R_GetPackedColor called without packed!\n"));
		return;
	}

	*packed = 
#ifndef GLQUAKE
		hlRGB(host_basepal, color);
#endif

#ifdef GLQUAKE
	0
#endif
		;
}

short R_LookupColor( byte r, byte g, byte b )
{
	float rr, gg, bb;
	int bestidx = (word)-1;
	float bestdiff = 999999;
	float diff;
	PackedColorVec* pal = (PackedColorVec*)host_basepal;

	for (int i = 0; i < 256; i++)
	{
		rr = r - pal[i].r;
		gg = g - pal[i].g;
		bb = b - pal[i].b;

		diff = rr*rr * 0.2f + bb*bb * 0.3f + gg*gg * 0.5f;
		if (diff < bestdiff)
		{
			bestidx = i;
			bestdiff = diff;
		}
	}

	return bestidx != (word)-1 ? bestidx : 0;
}

void R_ReadPointFile_f(void)
{
	FILE* f;
	vec3_t	org;
	int		r;
	int		c;
	particle_t* p;
	char	name[MAX_PATH];
	char	base[MAX_PATH];

	if (cl.worldmodel == nullptr)
		return;

	COM_StripExtension(cl.worldmodel->name, base);

	sprintf(name, "maps/%s.pts", base);
	name[sizeof(name) - 1] = 0;
	COM_FixSlashes(name);

	f = fopen(name, "rb");
	if (!f)
	{
		Con_Printf(const_cast<char*>("couldn't open %s\n"), name);
		return;
	}

	Con_Printf(const_cast<char*>("Reading %s...\n"), name);
	c = 0;
	for (;; )
	{
		r = fscanf(f, "%f %f %f\n", &org[0], &org[1], &org[2]);
		if (r != 3)
			break;
		c++;

		if (!free_particles)
		{
			Con_Printf(const_cast<char*>("Not enough free particles\n"));
			break;
		}
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = 99999;
		p->color = -short(c) & 15;
		p->type = pt_static;
		VectorCopy(vec3_origin, p->vel);
		VectorCopy(org, p->org);
	}

	fclose(f);
	Con_Printf(const_cast<char*>("%i points read\n"), c);
}

void R_BeamInit()
{
	// why 0x3E00?
	gBeams = (BEAM*)Hunk_AllocName(sizeof(BEAM) * MAX_BEAMS, const_cast<char*>("lightning"));
}

/*
===============
R_InitParticles
===============
*/
void R_InitParticles(void)
{
	int		i;

	i = COM_CheckParm(const_cast<char*>("-particles"));

	if (i)
	{
		r_numparticles = (int)(Q_atoi((char*)com_argv[i + 1]));
		if (r_numparticles < ABSOLUTE_MIN_PARTICLES)
			r_numparticles = ABSOLUTE_MIN_PARTICLES;
	}
	else
	{
		r_numparticles = MAX_PARTICLES;
	}

	particles = (particle_t*)
		Hunk_AllocName(r_numparticles * sizeof(particle_t), const_cast<char*>("particles"));

	R_BeamInit();
}

void R_FreeDeadParticles(particle_t **ppparticles)
{
	particle_t* kill, *queue, *save;

	if (*ppparticles == NULL)
		return;

	// перемещение показанных/пропущенных частиц в разряд доступных
	kill = *ppparticles;
	do
	{
		if (kill->die >= cl.time)
			break;

		if (kill->deathfunc)
			kill->deathfunc(kill);

		save = kill->next;
		kill->deathfunc = NULL;
		kill->next = free_particles;
		free_particles = kill;
		kill = save;
		*ppparticles = kill;
	} while (kill != NULL);

	for (kill = *ppparticles; kill != NULL; kill = kill->next)
	{
		for (queue = kill->next; queue != NULL; queue = kill->next)
		{
			if (queue->die >= cl.time)
				break;

			if (queue->deathfunc)
				queue->deathfunc(queue);

			save = queue->next;
			queue->deathfunc = NULL;
			kill->next = save;
			queue->next = free_particles;
			free_particles = queue;
		}
	}
}

void R_TracerDraw(void)
{
	float		scale;
	float		attenuation;
	float		frametime;
	float		clipDist;
	float		gravity;
	float		size;
	vec3_t		up, right;
	vec3_t		start, end;
	particle_t* p;
	vec3_t		screenLast, screen;
	vec3_t		tmp, normal;
	//	int			clip;

	qboolean	draw = TRUE;

	if (!gpActiveTracers)
		return; // no tracers to draw

	gTracerColors[4].r = tracerRed.value * tracerAlpha.value * 255.0;
	gTracerColors[4].g = tracerGreen.value * tracerAlpha.value * 255.0;
	gTracerColors[4].b = tracerBlue.value * tracerAlpha.value * 255.0;

	frametime = cl.time - cl.oldtime;

	R_FreeDeadParticles(&gpActiveTracers);

	VectorScale(vup, 1.5, up);
	VectorScale(vright, 1.5, right);

	if (tri.SpriteTexture(cl_sprite_dot, 0))
	{
		gravity = movevars.gravity * frametime;
		size = DotProduct(r_origin, vpn);

		scale = 1.0 - frametime * 0.9;
		if (scale < 0.0)
			scale = 0.0;

		tri.RenderMode(kRenderTransAdd);
		tri.CullFace(TRI_NONE);

		for (p = gpActiveTracers; p; p = p->next)
		{
			attenuation = (p->die - cl.time);

			if (attenuation > 0.1)
				attenuation = 0.1;

			VectorScale(p->vel, (p->ramp * attenuation), end);
			VectorAdd(p->org, end, end);
			VectorCopy(p->org, start);

			draw = TRUE;

			color24* pColor;
			if (ScreenTransform(start, screen) || ScreenTransform(end, screenLast))
			{
				float fraction;
				float dist1, dist2;

				dist1 = DotProduct(vpn, start) - size;
				dist2 = DotProduct(vpn, end) - size;

				if (dist1 <= 0.0 && dist2 <= 0.0)
					draw = FALSE;

				if (draw == TRUE)
				{
					clipDist = dist2 - dist1;
					if (clipDist < 0.01)
						draw = FALSE;
				}

				if (draw == TRUE)
				{
					fraction = dist1 / clipDist;

					if (dist1 > 0.0)
					{
						end[0] = start[0] + (end[0] - start[0]) * fraction;
						end[1] = start[1] + (end[1] - start[1]) * fraction;
						end[2] = start[2] + (end[2] - start[2]) * fraction;
					}
					else
					{
						start[0] = start[0] + (end[0] - start[0]) * fraction;
						start[1] = start[1] + (end[1] - start[1]) * fraction;
						start[2] = start[2] + (end[2] - start[2]) * fraction;
					}

					// Transform point into screen space
					ScreenTransform(start, screen);
					ScreenTransform(end, screenLast);
				}
			}

			if (draw == TRUE)
			{
				// Transform point into screen space
				ScreenTransform(start, screen);
				ScreenTransform(end, screenLast);

				// Build world-space normal to screen-space direction vector
				VectorSubtract(screenLast, screen, tmp);

				// We don't need Z, we're in screen space
				tmp[2] = 0;

				VectorNormalize(tmp);

				// build point along noraml line (normal is -y, x)
				VectorScale(vup, tmp[0] * gTracerSize[p->type], normal);
				VectorMA(normal, -tmp[1] * gTracerSize[p->type], vright, normal);

				tri.Begin(TRI_QUADS);

				pColor = &gTracerColors[p->color];
				tri.Color4ub(pColor->r, pColor->g, pColor->b, p->packedColor);

				tri.Brightness(0);
				tri.TexCoord2f(0, 0);
				tri.Vertex3f(start[0] + normal[0], start[1] + normal[1], start[2] + normal[2]);

				tri.Brightness(1);
				tri.TexCoord2f(0, 1);
				tri.Vertex3f(end[0] + normal[0], end[1] + normal[1], end[2] + normal[2]);

				tri.Brightness(1);
				tri.TexCoord2f(1, 1);
				tri.Vertex3f(end[0] - normal[0], end[1] - normal[1], end[2] - normal[2]);

				tri.Brightness(0);
				tri.TexCoord2f(1, 0);
				tri.Vertex3f(start[0] - normal[0], start[1] - normal[1], start[2] - normal[2]);

				tri.End();
			}

			p->org[0] = frametime * p->vel[0] + p->org[0];
			p->org[1] = frametime * p->vel[1] + p->org[1];
			p->org[2] = frametime * p->vel[2] + p->org[2];

			if (p->type == pt_grav)
			{
				p->vel[0] *= scale;
				p->vel[1] *= scale;
				p->vel[2] -= gravity;

				p->packedColor = 255 * (p->die - cl.time) * 2;

				if (p->packedColor > 255)
					p->packedColor = 255;
			}
			else if (p->type == pt_slowgrav)
			{
				p->vel[2] = gravity * 0.05;
			}
		}

		tri.CullFace(TRI_FRONT);
		tri.RenderMode(kRenderNormal);
	}
}

/*
===============
R_DrawParticles
===============
*/
extern	cvar_t	sv_gravity;

void R_DrawParticles(void)
{
	particle_t		*p, *kill;
	float			grav;
	int				i;
	float			time2, time3;
	float			time1;
	float			dvel;
	float			frametime;

#ifdef GLQUAKE
	vec3_t			up, right;
	float			scale;

	GL_Bind(particletexture);
	qglEnable(GL_ALPHA_TEST);
	qglEnable(GL_BLEND);
	qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	qglBegin(GL_TRIANGLES);

	VectorScale(vup, 1.5, up);
	VectorScale(vright, 1.5, right);
#else
	D_StartParticles();

	VectorScale(vright, xscaleshrink, r_pright);
	VectorScale(vup, yscaleshrink, r_pup);
	VectorCopy(vpn, r_ppn);
#endif

	frametime = cl.time - cl.oldtime;
	time3 = frametime * 15;
	time2 = frametime * 10;
	dvel = 4 * frametime;
	grav = frametime * sv_gravity.value * 0.05;

	R_FreeDeadParticles(&active_particles);

	for (p = active_particles; p; p = p->next)
	{
		if (p->type != pt_blob)
		{
#if defined ( GLQUAKE )
			word* pb;
			byte rgba[4];

			// hack a scale up to keep particles from disapearing
			scale = (p->org[0] - r_origin[0]) * vpn[0] + (p->org[1] - r_origin[1]) * vpn[1]
				+ (p->org[2] - r_origin[2]) * vpn[2];

			if (scale < 20)
				scale = 1;
			else
				scale = 1 + scale * 0.004;

			pb = &host_basepal[4 * p->color];

			if (filterMode)
			{
				rgba[0] = pb[2] * filterColorRed * filterBrightness;
				rgba[1] = pb[1] * filterColorGreen * filterBrightness;
				rgba[2] = pb[0] * filterColorBlue * filterBrightness;
				rgba[3] = filterBrightness * (255.0 * filterBrightness);
			}
			else
			{
				rgba[0] = pb[2];
				rgba[1] = pb[1];
				rgba[2] = pb[0];
				rgba[3] = 255;
			}

			qglColor3ubv(rgba);
			qglTexCoord2f(0, 0);
			qglVertex3fv(p->org);
			qglTexCoord2f(1, 0);
			qglVertex3f(p->org[0] + up[0] * scale, p->org[1] + up[1] * scale, p->org[2] + up[2] * scale);
			qglTexCoord2f(0, 1);
			qglVertex3f(p->org[0] + right[0] * scale, p->org[1] + right[1] * scale, p->org[2] + right[2] * scale);
#else
			D_DrawParticle(p);
#endif
		}

		if (p->type != pt_clientcustom)
		{
			p->org[0] += p->vel[0] * frametime;
			p->org[1] += p->vel[1] * frametime;
			p->org[2] += p->vel[2] * frametime;
		}

		switch (p->type)
		{
		case pt_grav:
			p->vel[2] -= grav * 20;
			break;
		case pt_slowgrav:
			p->vel[2] = grav;
			break;
		case pt_fire:
			p->ramp += frametime * 5;
			if (p->ramp >= 6)
				p->die = -1;
			else
			{
				p->color = ramp3[(int)p->ramp];
#if defined( GLQUAKE )
				p->packedColor = 0;
#else
				p->packedColor = hlRGB(host_basepal, p->color);
#endif
			}
			p->vel[2] += grav;
			break;
		case pt_explode:
			p->ramp += time2;
			if (p->ramp >= 8)
				p->die = -1;
			else
			{
				p->color = ramp1[(int)p->ramp];
#if defined( GLQUAKE )
				p->packedColor = 0;
#else
				p->packedColor = hlRGB(host_basepal, p->color);
#endif
			}

			for (i = 0; i<3; i++)
				p->vel[i] += p->vel[i] * dvel;
			p->vel[2] -= grav;

			break;
		case pt_explode2:
			p->ramp += time3;
			if (p->ramp >= 8)
				p->die = -1;
			else
			{
				p->color = ramp2[(int)p->ramp];
#if defined( GLQUAKE )
				p->packedColor = 0;
#else
				p->packedColor = hlRGB(host_basepal, p->color);
#endif
			}
			for (i = 0; i<3; i++)
				p->vel[i] -= p->vel[i] * frametime;
			p->vel[2] -= grav;
			
			break;
		case pt_blob:
		case pt_blob2:
			p->ramp += time2;
			if (p->ramp >= 9)
				p->ramp = 0;

			p->color = gSparkRamp[(int)p->ramp];
#if defined( GLQUAKE )
			p->packedColor = 0;
#else
			p->packedColor = hlRGB(host_basepal, p->color);
#endif

			for (i = 0; i<2; i++)
				p->vel[i] -= p->vel[i] * frametime * 0.5;
			p->vel[2] -= grav * 5.0;
			if (RandomLong(0, 3) != 0)
				p->type = pt_blob;
			else
				p->type = pt_blob2;
			break;
		case pt_vox_slowgrav:
			p->vel[2] -= grav * 4;
			break;
		case pt_vox_grav:
			p->vel[2] -= grav * 8;
			break;
		case pt_clientcustom:
			if (p->callback)
				p->callback(p, frametime);
			break;
		default:
			break;
		}
	}

#ifdef GLQUAKE
	qglEnd();
#endif

	R_TracerDraw();
	R_BeamDrawList();

#ifdef GLQUAKE
	qglDisable(GL_BLEND);
	qglDisable(GL_ALPHA_TEST);
#else
	D_EndParticles();
#endif
}

void R_ParseParticleEffect()
{
	int i, color, msgcount;
	vec3_t org, dir;

	for (i = 0; i < 3; i++)
		org[i] = MSG_ReadCoord(&net_message);

	for (i = 0; i < 3; i++)
		dir[i] = (float)MSG_ReadChar() / 16.0f;

	msgcount = MSG_ReadByte();
	color = MSG_ReadByte();

	R_RunParticleEffect(org, dir, color, msgcount != 255 ? msgcount : 1024);
}

void R_KillDeadBeams(int deadEntity)
{
	BEAM* pbeam;
	BEAM* pnewlist;
	BEAM* pnext;
	particle_t* pHead;

	pbeam = gpActiveBeams;  // Old list.
	pnewlist = NULL;		// New list.

	while (pbeam)
	{
		pnext = pbeam->next;
		if (pbeam->startEntity != deadEntity)   // Link into new list.
		{
			pbeam->next = pnewlist;
			pnewlist = pbeam;

			pbeam = pnext;
			continue;
		}

		pbeam->flags &= ~(FBEAM_STARTENTITY | FBEAM_ENDENTITY);
		if (pbeam->type != TE_BEAMFOLLOW)
		{
			// Die Die Die!
			pbeam->die = cl.time - 0.1;

			// Kill off particles
			pHead = pbeam->particles;
			while (pHead)
			{
				pHead->die = cl.time - 0.1;
				pHead = pHead->next;
			}

			// Free particles that have died off.
			R_FreeDeadParticles(&pbeam->particles);

			// Clear us out
			Q_memset(pbeam, 0, sizeof(*pbeam));

			// Now link into free list;
			pbeam->next = gpFreeBeams;
			gpFreeBeams = pbeam;
		}
		else
		{
			// Stay active
			pbeam->next = pnewlist;
			pnewlist = pbeam;
		}
		pbeam = pnext;
	}

	// We now have a new list with the bogus stuff released.
	gpActiveBeams = pnewlist;
}

void R_BeamClear(void)
{
	gpFreeBeams = gBeams;
	gpActiveBeams = NULL;

	Q_memset(gBeams, 0, sizeof(BEAM) * MAX_BEAMS);

	for (int i = 0; i < MAX_BEAMS; i++)
		gBeams[i].next = &gBeams[i + 1];

	gBeams[MAX_BEAMS - 1].next = NULL;
}

void R_ClearParticles(void)
{
	active_particles = gpActiveTracers = NULL;
	free_particles = particles;

	for (int i = 0; i < r_numparticles; i++)
		particles[i].next = &particles[i + 1];

	particles[r_numparticles - 1].next = NULL;

	R_BeamClear();
}
