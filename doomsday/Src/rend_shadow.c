
//**************************************************************************
//**
//** REND_SHADOW.C
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_refresh.h"
#include "de_render.h"
#include "de_play.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int		useShadows = true;
int		shadowMaxRad = 80;
int		shadowMaxDist = 1000;
float	shadowFactor = 0.5f;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//===========================================================================
// Rend_ProcessThingShadow
//===========================================================================
void Rend_ProcessThingShadow(mobj_t *mo)
{
	fixed_t		moz;
	float		height, moh, halfmoh, color, pos[2];
	sector_t	*sec = mo->subsector->sector;
	int			radius, i;
	rendpoly_t	poly;

	// Is this too far?
	pos[VX] = FIX2FLT( mo->x );
	pos[VY] = FIX2FLT( mo->y );
	if(Rend_PointDist2D(pos) > shadowMaxDist) return;

	// Apply a Short Range Visual Offset?
	if(r_use_srvo && mo->state && mo->tics >= 0)
	{
		float mul = mo->tics / (float) mo->state->tics;				
		pos[VX] += FIX2FLT(mo->srvo[VX] << 8) * mul;
		pos[VY] += FIX2FLT(mo->srvo[VY] << 8) * mul;
	}

	// Check the height.
	moz = mo->z - mo->floorclip;
	if(mo->ddflags & DDMF_BOB)
	{
		moz -= R_GetBobOffset(mo);
	}
	height = FIX2FLT( moz - mo->floorz );
	moh = FIX2FLT( mo->height );
	if(!moh) moh = 1;
	if(height > moh) return;	// Too far.
	if(moz + mo->height < mo->floorz) return;
	
	// Calculate the strength of the shadow.
	color = shadowFactor 
		* sec->lightlevel/255.0f
		* (1 - mo->translucency/255.0f);
	halfmoh = moh/2;
	if(height > halfmoh) color *= 1 - (height - halfmoh)/(moh - halfmoh);
	if(whitefog) color /= 2;
	if(color <= 0) return;		// Can't be seen.
	if(color > 1) color = 1;

	// Calculate the radius of the shadow.
	radius = R_VisualRadius(mo);
	if(!radius) return;
	if(radius > shadowMaxRad) radius = shadowMaxRad;

	// Prepare the poly.
	memset(&poly, 0, sizeof(poly));
	poly.type = RP_FLAT;
	poly.flags = RPF_SHADOW;
//	poly.light = (lumobj_t*) (radius*2);
/*	gl.TexCoord2f((prim->texoffx - vtx->pos[VX])/prim->shadowradius,
				(prim->texoffy - vtx->pos[VY])/prim->shadowradius);*/
	poly.texoffx = pos[VX] + radius;
	poly.texoffy = pos[VY] + radius;
	poly.top = FIX2FLT( mo->floorz ) + 0.2f;

	poly.numvertices = 4;
	poly.vertices[0].pos[VX] = pos[VX] - radius;
	poly.vertices[0].pos[VY] = pos[VY] + radius;
	poly.vertices[1].pos[VX] = pos[VX] + radius;
	poly.vertices[1].pos[VY] = pos[VY] + radius;
	poly.vertices[2].pos[VX] = pos[VX] + radius;
	poly.vertices[2].pos[VY] = pos[VY] - radius;
	poly.vertices[3].pos[VX] = pos[VX] - radius;
	poly.vertices[3].pos[VY] = pos[VY] - radius;
	for(i = 0; i < 4; i++)
		memset(poly.vertices[i].color.rgb, color*255, 3);

	RL_AddPoly(&poly);
}

//===========================================================================
// Rend_RenderShadows
//===========================================================================
void Rend_RenderShadows(void)
{
#if 0
	sector_t *sec;
	mobj_t *mo;
	int i;

	if(!useShadows) return;

	// Check all mobjs in all visible sectors.
	for(i = 0; i < numsectors; i++)
	{
		if(!(secinfo[i].flags & SIF_VISIBLE)) continue;
		sec = SECTOR_PTR(i);
		for(mo = sec->thinglist; mo; mo = mo->snext)
		{
			// Should this mobj have a shadow?
			if(mo->frame & FF_FULLBRIGHT 
				|| mo->ddflags & DDMF_DONTDRAW
				|| mo->ddflags & DDMF_ALWAYSLIT) continue;
			Rend_ProcessThingShadow(mo);	
		}
	}
#endif
}

