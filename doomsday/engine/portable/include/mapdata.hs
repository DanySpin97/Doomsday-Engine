# $Id$
# Runtime map data defitions. Processed by the makedmt.py script.

public
#define DMT_VERTEX_POS  DDVT_FLOAT
end

internal
// Each Sector and SideDef has an origin in the world (used for distance based delta queuing)
typedef struct origin_s {
    float               pos[2];
} origin_t;

#define LO_prev     link[0]
#define LO_next     link[1]

typedef struct shadowvert_s {
    float           inner[2];
    float           extended[2];
} shadowvert_t;

typedef struct lineowner_s {
    struct linedef_s *lineDef;
    struct lineowner_s *link[2];    // {prev, next} (i.e. {anticlk, clk}).
    binangle_t      angle;          // between this and next clockwise.
    shadowvert_t    shadowOffsets;
} lineowner_t;

#define V_pos                   v.pos

typedef struct mvertex_s {
    // Vertex index. Always valid after loading and pruning of unused
    // vertices has occurred.
    int         index;

    // Reference count. When building normal node info, unused vertices
    // will be pruned.
    int         refCount;

    // Usually NULL, unless this vertex occupies the same location as a
    // previous vertex. Only used during the pruning phase.
    struct vertex_s *equiv;

    struct edgetip_s *tipSet; // Set of wall_tips.

// Final data.
    double      pos[2];
} mvertex_t;
end

struct Vertex
    -       uint        numLineOwners // Number of line owners.
    -       lineowner_t* lineOwners // Lineowner base ptr [numlineowners] size. A doubly, circularly linked list. The base is the line with the lowest angle and the next-most with the largest angle.
    -       fvertex_t   v
    -       mvertex_t   buildData
end

internal
// Helper macros for accessing seg data elements.
#define FRONT 0
#define BACK  1

#define HE_v(n)                 v[(n)? 1:0]
#define HE_vpos(n)              HE_v(n)->V_pos

#define HE_v1                   HE_v(0)
#define HE_v1pos                HE_v(0)->V_pos

#define HE_v2                   HE_v(1)
#define HE_v2pos                HE_v(1)->V_pos

#define HE_sector(n)            sec[(n)? 1:0]
#define HE_frontsector          HE_sector(FRONT)
#define HE_backsector           HE_sector(BACK)

#define HEDGE_SIDEDEF(he)       ((he)->lineDef->sideDefs[(he)->side])

// HEdge flags
#define HEDGEF_POLYOBJ          0x1 /// < Half-edge is part of a poly object.

// HEdge frame flags
#define HEDGEINF_FACINGFRONT    0x0001
end

public
#define DMT_HEDGE_SIDEDEF       DDVT_PTR
end

struct HEdge
    PTR     vertex_s*[2] v          // [Start, End] of the segment.
    PTR     linedef_s*  lineDef
    PTR     sector_s*[2] sec
    PTR     bspleaf_s*  bspLeaf
    PTR     hedge_s*    twin
    ANGLE   angle_t     angle
    BYTE    byte        side        // 0=front, 1=back
    BYTE    byte        flags
    FLOAT   float       length      // Accurate length of the segment (v1 -> v2).
    FLOAT   float       offset
    -       biassurface_t*[3] bsuf // 0=middle, 1=top, 2=bottom
    -       short       frameFlags
end

internal
#define BLF_MIDPOINT             0x80 ///< Midpoint is tri-fan centre.
end

struct BspLeaf
    UINT    uint        hedgeCount
    PTR     hedge_s**   hedges // [hedgeCount] size.
    PTR     polyobj_s*  polyObj // NULL, if there is no polyobj.
    PTR     sector_s*   sector
    -       int         addSpriteCount // frame number of last R_AddSprites
    -       uint        inSectorID
    -       int         flags
    -       int         validCount
    -       uint[NUM_REVERB_DATA] reverb
    -       AABoxf      aaBox // Min and max points.
    -       float[2]    worldGridOffset // Offset to align the top left of the bBox to the world grid.
    -       fvertex_t   midPoint // Center of vertices.
    -       ushort      numVertices
    -       fvertex_s** vertices // [numvertices] size
    -       shadowlink_s* shadows
    -       biassurface_s** bsuf // [sector->planeCount] size.
end

