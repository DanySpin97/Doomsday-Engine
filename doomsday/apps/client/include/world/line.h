/** @file line.h  Map line.
 * @ingroup world
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2016 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef DENG_WORLD_LINE_H
#define DENG_WORLD_LINE_H

#include <functional>
#include <QFlags>
#include <de/binangle.h>
#include <de/Error>
#include <de/Observers>
#include <de/String>
#include <de/Vector>
#include "world/mapelement.h"
#include "HEdge"
#include "Polyobj"
#include "Vertex"

class LineOwner;
class Plane;
class Sector;
class Surface;

#ifdef __CLIENT__
struct edgespan_t;
struct shadowcorner_t;
#endif

/**
 * World map line.
 *
 * @attention This component has a notably different design and slightly different
 * purpose when compared to a Linedef in the id Tech 1 map format. The definitions
 * of which are not always interchangeable.
 *
 * DENG lines always have two logical sides, however they may not have a sector
 * attributed to either or both sides.
 *
 * @note Lines are @em not considered to define the geometry of a map. Instead
 * a line should be thought of as a finite line segment in the plane, according
 * to the standard definition of a line as used with an arrangement of lines in
 * computational geometry.
 *
 * @see http://en.wikipedia.org/wiki/Arrangement_of_lines
 */
class Line : public world::MapElement
{
    DENG2_NO_COPY  (Line)
    DENG2_NO_ASSIGN(Line)

public:
    /// Required sector attribution is missing. @ingroup errors
    DENG2_ERROR(MissingSectorError);

    /// Required polyobj attribution is missing. @ingroup errors
    DENG2_ERROR(MissingPolyobjError);

    /// The given side section identifier is invalid. @ingroup errors
    DENG2_ERROR(InvalidSectionIdError);

    /// Notified whenever the flags change.
    DENG2_DEFINE_AUDIENCE(FlagsChange, void lineFlagsChanged(Line &line, de::dint oldFlags))

    // Logical edge identifiers:
    enum { From, To };

    // Logical face identifiers:
    enum { Front, Back };

    static de::String faceIdAsText(de::dint faceId);

    /**
     * Logical side of which there are always two (a front and a back).
     */
    class Side : public world::MapElement
    {
        DENG2_NO_COPY  (Side)
        DENG2_NO_ASSIGN(Side)

    public:
        // Section identifiers:
        enum {
            Middle,
            Bottom,
            Top
        };

        static de::String sectionIdAsText(de::dint sectionId);

        /**
         * Flags used as Section identifiers:
         */
        enum SectionFlag {
            MiddleFlag  = 0x1,
            BottomFlag  = 0x2,
            TopFlag     = 0x4,

            AllSectionFlags = MiddleFlag | BottomFlag | TopFlag
        };
        Q_DECLARE_FLAGS(SectionFlags, SectionFlag)

        /**
         * Side geometry segment on the XY plane.
         */
        class Segment : public world::MapElement
        {
            DENG2_NO_COPY  (Segment)
            DENG2_NO_ASSIGN(Segment)

        public:
            /**
             * Construct a new line side segment.
             *
             * @param lineSide  Side parent which will own the segment.
             * @param hedge     Half-edge from the map geometry mesh which the
             *                  new segment visualizes.
             */
            Segment(Side &lineSide, de::HEdge &hedge);

            de::String describe() const override;

            /**
             * Returns the line side owner of the segment.
             */
            Side       &lineSide();
            Side const &lineSide() const;

            /**
             * Convenient accessor method for returning the line of the owning
             * line side.
             *
             * @see lineSide()
             */
            inline Line       &line()       { return lineSide().line(); }
            inline Line const &line() const { return lineSide().line(); }

            /**
             * Returns the half-edge for the segment.
             */
            de::HEdge &hedge() const;

#ifdef __CLIENT__

            /**
             * Returns the distance along the attributed map line at which the
             * from vertex vertex occurs.
             *
             * @see lineSide()
             */
            de::ddouble lineSideOffset() const;

            /// @todo Refactor away.
            void setLineSideOffset(de::ddouble newOffset);

            /**
             * Returns the accurate length of the segment, from the 'from'
             * vertex to the 'to' vertex in map coordinate space units.
             */
            de::ddouble length() const;

            /// @todo Refactor away.
            void setLength(de::ddouble newLength);

            /**
             * Returns @c true iff the segment is marked as "front facing".
             */
            bool isFrontFacing() const;

            /**
             * Mark the current segment as "front facing".
             */
            void setFrontFacing(bool yes = true);

#endif  // __CLIENT__

        private:
            DENG2_PRIVATE(d)
        };

    public:
        /**
         * Construct a new line side.
         *
         * @param line    Line parent which will own the side.
         * @param sector  Sector on "this" side of the line. Can be @c nullptr.
         *                Note that once attributed, the sector cannot normally
         *                be changed.
         */
        Side(Line &line, Sector *sector = nullptr);

        de::String describe() const override;

        /**
         * Returns the Line owner of the side.
         */
        Line       &line();
        Line const &line() const;

        /**
         * Returns the logical identifier for the side (Front or Back).
         */
        de::dint sideId() const;

        /**
         * Returns @c true iff this is the front side of the owning line.
         *
         * @see lineSideId()
         */
        inline bool isFront() const { return sideId() == Front; }

        /**
         * Returns @c true iff this is the back side of the owning line.
         *
         * @see lineSideId(), isFront()
         */
        inline bool isBack() const { return !isFront(); }

        /**
         * Returns the relative back Side from the Line owner.
         *
         * @see lineSideId(), line(), Line::side(),
         */
        inline Side       &back()       { return line().side(sideId() ^ 1); }
        inline Side const &back() const { return line().side(sideId() ^ 1); }

        /**
         * Determines whether "this" side of the respective line should be
         * considered as though there were no back sector. Primarily for use
         * with id Tech 1 format maps (which, supports partial suppression of
         * the back sector, for use with special case drawing and playsim
         * functionality).
         */
        bool considerOneSided() const;

        /**
         * Returns the specified relative vertex from the Line owner.
         *
         * @see lineSideId(), line(), Line::vertex(),
         */
        inline Vertex &vertex(de::dint to) const { return line().vertex(sideId() ^ to); }

        /**
         * Returns the relative From Vertex for the side, from the Line owner.
         *
         * @see vertex(), to()
         */
        inline Vertex &from() const { return vertex(From); }

        /**
         * Returns the relative To Vertex for the side, from the Line owner.
         *
         * @see vertex(), from()
         */
        inline Vertex &to  () const { return vertex(To); }

        /**
         * Returns @c true iff Sections are defined for the side.
         *
         * @see addSections()
         */
        bool hasSections() const;

        /**
         * Add default sections to the side if they aren't already defined.
         *
         * @see hasSections()
         */
        void addSections();

        /**
         * Returns the specified surface of the side.
         *
         * @param sectionId  Identifier of the surface to return.
         */
        Surface       &surface(de::dint sectionId);
        Surface const &surface(de::dint sectionId) const;

        /**
         * Returns the middle surface of the side.
         *
         * @see surface()
         */
        inline Surface       &middle()       { return surface(Middle); }
        inline Surface const &middle() const { return surface(Middle); }

        /**
         * Returns the bottom surface of the side.
         *
         * @see surface()
         */
        inline Surface       &bottom()       { return surface(Bottom); }
        inline Surface const &bottom() const { return surface(Bottom); }

        /**
         * Returns the top surface of the side.
         *
         * @see surface()
         */
        inline Surface       &top()       { return surface(Top); }
        inline Surface const &top() const { return surface(Top); }

        /**
         * Iterate through the Surfaces of the side. If no sections are present
         * then nothing happens.
         *
         * @param func  Callback to make for each Surface.
         */
        de::LoopResult forAllSurfaces(std::function<de::LoopResult(Surface &)> func) const;

        /**
         * Returns the specified sound emitter of the side.
         *
         * @param sectionId  Identifier of the sound emitter to return.
         *
         * @see Section::soundEmitter()
         */
        SoundEmitter       &soundEmitter(de::dint sectionId);
        SoundEmitter const &soundEmitter(de::dint sectionId) const;

        /**
         * Returns the middle sound emitter of the side.
         *
         * @see Section::soundEmitter()
         */
        inline SoundEmitter       &middleSoundEmitter()       { return soundEmitter(Middle); }
        inline SoundEmitter const &middleSoundEmitter() const { return soundEmitter(Middle); }