internal
typedef enum {
    MEC_UNKNOWN = -1,
    MEC_FIRST = 0,
    MEC_METAL = MEC_FIRST,
    MEC_ROCK,
    MEC_WOOD,
    MEC_CLOTH,
    NUM_MATERIAL_ENV_CLASSES
} material_env_class_t;

#define VALID_MATERIAL_ENV_CLASS(v) ((v) >= MEC_FIRST && (v) < NUM_MATERIAL_ENV_CLASSES)

struct material_variantlist_node_s;
end

public
#define DMT_MATERIAL_FLAGS      DDVT_SHORT
#define DMT_MATERIAL_WIDTH      DDVT_INT
#define DMT_MATERIAL_HEIGHT     DDVT_INT
end

struct material
    -       ded_material_s* _def
    -       material_variantlist_node_s* _variants
    -       material_env_class_t _envClass // Environmental sound class.
    -       materialid_t _primaryBind // Unique identifier of the MaterialBind associated with this Material or @c NULL if not bound.
    -       Size2i*     _size // Logical dimensions in world-space units.
    -       short       _flags // @see materialFlags
    -       boolean     _inAnimGroup // @c true if belongs to some animgroup.
    -       boolean     _isCustom
    -       texture_s*  _detailTex;
    _       float       _detailScale;
    -       float       _detailStrength;
    -       texture_s*  _shinyTex;
    -       blendmode_t _shinyBlendmode;
    -       float[3]    _shinyMinColor;
    -       float       _shinyStrength;
    -       texture_s*  _shinyMaskTex;
    -       byte        _prepared;
end

internal
// Internal surface flags:
#define SUIF_PVIS             0x0001
#define SUIF_FIX_MISSING_MATERIAL 0x0002 // Current Material is a fix replacement
                                     // (not sent to clients, returned via DMU etc).
#define SUIF_BLEND            0x0004 // Surface possibly has a blended texture.
#define SUIF_NO_RADIO         0x0008 // No fakeradio for this surface.

#define SUIF_UPDATE_FLAG_MASK 0xff00
#define SUIF_UPDATE_DECORATIONS 0x8000

typedef struct surfacedecor_s {
    float               pos[3]; // World coordinates of the decoration.
    BspLeaf*		bspLeaf;
    const struct ded_decorlight_s* def;
} surfacedecor_t;
end

struct Surface
    -       void*       owner // Either @c DMU_SIDEDEF, or @c DMU_PLANE
    INT     int         flags // SUF_ flags
    -       int         oldFlags
    PTR     material_t* material
    BLENDMODE blendmode_t blendMode
    FLOAT   float[3]    tangent
    FLOAT   float[3]    bitangent
    FLOAT   float[3]    normal
    FLOAT   float[2]    offset // [X, Y] Planar offset to surface material origin.
	-		float[2][2] oldOffset
    -       float[2]    visOffset
    -       float[2]    visOffsetDelta
    FLOAT   float[4]    rgba // Surface color tint
    -       short       inFlags // SUIF_* flags
    -       uint        numDecorations
    -       surfacedecor_t *decorations
end

internal
typedef enum {
    PLN_FLOOR,
    PLN_CEILING,
    PLN_MID,
    NUM_PLANE_TYPES
} planetype_t;
end

internal
#define PS_tangent              surface.tangent
#define PS_bitangent            surface.bitangent
#define PS_normal               surface.normal
#define PS_material             surface.material
#define PS_offset               surface.offset
#define PS_visoffset            surface.visOffset
#define PS_rgba                 surface.rgba
#define PS_flags                surface.flags
#define PS_inflags              surface.inFlags
end

struct Plane
    PTR     ddmobj_base_t origin
    PTR     sector_s*   sector // Owner of the plane (temp)
    -       Surface     surface
    FLOAT   float       height // Current height
    -       float[2]    oldHeight
    FLOAT   float       target // Target height
    FLOAT   float       speed // Move speed
    -       float       visHeight // Visible plane height (smoothed)
    -       float       visHeightDelta
    -       planetype_t type // PLN_* type.
    -       int         planeID
end

internal
// Helper macros for accessing sector floor/ceiling plane data elements.
#define SP_plane(n)             planes[(n)]