        /**
         * Returns the bottom sound emitter (tee-hee) for the side.
         *
         * @see Section::soundEmitter()
         */
        inline SoundEmitter       &bottomSoundEmitter()       { return soundEmitter(Bottom); }
        inline SoundEmitter const &bottomSoundEmitter() const { return soundEmitter(Bottom); }

        /**
         * Returns the top sound emitter for the side.
         *
         * @see Section::soundEmitter()
         */
        inline SoundEmitter       &topSoundEmitter()       { return soundEmitter(Top); }
        inline SoundEmitter const &topSoundEmitter() const { return soundEmitter(Top); }

        /**
         * Returns @c true iff a Sector is attributed to the side.
         *
         * @see considerOneSided()
         */
        bool hasSector() const;

        /**
         * Returns the Sector attributed to the side.
         *
         * @see hasSector()
         */
        Sector &sector() const;

        /**
         * Returns a pointer to the Sector attributed to the side; otherwise @c nullptr.
         *
         * @see hasSector()
         */
        inline Sector *sectorPtr() const { return hasSector()? &sector() : nullptr; }

        /**
         * Clears (destroys) all segments for the side.
         */
        void clearSegments();

        /**
         * Create a Segment for the specified half-edge. If an existing Segment
         * is present for the half-edge it will be returned instead (nothing will
         * happen).
         *
         * It is assumed that the half-edge is collinear with and represents a
         * subsection of the line geometry. It is also assumed that the half-edge
         * faces the same direction as this side. It is the caller's responsibility
         * to ensure these two requirements are met otherwise the segment list
         * will be ordered illogically.
         *
         * @param hedge  Half-edge to create a new Segment for.
         *
         * @return  Pointer to the (possibly newly constructed) Segment.
         */
        Segment *addSegment(de::HEdge &hedge);

        /**
         * Convenient method of returning the half-edge of the left-most segment
         * on this side of the line; otherwise @c nullptr (no segments exist).
         */
        de::HEdge *leftHEdge() const;

        /**
         * Convenient method of returning the half-edge of the right-most segment
         * on this side of the line; otherwise @c nullptr (no segments exist).
         */
        de::HEdge *rightHEdge() const;

        /**
         * Update the tangent space normals of the side's surfaces according to the
         * points defined by the Line's vertices. If no Sections are defined this is
         * a no-op.
         */
        void updateSurfaceNormals();

        /**
         * Returns the @ref sdefFlags for the side.
         */
        de::dint flags() const;

        /**
         * Change the side's flags.
         *
         * @param flagsToChange  Flags to change the value of.
         * @param operation      Logical operation to perform on the flags.
         */
        void setFlags(de::dint flagsToChange, de::FlagOp operation = de::SetFlags);

        /**
         * Returns @c true iff the side is flagged @a flagsToTest.
         */
        inline bool isFlagged(de::dint flagsToTest) const { return (flags() & flagsToTest) != 0; }

        void chooseSurfaceColors(de::dint sectionId, de::Vector3f const **topColor,
                                 de::Vector3f const **bottomColor) const;

        /**
         * Returns the frame number of the last time shadows were drawn for the side.
         */
        de::dint shadowVisCount() const;

        /**
         * Change the frame number of the last time shadows were drawn for the side.
         *
         * @param newCount  New shadow vis count.
         */
        void setShadowVisCount(de::dint newCount);

#ifdef __CLIENT__

        /**
         * Do as in the original DOOM if the texture has not been defined -
         * extend the floor/ceiling to fill the space (unless it is skymasked).
         */
        void fixMissingMaterials();

        /**
         * To be called to update the shadow properties for the line side.
         *
         * @todo Handle this internally -ds.
         */
        void updateRadioForFrame(de::dint frameNumber);

        /**
         * Provides access to the FakeRadio shadowcorner_t data.
         */
        shadowcorner_t const &radioCornerTop   (bool right) const;
        shadowcorner_t const &radioCornerBottom(bool right) const;
        shadowcorner_t const &radioCornerSide  (bool right) const;

        /**
         * Provides access to the FakeRadio edgespan_t data.
         */
        edgespan_t const &radioEdgeSpan(bool top) const;

#endif  // __CLIENT__

    protected:
        de::dint property(de::DmuArgs &args) const;
        de::dint setProperty(de::DmuArgs const &args);

    private:
        DENG2_PRIVATE(d)
    };

public: /// @todo make private:
    /// Links to vertex line owner nodes:
    LineOwner *_vo1 = nullptr;
    LineOwner *_vo2 = nullptr;

    /// Sector of the map for which this line acts as a "One-way window".
    /// @todo Now unnecessary, refactor away -ds
    Sector *_bspWindowSector = nullptr;

public:
    Line(Vertex &from, Vertex &to,
         de::dint flags      = 0,
         Sector *frontSector = nullptr,
         Sector *backSector  = nullptr);

    de::String describe() const override;

    /**
     * Returns the specified logical side of the line.
     *
     * @param back  If not @c 0 return the Back side; otherwise the Front side.
     */
    Side       &side(de::dint back);
    Side const &side(de::dint back) const;

    /**
     * Returns the logical Front side of the line.
     */
    inline Side       &front()       { return side(Front); }
    inline Side const &front() const { return side(Front); }

    /**
     * Returns the logical Back side of the line.
     */
    inline Side       &back()        { return side(Back); }
    inline Side const &back() const  { return side(Back); }

    /**
     * Iterate through the Sides of the line.
     *
     * @param func  Callback to make for each Side (front then back).
     */
    de::LoopResult forAllSides(std::function<de::LoopResult (Side &)> func) const;

    /**
     * Returns @c true iff Side::Sections are defined for the specified side
     * of the line.
     *
     * @param back  If not @c 0 test the Back side; otherwise the Front side.
     */
    inline bool hasSections(de::dint back) const { return side(back).hasSections(); }

    /**
     * Returns @c true iff Side::Sections are defined for the Front side of the line.
     */
    inline bool hasFrontSections() const { return hasSections(Front); }

    /**
     * Returns @c true iff Side::Sections are defined for the Back side of the line.
     */
    inline bool hasBackSections() const  { return hasSections(Back); }

    /**
     * Returns @c true iff a sector is attributed to the specified side of the line.
     *
     * @param back  If not @c 0 test the Back side; otherwise the Front side.
     */
    inline bool hasSector(de::dint back) const { return side(back).hasSector(); }

    /**
     * Returns @c true iff a sector is attributed to the Front side of the line.
     */
    inline bool hasFrontSector() const { return hasSector(Front); }

    /**
     * Returns @c true iff a sector is attributed to the Back side of the line.
     */
    inline bool hasBackSector() const  { return hasSector(Back); }

    /**
     * Convenient accessor method for returning the sector attributed to the
     * specified side of the line.
     *
     * @param back  If not @c 0 return the sector for the Back side; otherwise
     *              the sector of the Front side.
     */
    inline Sector &sector(de::dint back) const { return side(back).sector(); }

    /**
     * Convenient accessor method for returning a pointer to the sector attributed
     * to the specified side of the line.
     *
     * @param back  If not @c 0 return the sector for the Back side; otherwise
     *              the sector of the Front side.
     */
    inline Sector *sectorPtr(de::dint back) const { return side(back).sectorPtr(); }

    /**
     * Returns the sector attributed to the Front side of the line.
     */
    inline Sector &frontSector() const { return sector(Front); }

    /**
     * Returns the sector attributed to the Back side of the line.
     */
    inline Sector &backSector() const  { return sector(Back); }

    /**
     * Convenient accessor method for returning a pointer to the sector attributed
     * to the front side of the line.
     */
    inline Sector *frontSectorPtr() const { return sectorPtr(Front); }

    /**
     * Convenient accessor method for returning a pointer to the sector attributed
     * to the back side of the line.
     */
    inline Sector *backSectorPtr() const  { return sectorPtr(Back); }

    /**
     * Returns @c true iff the line is considered @em self-referencing. In this context,
     * self-referencing (a term whose origins stem from the DOOM modding community) means
     * a two-sided line (which is to say that a Sector is attributed to both logical sides
     * of the line) where the attributed sectors for each logical side are the same.
     */
    inline bool isSelfReferencing() const {
        return hasFrontSector() && frontSectorPtr() == backSectorPtr();
    }

    /**
     * Returns the specified edge vertex of the line.
     *
     * @param to  If not @c 0 return the To vertex; otherwise the From vertex.
     */
    Vertex &vertex(de::dint to) const;