#define SP_planesurface(n)      SP_plane(n)->surface
#define SP_planeheight(n)       SP_plane(n)->height
#define SP_planetangent(n)      SP_plane(n)->surface.tangent
#define SP_planebitangent(n)    SP_plane(n)->surface.bitangent
#define SP_planenormal(n)       SP_plane(n)->surface.normal
#define SP_planematerial(n)     SP_plane(n)->surface.material
#define SP_planeoffset(n)       SP_plane(n)->surface.offset
#define SP_planergb(n)          SP_plane(n)->surface.rgba
#define SP_planetarget(n)       SP_plane(n)->target
#define SP_planespeed(n)        SP_plane(n)->speed
#define SP_planeorigin(n)       SP_plane(n)->origin
#define SP_planevisheight(n)    SP_plane(n)->visHeight

#define SP_ceilsurface          SP_planesurface(PLN_CEILING)
#define SP_ceilheight           SP_planeheight(PLN_CEILING)
#define SP_ceiltangent          SP_planetangent(PLN_CEILING)
#define SP_ceilbitangent        SP_planebitangent(PLN_CEILING)
#define SP_ceilnormal           SP_planenormal(PLN_CEILING)
#define SP_ceilmaterial         SP_planematerial(PLN_CEILING)
#define SP_ceiloffset           SP_planeoffset(PLN_CEILING)
#define SP_ceilrgb              SP_planergb(PLN_CEILING)
#define SP_ceiltarget           SP_planetarget(PLN_CEILING)
#define SP_ceilspeed            SP_planespeed(PLN_CEILING)
#define SP_ceilorigin           SP_planeorigin(PLN_CEILING)
#define SP_ceilvisheight        SP_planevisheight(PLN_CEILING)

#define SP_floorsurface         SP_planesurface(PLN_FLOOR)
#define SP_floorheight          SP_planeheight(PLN_FLOOR)
#define SP_floortangent         SP_planetangent(PLN_FLOOR)
#define SP_floorbitangent       SP_planebitangent(PLN_FLOOR)
#define SP_floornormal          SP_planenormal(PLN_FLOOR)
#define SP_floormaterial        SP_planematerial(PLN_FLOOR)
#define SP_flooroffset          SP_planeoffset(PLN_FLOOR)
#define SP_floorrgb             SP_planergb(PLN_FLOOR)
#define SP_floortarget          SP_planetarget(PLN_FLOOR)
#define SP_floorspeed           SP_planespeed(PLN_FLOOR)
#define SP_floororigin          SP_planeorigin(PLN_FLOOR)
#define SP_floorvisheight       SP_planevisheight(PLN_FLOOR)

#define S_skyfix(n)             skyFix[(n)? 1:0]
#define S_floorskyfix           S_skyfix(PLN_FLOOR)
#define S_ceilskyfix            S_skyfix(PLN_CEILING)
end

internal
// Sector frame flags
#define SIF_VISIBLE         0x1     // Sector is visible on this frame.
#define SIF_FRAME_CLEAR     0x1     // Flags to clear before each frame.
#define SIF_LIGHT_CHANGED   0x2

// Sector flags.
#define SECF_UNCLOSED       0x1     // An unclosed sector (some sort of fancy hack).

typedef struct msector_s {
    // Sector index. Always valid after loading & pruning.
    int         index;

    // Suppress superfluous mini warnings.
    int         warnedFacing;
    int         refCount;
} msector_t;
end

struct Sector
    -       int         frameFlags
    INT     int         validCount // if == validCount, already checked.
    -       int         flags
    -       AABox       aaBox // Bounding box for the sector.
    -       float       roughArea // Rough approximation of sector area.
    FLOAT   float       lightLevel
    -       float       oldLightLevel
    FLOAT   float[3]    rgb
    -       float[3]    oldRGB
    PTR     mobj_s*     mobjList // List of mobjs in the sector.
    UINT    uint        lineDefCount
    PTR     linedef_s** lineDefs // [lineDefCount+1] size.
    UINT    uint        bspLeafCount
    PTR     bspleaf_s** bspLeafs // [bspLeafCount+1] size.
    -       uint        numReverbBspLeafAttributors
    -       bspleaf_s** reverbBspLeafs // [numReverbBspLeafAttributors] size.
    PTR     ddmobj_base_t origin
    UINT    uint        planeCount
    -       plane_s**   planes // [planeCount+1] size.
    -       uint        blockCount // Number of gridblocks in the sector.
    -       uint        changedBlockCount // Number of blocks to mark changed.
    -       ushort*     blocks // Light grid block indices.
    FLOAT   float[NUM_REVERB_DATA] reverb
    -       msector_t   buildData
end

internal
// Sections of a sidedef
typedef enum sidedefsection_e {
    SS_MIDDLE,
    SS_TOP,
    SS_BOTTOM
} sidedefsection_t;

// Helper macros for accessing sidedef top/middle/bottom section data elements.
#define SW_surface(n)           sections[(n)]
#define SW_surfaceflags(n)      SW_surface(n).flags
#define SW_surfaceinflags(n)    SW_surface(n).inFlags
#define SW_surfacematerial(n)   SW_surface(n).material
#define SW_surfacetangent(n)    SW_surface(n).tangent
#define SW_surfacebitangent(n)  SW_surface(n).bitangent
#define SW_surfacenormal(n)     SW_surface(n).normal
#define SW_surfaceoffset(n)     SW_surface(n).offset
#define SW_surfacevisoffset(n)  SW_surface(n).visOffset
#define SW_surfacergba(n)       SW_surface(n).rgba
#define SW_surfaceblendmode(n)  SW_surface(n).blendMode

#define SW_middlesurface        SW_surface(SS_MIDDLE)
#define SW_middleflags          SW_surfaceflags(SS_MIDDLE)
#define SW_middleinflags        SW_surfaceinflags(SS_MIDDLE)
#define SW_middlematerial       SW_surfacematerial(SS_MIDDLE)
#define SW_middletangent        SW_surfacetangent(SS_MIDDLE)
#define SW_middlebitangent      SW_surfacebitangent(SS_MIDDLE)
#define SW_middlenormal         SW_surfacenormal(SS_MIDDLE)
#define SW_middletexmove        SW_surfacetexmove(SS_MIDDLE)
#define SW_middleoffset         SW_surfaceoffset(SS_MIDDLE)
#define SW_middlevisoffset      SW_surfacevisoffset(SS_MIDDLE)
#define SW_middlergba           SW_surfacergba(SS_MIDDLE)
#define SW_middleblendmode      SW_surfaceblendmode(SS_MIDDLE)

#define SW_topsurface           SW_surface(SS_TOP)
#define SW_topflags             SW_surfaceflags(SS_TOP)
#define SW_topinflags           SW_surfaceinflags(SS_TOP)
#define SW_topmaterial          SW_surfacematerial(SS_TOP)
#define SW_toptangent           SW_surfacetangent(SS_TOP)
#define SW_topbitangent         SW_surfacebitangent(SS_TOP)
#define SW_topnormal            SW_surfacenormal(SS_TOP)
#define SW_toptexmove           SW_surfacetexmove(SS_TOP)
#define SW_topoffset            SW_surfaceoffset(SS_TOP)
#define SW_topvisoffset         SW_surfacevisoffset(SS_TOP)
#define SW_toprgba              SW_surfacergba(SS_TOP)

#define SW_bottomsurface        SW_surface(SS_BOTTOM)
#define SW_bottomflags          SW_surfaceflags(SS_BOTTOM)
#define SW_bottominflags        SW_surfaceinflags(SS_BOTTOM)
#define SW_bottommaterial       SW_surfacematerial(SS_BOTTOM)
#define SW_bottomtangent        SW_surfacetangent(SS_BOTTOM)
#define SW_bottombitangent      SW_surfacebitangent(SS_BOTTOM)
#define SW_bottomnormal         SW_surfacenormal(SS_BOTTOM)
#define SW_bottomtexmove        SW_surfacetexmove(SS_BOTTOM)
#define SW_bottomoffset         SW_surfaceoffset(SS_BOTTOM)
#define SW_bottomvisoffset      SW_surfacevisoffset(SS_BOTTOM)
#define SW_bottomrgba           SW_surfacergba(SS_BOTTOM)
end

internal
#define FRONT                   0
#define BACK                    1

typedef struct msidedef_s {
    // Sidedef index. Always valid after loading & pruning.
    int         index;
    int         refCount;
} msidedef_t;
end