    /**
     * Iterate through the edge Vertexs for the line.
     *
     * @param func  Callback to make for each Vertex.
     */
    de::LoopResult forAllVertexs(std::function<de::LoopResult(Vertex &)> func) const;

    /**
     * Convenient accessor method for returning the origin of the specified edge vertex
     * for the line.
     *
     * @see vertex()
     */
    inline de::Vector2d const &vertexOrigin(de::dint to) const {
        return vertex(to).origin();
    }

    /**
     * Returns the From/Start vertex for the line.
     */
    inline Vertex &from() const { return vertex(From); }

    /**
     * Returns the To/End vertex for the line.
     */
    inline Vertex &to() const   { return vertex(To); }

    /**
     * Convenient accessor method for returning the origin of the From/Start vertex for
     * the line.
     *
     * @see from()
     */
    inline de::Vector2d const &fromOrigin() const { return from().origin(); }

    /**
     * Convenient accessor method for returning the origin of the To/End vertex for the line.
     *
     * @see to()
     */
    inline de::Vector2d const &toOrigin() const   { return to().origin(); }

    /**
     * Returns the point on the line which lies at the exact center of the two vertexes.
     */
    inline de::Vector2d center() const { return fromOrigin() + direction() / 2; }

    /**
     * Returns the binary angle of the line (which, is derived from the direction vector).
     *
     * @see direction()
     */
    binangle_t angle() const;

    /**
     * Returns a direction vector for the line from Start to End vertex.
     */
    de::Vector2d const &direction() const;

    /**
     * Returns the logical @em slopetype for the line (which, is determined according to
     * the global direction of the line).
     *
     * @see direction()
     * @see M_SlopeType()
     */
    slopetype_t slopeType() const;

    /**
     * Update the line's logical slopetype and direction according to the points defined by
     * the origins of it's vertexes.
     */
    void updateSlopeType();

    /**
     * Returns the accurate length of the line from Start to End vertex.
     */
    de::ddouble length() const;

    /**
     * Returns @c true iff the line has a length equivalent to zero.
     */
    inline bool hasZeroLength() const { return de::abs(length()) < 1.0 / 128.0; }

    /**
     * Returns the axis-aligned bounding box which encompases both vertex origin points,
     * in map coordinate space units.
     */
    AABoxd const &aaBox() const;

    /**
     * Update the line's map space axis-aligned bounding box to encompass the points defined
     * by it's vertexes.
     */
    void updateAABox();

    /**
     * On which side of the line does the specified box lie?
     *
     * @param box  Bounding box to test.
     *
     * @return One of the following:
     * - Negative: @a box is entirely on the left side.
     * - Zero: @a box intersects the line.
     * - Positive: @a box is entirely on the right side.
     */
    de::dint boxOnSide(AABoxd const &box) const;

    /**
     * On which side of the line does the specified box lie? The test is carried out using
     * fixed-point math for behavior compatible with vanilla DOOM. Note that this means
     * there is a maximum size for both the bounding box and the line: neither can exceed
     * the fixed-point 16.16 range (about 65k units).
     *
     * @param box  Bounding box to test.
     *
     * @return One of the following:
     * - Negative: @a box is entirely on the left side.
     * - Zero: @a box intersects the line.
     * - Positive: @a box is entirely on the right side.
     */
    de::dint boxOnSide_FixedPrecision(AABoxd const &box) const;

    /**
     * @param offset  Returns the position of the nearest point along the line [0..1].
     */
    de::ddouble pointDistance(de::Vector2d const &point, de::ddouble *offset = nullptr) const;

    /**
     * Where does the given @a point lie relative to the line? Note that the line is considered
     * to extend to infinity for this test.
     *
     * @param point  The point to test.
     *
     * @return @c <0 Point is to the left of the line.
     *         @c =0 Point lies directly on/incident with the line.
     *         @c >0 Point is to the right of the line.
     */
    de::ddouble pointOnSide(de::Vector2d const &point) const;

    /**
     * Returns @c true iff the line defines a section of some Polyobj.
     */
    bool definesPolyobj() const;

    /**
     * Returns the Polyobj for which the line is a defining section.
     *
     * @see definesPolyobj()
     */
    Polyobj &polyobj() const;