struct SideDef
    -       Surface[3]  sections
    UINT    uint        hedgeCount
    PTR     hedge_s**   hedges      // [hedgeCount] size, segs arranged left>right
    PTR     linedef_s*  line
    PTR     sector_s*   sector
    SHORT   short       flags
    -       origin_t    origin
    -       msidedef_t  buildData

# The following is used with FakeRadio.
    -       int         fakeRadioUpdateCount // frame number of last update
    -       shadowcorner_t[2] topCorners
    -       shadowcorner_t[2] bottomCorners
    -       shadowcorner_t[2] sideCorners
    -       edgespan_t[2] spans // [left, right]
end

internal
// Helper macros for accessing linedef data elements.
#define L_v(n)                  v[(n)? 1:0]
#define L_vpos(n)               v[(n)]->V_pos

#define L_v1                    L_v(0)
#define L_v1pos                 L_v(0)->V_pos

#define L_v2                    L_v(1)
#define L_v2pos                 L_v(1)->V_pos

#define L_vo(n)                 vo[(n)? 1:0]
#define L_vo1                   L_vo(0)
#define L_vo2                   L_vo(1)

#define L_side(n)               sideDefs[(n)? 1:0]
#define L_frontside             L_side(FRONT)
#define L_backside              L_side(BACK)
#define L_sector(n)             sideDefs[(n)? 1:0]->sector
#define L_frontsector           L_sector(FRONT)
#define L_backsector            L_sector(BACK)

// Is this line self-referencing (front sec == back sec)? 
#define LINE_SELFREF(l)         ((l)->L_frontside && (l)->L_backside && \
                                 (l)->L_frontsector == (l)->L_backsector)

// Internal flags:
#define LF_POLYOBJ              0x1 // Line is part of a polyobject.
end

internal
#define MLF_TWOSIDED            0x1 // Line is marked two-sided.
#define MLF_ZEROLENGTH          0x2 // Zero length (line should be totally ignored).
#define MLF_SELFREF             0x4 // Sector is the same on both sides.
#define MLF_POLYOBJ             0x8 // Line is part of a polyobj.

typedef struct mlinedef_s {
    // Linedef index. Always valid after loading & pruning of zero
    // length lines has occurred.
    int         index;
    int         mlFlags; // MLF_* flags.
    
    // One-sided linedef used for a special effect (windows).
    // The value refers to the opposite sector on the back side.
    struct sector_s *windowEffect;

    // Normally NULL, except when this linedef directly overlaps an earlier
    // one (a rarely-used trick to create higher mid-masked textures).
    // No segs should be created for these overlapping linedefs.
    struct linedef_s *overlap;
} mlinedef_t;
end

public
#define DMT_LINEDEF_SEC    DDVT_PTR
#define DMT_LINEDEF_AABOX  DDVT_FLOAT
end

struct LineDef
    PTR     vertex_s*[2] v
    -       lineowner_s*[2] vo      // Links to vertex line owner nodes [left, right]
    PTR     sidedef_s*[2] sideDefs
    INT     int         flags       // Public DDLF_* flags.
    -       byte        inFlags	    // Internal LF_* flags
    INT     slopetype_t slopeType
    INT     int         validCount
    -       binangle_t  angle       // Calculated from front side's normal
    FLOAT   float       dX
    FLOAT   float       dY
    -       float       length      // Accurate length
    -       AABox       aaBox
    -       boolean[DDMAXPLAYERS] mapped // Whether the line has been mapped by each player yet.
    -       mlinedef_t  buildData
    -       ushort[2]   shadowVisFrame // Framecount of last time shadows were drawn for this line, for each side [right, left].
end

internal
#define RIGHT                   0
#define LEFT                    1

/**
 * An infinite line of the form point + direction vectors. 
 */
typedef struct partition_s {
    float x, y;
    float dX, dY;
} partition_t;

typedef struct mbspnode_s {
    // Node index. Only valid once the nodes have been hardened.
    int index;
} mbspnode_t;
end

public
#define DMT_BSPNODE_AABOX  DDVT_FLOAT
end

struct BspNode
    -       partition_t partition
    -       AABoxf[2]   aaBox    // Bounding box for each child.
    UINT    uint[2]     children // If NF_LEAF it's a BspLeaf.
    -       mbspnode_t  buildData
end