    /**
     * Change the polyobj attributed to the line.
     *
     * @param newPolyobj  New polyobj to attribute the line to. Can be @c nullptr, to clear
     *                    the attribution. (Note that the polyobj may also represent this
     *                    relationship, so the relevant method(s) of Polyobj will also need
     *                    to be called to complete the job of clearing this relationship.)
     */
    void setPolyobj(Polyobj *newPolyobj);

    /**
     * Returns @c true iff the line resulted in the creation of a BSP window effect when
     * partitioning the map.
     *
     * @todo Refactor away. The prescence of a BSP window effect can now be trivially determined
     *       through inspection of the tree elements.
     */
    bool isBspWindow() const;

    /**
     * Returns the public DDLF_* flags for the line.
     */
    de::dint flags() const;

    /**
     * Change the line's flags. The FlagsChange audience is notified whenever the flags
     * are changed.
     *
     * @param flagsToChange  Flags to change the value of.
     * @param operation      Logical operation to perform on the flags.
     */
    void setFlags(de::dint flagsToChange, de::FlagOp operation = de::SetFlags);

    /**
     * Returns @c true iff the line is flagged @a flagsToTest.
     */
    inline bool isFlagged(de::dint flagsToTest) const {
        return (flags() & flagsToTest) != 0;
    }

    /**
     * Returns @c true if the line is marked as @em mapped for @a playerNum.
     */
    bool isMappedByPlayer(de::dint playerNum) const;

    /**
     * Change the @em mapped by player state of the line.
     */
    void markMappedByPlayer(de::dint playerNum, bool yes = true);

    /**
     * Returns the @em validCount of the line. Used by some legacy iteration algorithms
     * for marking lines as processed/visited.
     *
     * @todo Refactor away.
     */
    de::dint validCount() const;

    /// @todo Refactor away.
    void setValidCount(de::dint newValidCount);

    /**
     * Replace the specified edge vertex of the line.
     *
     * @attention Should only be called in map edit mode.
     *
     * @param to         If not @c 0 replace the To vertex; otherwise the From vertex.
     * @param newVertex  The replacement vertex.
     */
    void replaceVertex(de::dint to, Vertex &newVertex);

    inline void replaceFrom(Vertex &newVertex) { replaceVertex(From, newVertex); }
    inline void replaceTo  (Vertex &newVertex) { replaceVertex(To  , newVertex); }

#ifdef __CLIENT__
    /**
     * Returns @c true if the line qualifies for FakeRadio shadow casting (on planes).
     */
    bool castsShadow() const;
#endif

protected:
    de::dint property(de::DmuArgs &args) const;
    de::dint setProperty(de::DmuArgs const &args);

public:
    /**
     * Returns a pointer to the line owner node for the specified edge vertex of the line.
     *
     * @param to  If not @c 0 return the owner for the To vertex; otherwise the From vertex.
     *
     * @deprecated Will be replaced with half-edge ring iterator/rover. -ds
     */
    LineOwner *vertexOwner(de::dint to) const;

    /**
     * Returns a pointer to the line owner for the specified edge @a vertex of the line.
     * If the vertex is not an edge vertex for the line then @c nullptr will be returned.
     */
    inline LineOwner *vertexOwner(Vertex const &vertex) const {
        if(&vertex == &from()) return v1Owner();
        if(&vertex == &to())   return v2Owner();
        return nullptr;
    }

    /**
     * Returns a pointer to the line owner node for the From/Start vertex of the line.
     *
     * @deprecated Will be replaced with half-edge ring iterator/rover. -ds
     */
    inline LineOwner *v1Owner() const { return vertexOwner(From); }

    /**
     * Returns a pointer to the line owner node for the To/End vertex of the line.
     *
     * @deprecated Will be replaced with half-edge ring iterator/rover. -ds
     */
    inline LineOwner *v2Owner() const { return vertexOwner(To); }

public:
    /**
     * Register the console commands and/or variables of this module.
     */
    static void consoleRegister();

private:
    DENG2_PRIVATE(d)
};

typedef Line::Side LineSide;
typedef Line::Side::Segment LineSideSegment;

Q_DECLARE_OPERATORS_FOR_FLAGS(Line::Side::SectionFlags)

#endif  // DENG_WORLD_LINE_H
