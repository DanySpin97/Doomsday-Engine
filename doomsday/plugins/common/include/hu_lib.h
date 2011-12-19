/**\file hu_lib.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2005-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2011 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#ifndef LIBCOMMON_GUI_LIBRARY_H
#define LIBCOMMON_GUI_LIBRARY_H

#if __JDOOM__
#  include "jdoom.h"
#elif __JDOOM64__
#  include "jdoom64.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#endif

#include "hu_stuff.h"

struct mn_object_s;
struct mn_page_s;

typedef enum menucommand_e {
    MCMD_OPEN, // Open the menu.
    MCMD_CLOSE, // Close the menu.
    MCMD_CLOSEFAST, // Instantly close the menu.
    MCMD_NAV_OUT, // Navigate "out" of the current menu/widget (up a level).
    MCMD_NAV_LEFT,
    MCMD_NAV_RIGHT,
    MCMD_NAV_DOWN,
    MCMD_NAV_UP,
    MCMD_NAV_PAGEDOWN,
    MCMD_NAV_PAGEUP,
    MCMD_SELECT, // Execute whatever action is attaced to the current item.
    MCMD_DELETE
} menucommand_e;

// Menu object types.
typedef enum {
    MN_NONE,
    MN_TEXT,
    MN_BUTTON,
    MN_EDIT,
    MN_LIST,
    MN_SLIDER,
    MN_COLORBOX,
    MN_BINDINGS,
    MN_MOBJPREVIEW
} mn_obtype_e;

/**
 * @defgroup menuObjectFlags Menu Object Flags
 */
/*@{*/
#define MNF_HIDDEN              0x1
#define MNF_DISABLED            0x2 // Can't be interacted with.
//#define MNF_PAUSED              0x4 // Ticker not called.
#define MNF_CLICKED             0x8
#define MNF_ACTIVE              0x10 // Object active.
#define MNF_FOCUS               0x20 // Has focus.
#define MNF_NO_FOCUS            0x40 // Can't receive focus.
#define MNF_DEFAULT             0x80 // Has focus by default.
//#define MNF_LEFT_ALIGN          0x100
//#define MNF_FADE_AWAY           0x200 // Fade UI away while the control is active.
//#define MNF_NEVER_FADE          0x400
#define MNF_NO_ALTTEXT          0x800 // Don't use alt text instead of lump (M_NMARE)

/// \todo We need a new dynamic id allocating mechanism.
#define MNF_ID7                 0x1000000
#define MNF_ID6                 0x2000000
#define MNF_ID5                 0x4000000
#define MNF_ID4                 0x8000000
#define MNF_ID3                 0x10000000
#define MNF_ID2                 0x20000000
#define MNF_ID1                 0x40000000
#define MNF_ID0                 0x80000000
/*@}*/

typedef enum {
    FO_CLEAR,
    FO_SET,
    FO_TOGGLE
} flagop_t;

/**
 * Logical Menu (object) Action identifiers. Associated with/to events which
 * produce/result-in callbacks made either automatically by this subsystem,
 * or "actioned" through the type-specific event/command responders of the
 * various widgets, according to their own widget-specific logic.
 */
typedef enum {
    MNA_NONE = -1,
    MNACTION_FIRST = 0,
    MNA_MODIFIED = MNACTION_FIRST, /// Object's internal "modified" status changed.
    MNA_ACTIVEOUT,              /// Deactivated i.e., no longer active.
    MNA_ACTIVE,                 /// Becomes "active".
    MNA_CLOSE,                  /// Normally means changed-state to be discarded.
    MNA_FOCUSOUT,               /// Loses selection "focus".
    MNA_FOCUS,                  /// Gains selection "focus".
    MNACTION_LAST = MNA_FOCUS
} mn_actionid_t;

/// Total number of known Menu Actions.
#define MNACTION_COUNT          (MNACTION_LAST + 1 - MNACTION_FIRST)

/// Non-zero if the value @a val can be interpreted as a known, valid Menu Action identifier.
#define VALID_MNACTION(val)     ((id) >= MNACTION_FIRST && (id) <= MNACTION_LAST)

/**
 * Menu Action Info (Record). Holds information about an "actionable" menu
 * event, such as an object being activated or upon receiving focus.
 */
typedef struct {
    /// Callback to be made when this action is executed. Can be @c NULL in
    /// which case attempts to action this will be NOPs.
    ///
    /// @param obj  Object being referenced for this callback.
    /// @param action  Identifier of the Menu Action to be processed.
    /// @param paramaters  Passed to the callback from event which actioned this.
    /// @return  Callback return value. Callback should return zero if the action
    ///     was recognised and processed, regardless of outcome.
    int (*callback) (struct mn_object_s* obj, mn_actionid_t action, void* paramaters);
} mn_actioninfo_t;

/**
 * MNObject. Abstract base from which all menu page objects must be derived.
 */
typedef struct mn_object_s {
    /// Type of the object.
    mn_obtype_e _type;

    /// Object group identifier.
    int _group;

    /// @see menuObjectFlags.
    int _flags;

    /// DDKEY shortcut used to switch focus to this object directly.
    /// @c 0= no shortcut defined.
    int _shortcut;

    /// Index of the predefined page font to use when drawing this.
    int _pageFontIdx;

    /// Index of the predefined page color to use when drawing this.
    int _pageColorIdx;

    /// Calculate geometry for this when visible on the specified page.
    void (*updateGeometry) (struct mn_object_s* obj, struct mn_page_s* page);

    /// Draw this at the specified offset within the owning view-space.
    /// Can be @c NULL in which case this will never be drawn.
    void (*drawer) (struct mn_object_s* obj, const Point2Raw* origin);

    /// Info about "actionable event" callbacks.
    mn_actioninfo_t actions[MNACTION_COUNT];

    /// Respond to the given (menu) @a command. Can be @c NULL.
    /// @return  @c true if the command is eaten.
    int (*cmdResponder) (struct mn_object_s* obj, menucommand_e command);

    /// Respond to the given (input) event @a ev. Can be @c NULL.
    /// @return  @c true if the event is eaten.
    int (*responder) (struct mn_object_s* obj, event_t* ev);

    /// Respond to the given (input) event @a ev. Can be @c NULL.
    /// @return  @c true if the event is eaten.
    int (*privilegedResponder) (struct mn_object_s* obj, event_t* ev);

    void* _typedata; // Type-specific extra data.

    // Extra property values.
    void* data1;
    int data2;

    /// Current geometry.
    RectRaw _geometry;
} mn_object_t;

mn_obtype_e MNObject_Type(const mn_object_t* obj);

int MNObject_Flags(const mn_object_t* obj);

const RectRaw* MNObject_Geometry(const mn_object_t* obj);

/// @return  Flags value post operation for caller convenience.
int MNObject_SetFlags(mn_object_t* obj, flagop_t op, int flags);

int MNObject_Shortcut(mn_object_t* obj);

void MNObject_SetShortcut(mn_object_t* obj, int ddkey);

/// @return  Index of the font used from the owning/active page.
int MNObject_Font(mn_object_t* obj);

/// @return  Index of the color used from the owning/active page.
int MNObject_Color(mn_object_t* obj);

boolean MNObject_IsGroupMember(const mn_object_t* obj, int group);

int MNObject_DefaultCommandResponder(mn_object_t* obj, menucommand_e command);

/**
 * Lookup the unique ActionInfo associated with the identifier @a id.
 * @return  Associated info if found else @c NULL.
 */
const mn_actioninfo_t* MNObject_Action(mn_object_t*obj, mn_actionid_t action);

/// @return  @c true if this object has a registered executeable action
/// associated with the unique identifier @a action.
boolean MNObject_HasAction(mn_object_t* obj, mn_actionid_t action);

/**
 * Execute the action associated with @a id
 * @param action  Identifier of the action to be executed (if found).
 * @param paramaters  Passed to the action callback.
 * @return  Return value of the executed action else @c -1 if NOP.
 */
int MNObject_ExecAction(mn_object_t* obj, mn_actionid_t action, void* paramaters);

typedef enum {
    MENU_COLOR1,
    MENU_COLOR2,
    MENU_COLOR3,
    MENU_COLOR4,
    MENU_COLOR5,
    MENU_COLOR6,
    MENU_COLOR7,
    MENU_COLOR8,
    MENU_COLOR9,
    MENU_COLOR10,
    MENU_COLOR_COUNT
} mn_page_colorid_t;

#define VALID_MNPAGE_COLORID(v)      ((v) >= MENU_COLOR1 && (v) < MENU_COLOR_COUNT)

typedef enum {
    MENU_FONT1,
    MENU_FONT2,
    MENU_FONT3,
    MENU_FONT4,
    MENU_FONT5,
    MENU_FONT6,
    MENU_FONT7,
    MENU_FONT8,
    MENU_FONT9,
    MENU_FONT10,
    MENU_FONT_COUNT
} mn_page_fontid_t;

#define VALID_MNPAGE_FONTID(v)      ((v) >= MENU_FONT1 && (v) < MENU_FONT_COUNT)

/*typedef struct mn_page_unscaledstate_s {
    uint numVisObjects;
    int y;
} mn_page_unscaledstate_t;*/

typedef struct mn_page_s {
    mn_object_t* objects; // List of objects.
    Point2Raw origin;
    fontid_t fonts[MENU_FONT_COUNT];
    uint colors[MENU_COLOR_COUNT];
    void (*drawer) (struct mn_page_s* page, const Point2Raw* origin);
    int (*cmdResponder) (struct mn_page_s* page, menucommand_e cmd);
    struct mn_page_s* previous; // Pointer to the previous page, if any.
    void* data;
    // For scrollable multi-pages.
    //uint firstObject, numVisObjects;
    // Scalable pages.
    //mn_page_unscaledstate_t unscaled;
    // Auto-initialized.
    int objectsCount;
    int focus; // Index of the focus object.
} mn_page_t;

void MNPage_Initialize(mn_page_t* page);

/// @return  Currently focused object else @c NULL
mn_object_t* MNPage_FocusObject(mn_page_t* page);

/**
 * Attempt to give focus to the MNObject @a obj which is thought to be on
 * this page. If @a obj is found to present and is not currently in-focus,
 * an out-focus action is first sent to the presently focused object, then
 * this page's focused object is set before finally executing an in-focus
 * action on the new object. If the object is not found on this page then
 * this is a NOP.
 *
 * @param obj  MNObject to be given focus.
 */
void MNPage_SetFocus(mn_page_t* page, mn_object_t* obj);

/**
 * Retrieve an object on this page in the specified object group.
 * @param flags  Flags used to locate the object. All specified flags must
 *      must be set @see mnobjectFlags
 * @return  Found MNObject else @c NULL
 */
mn_object_t* MNPage_FindObject(mn_page_t* page, int group, int flags);

/**
 * Retrieve a predefined color triplet associated with this page by it's
 * logical page color identifier.
 * @param id  Unique identifier of the predefined color being retrieved.
 * @param rgb  Found color values are written here, else set to white.
 */
void MNPage_PredefinedColor(mn_page_t* page, mn_page_colorid_t id, float rgb[3]);

/**
 * Retrieve a predefined Doomsday font-identifier associated with this page
 * by it's logical page font identifier.
 * @param id  Unique identifier of the predefined font being retrieved.
 * @return  Identifier of the found font else @c 0
 */
fontid_t MNPage_PredefinedFont(mn_page_t* page, mn_page_fontid_t id);

/**
 * Text objects.
 */
typedef struct mndata_text_s {
    const char* text;

    /// Patch to be used when drawing this instead of text if Patch Replacement is in use.
    patchid_t* patch;
} mndata_text_t;

void MNText_Drawer(mn_object_t* obj, const Point2Raw* origin);
void MNText_UpdateGeometry(mn_object_t* obj, mn_page_t* page);

/**
 * Buttons.
 */
typedef struct mndata_button_s {
    boolean staydownMode; /// @c true= this is operating in two-state "staydown" mode.
    void* data;
    const char* text;
    /// Patch to be used when drawing this instead of text.
    patchid_t* patch;
    const char* yes, *no;
} mndata_button_t;

void MNButton_Drawer(mn_object_t* obj, const Point2Raw* origin);
int MNButton_CommandResponder(mn_object_t* obj, menucommand_e command);
void MNButton_UpdateGeometry(mn_object_t* obj, mn_page_t* page);

/**
 * Edit field.
 */
#define MNDATA_EDIT_TEXT_MAX_LENGTH             (24)
#if __JDOOM__ || __JDOOM64__
#  define MNDATA_EDIT_TEXT_COLORIDX             (0)
#  define MNDATA_EDIT_OFFSET_X                  (0)
#  define MNDATA_EDIT_OFFSET_Y                  (0)
#  define MNDATA_EDIT_BACKGROUND_OFFSET_X       (-11)
#  define MNDATA_EDIT_BACKGROUND_OFFSET_Y       (-4)
#  define MNDATA_EDIT_BACKGROUND_PATCH_LEFT     ("M_LSLEFT")
#  define MNDATA_EDIT_BACKGROUND_PATCH_RIGHT    ("M_LSRGHT")
#  define MNDATA_EDIT_BACKGROUND_PATCH_MIDDLE   ("M_LSCNTR")
#elif __JHERETIC__ || __JHEXEN__
#  define MNDATA_EDIT_TEXT_COLORIDX             (2)
#  define MNDATA_EDIT_OFFSET_X                  (13)
#  define MNDATA_EDIT_OFFSET_Y                  (5)
#  define MNDATA_EDIT_BACKGROUND_OFFSET_X       (-13)
#  define MNDATA_EDIT_BACKGROUND_OFFSET_Y       (-5)
//#  define MNDATA_EDIT_BACKGROUND_PATCH_LEFT   ("")
//#  define MNDATA_EDIT_BACKGROUND_PATCH_RIGHT  ("")
#  define MNDATA_EDIT_BACKGROUND_PATCH_MIDDLE   ("M_FSLOT")
#endif

typedef struct mndata_edit_s {
    char text[MNDATA_EDIT_TEXT_MAX_LENGTH+1];
    char oldtext[MNDATA_EDIT_TEXT_MAX_LENGTH+1]; // If the current edit is canceled...
    uint maxVisibleChars;
    const char* emptyString; // Drawn when editfield is empty/null.
    void* data1;
    int data2;
} mndata_edit_t;

void MNEdit_Drawer(mn_object_t* obj, const Point2Raw* origin);
int MNEdit_CommandResponder(mn_object_t* obj, menucommand_e command);
int MNEdit_Responder(mn_object_t* obj, event_t* ev);
void MNEdit_UpdateGeometry(mn_object_t* obj, mn_page_t* page);

/**
 * @defgroup mneditSetTextFlags  MNEdit Set Text Flags
 * @{
 */
#define MNEDIT_STF_NO_ACTION            0x1 /// Do not call any linked action function.
#define MNEDIT_STF_REPLACEOLD           0x2 /// Replace the "old" copy (used for canceled edits).
/**@}*/

/// @return  A pointer to an immutable copy of the current contents of the edit field.
const char* MNEdit_Text(mn_object_t* obj);

/**
 * Change the current contents of the edit field.
 * @param flags  @see mneditSetTextFlags
 * @param string  New text string which will replace the existing string.
 */
void MNEdit_SetText(mn_object_t* obj, int flags, const char* string);

/**
 * List selection.
 */
#define MNDATA_LIST_LEADING     .5f /// Inter-item leading factor (does not apply to MNList_InlineDrawer).
#define MNDATA_LIST_NONSELECTION_LIGHT  .7f /// Light value multiplier for non-selected items (does not apply to MNList_InlineDrawer).

#define NUMLISTITEMS(x) (sizeof(x)/sizeof(mndata_listitem_t))

typedef struct {
    const char* text;
    int data;
} mndata_listitem_t;

typedef struct mndata_list_s {
    void* items;
    int count; // Number of items.
    void* data;
    int mask;
    int selection; // Selected item (-1 if none).
    int first; // First visible item.
    int numvis;
} mndata_list_t;

void MNList_Drawer(mn_object_t* obj, const Point2Raw* origin);
void MNList_InlineDrawer(mn_object_t* obj, const Point2Raw* origin);

int MNList_CommandResponder(mn_object_t* obj, menucommand_e command);
int MNList_InlineCommandResponder(mn_object_t* obj, menucommand_e command);

void MNList_UpdateGeometry(mn_object_t* obj, mn_page_t* page);
void MNList_InlineUpdateGeometry(mn_object_t* obj, mn_page_t* page);

/// @return  Index of the currently selected item else -1.
int MNList_Selection(mn_object_t* obj);

/// @return  @c true if the currently selected item is presently visible.
boolean MNList_SelectionIsVisible(mn_object_t* obj);

/// @return  Index of the found item associated with @a dataValue else -1.
int MNList_FindItem(const mn_object_t* obj, int dataValue);

/**
 * @defgroup mnlistSelectItemFlags  MNList Select Item Flags
 * @{
 */
#define MNLIST_SIF_NO_ACTION            0x1 /// Do not call any linked action function.
/**@}*/

/**
 * Change the currently selected item.
 * @param flags  @see mnlistSelectItemFlags
 * @param itemIndex  Index of the new selection.
 * @return  @c true if the selected item changed.
 */
boolean MNList_SelectItem(mn_object_t* obj, int flags, int itemIndex);

/**
 * Change the currently selected item by looking up its data value.
 * @param flags  @see mnlistSelectItemFlags
 * @param dataValue  Value associated to the candidate item being selected.
 * @return  @c true if the selected item changed.
 */
boolean MNList_SelectItemByValue(mn_object_t* obj, int flags, int itemIndex);

/**
 * Color preview box.
 */
#define MNDATA_COLORBOX_WIDTH   4 // Default inner width in fixed 320x200 space.
#define MNDATA_COLORBOX_HEIGHT  4 // Default inner height in fixed 320x200 space.
#define MNDATA_COLORBOX_PADDING_X   3 // Inclusive of the outer border.
#define MNDATA_COLORBOX_PADDING_Y   5 //

typedef struct mndata_colorbox_s {
    /// Inner dimensions in fixed 320x200 space. If @c <= 0 the default
    /// dimensions will be used instead.
    int width, height;
    float r, g, b, a;
    boolean rgbaMode;
    void* data1;
    void* data2;
    void* data3;
    void* data4;
} mndata_colorbox_t;

void MNColorBox_Drawer(mn_object_t* obj, const Point2Raw* origin);
int MNColorBox_CommandResponder(mn_object_t* obj, menucommand_e command);
void MNColorBox_UpdateGeometry(mn_object_t* obj, mn_page_t* page);

/// @return  @c true if this colorbox is operating in RGBA mode.
boolean MNColorBox_RGBAMode(mn_object_t* obj);

/// @return  Current red color component.
float MNColorBox_Redf(const mn_object_t* obj);

/// @return  Current green color component.
float MNColorBox_Greenf(const mn_object_t* obj);

/// @return  Current blue color component.
float MNColorBox_Bluef(const mn_object_t* obj);

/// @return  Current alpha value or @c 1 if this colorbox is not
///     operating in "rgba mode".
float MNColorBox_Alphaf(const mn_object_t* obj);

/**
 * @defgroup mncolorboxSetColorFlags  MNColorBox Set Color Flags.
 * @{
 */
#define MNCOLORBOX_SCF_NO_ACTION        0x1 /// Do not call any linked action function.
/**@}*/

/**
 * Change the current color of the color box.
 * @param flags  @see mncolorboxSetColorFlags
 * @param rgba  New color and alpha. Note: will be NOP if this colorbox
 *      is not operating in "rgba mode".
 * @return  @c true if the current color changed.
 */
boolean MNColorBox_SetColor4fv(mn_object_t* obj, int flags, float rgba[4]);
boolean MNColorBox_SetColor4f(mn_object_t* obj, int flags, float red, float green,
    float blue, float alpha);

/// Change the current red color component.
/// @return  @c true if the value changed.
boolean MNColorBox_SetRedf(mn_object_t* obj, int flags, float red);

/// Change the current green color component.
/// @return  @c true if the value changed.
boolean MNColorBox_SetGreenf(mn_object_t* obj, int flags, float green);

/// Change the current blue color component.
/// @return  @c true if the value changed.
boolean MNColorBox_SetBluef(mn_object_t* obj, int flags, float blue);

/// Change the current alpha value. Note: will be NOP if this colorbox
/// is not operating in "rgba mode".
/// @return  @c true if the value changed.
boolean MNColorBox_SetAlphaf(mn_object_t* obj, int flags, float alpha);

/**
 * Copy the current color from @a other.
 * @param flags  @see mncolorboxSetColorFlags
 * @return  @c true if the current color changed.
 */
boolean MNColorBox_CopyColor(mn_object_t* obj, int flags, const mn_object_t* otherObj);

/**
 * Graphical slider.
 */
#define MNDATA_SLIDER_SLOTS             (10)
#define MNDATA_SLIDER_SCALE             (.75f)
#if __JDOOM__ || __JDOOM64__
#  define MNDATA_SLIDER_OFFSET_X        (0)
#  define MNDATA_SLIDER_OFFSET_Y        (0)
#  define MNDATA_SLIDER_PADDING_X       (0)
#  define MNDATA_SLIDER_PADDING_Y       (2)
#  define MNDATA_SLIDER_PATCH_LEFT      ("M_THERML")
#  define MNDATA_SLIDER_PATCH_RIGHT     ("M_THERMR")
#  define MNDATA_SLIDER_PATCH_MIDDLE    ("M_THERM2")
#  define MNDATA_SLIDER_PATCH_HANDLE    ("M_THERMO")
#elif __JHERETIC__ || __JHEXEN__
#  define MNDATA_SLIDER_OFFSET_X        (0)
#  define MNDATA_SLIDER_OFFSET_Y        (1)
#  define MNDATA_SLIDER_PADDING_X       (0)
#  define MNDATA_SLIDER_PADDING_Y       (0)
#  define MNDATA_SLIDER_PATCH_LEFT      ("M_SLDLT")
#  define MNDATA_SLIDER_PATCH_RIGHT     ("M_SLDRT")
#  define MNDATA_SLIDER_PATCH_MIDDLE    ("M_SLDMD1")
#  define MNDATA_SLIDER_PATCH_HANDLE    ("M_SLDKB")
#endif

typedef struct mndata_slider_s {
    float min, max;
    float value;
    float step; // Button step.
    boolean floatMode; // Otherwise only integers are allowed.
    /// \todo Turn this into a property record or something.
    void* data1;
    void* data2;
    void* data3;
    void* data4;
    void* data5;
} mndata_slider_t;

void MNSlider_Drawer(mn_object_t* obj, const Point2Raw* origin);
void MNSlider_TextualValueDrawer(mn_object_t* obj, const Point2Raw* origin);
int MNSlider_CommandResponder(mn_object_t* obj, menucommand_e command);
void MNSlider_UpdateGeometry(mn_object_t* obj, mn_page_t* page);
void MNSlider_TextualValueUpdateGeometry(mn_object_t* obj, mn_page_t* page);
int MNSlider_ThumbPos(const mn_object_t* obj);

/// @return  Current value represented by the slider.
float MNSlider_Value(const mn_object_t* obj);

/**
 * @defgroup mnsliderSetValueFlags  MNSlider Set Value Flags
 * @{
 */
#define MNSLIDER_SVF_NO_ACTION            0x1 /// Do not call any linked action function.
/**@}*/

/**
 * Change the current value represented by the slider.
 * @param flags  @see mnsliderSetValueFlags
 * @param value  New value.
 */
void MNSlider_SetValue(mn_object_t* obj, int flags, float value);

/**
 * Mobj preview visual.
 */
#define MNDATA_MOBJPREVIEW_WIDTH    38
#define MNDATA_MOBJPREVIEW_HEIGHT   52

typedef struct mndata_mobjpreview_s {
    int mobjType;
    /// Color translation class and map.
    int tClass, tMap;
    int plrClass; /// Player class identifier.
} mndata_mobjpreview_t;

void MNMobjPreview_SetMobjType(mn_object_t* obj, int mobjType);
void MNMobjPreview_SetPlayerClass(mn_object_t* obj, int plrClass);
void MNMobjPreview_SetTranslationClass(mn_object_t* obj, int tClass);
void MNMobjPreview_SetTranslationMap(mn_object_t* obj, int tMap);

void MNMobjPreview_Drawer(mn_object_t* obj, const Point2Raw* origin);
void MNMobjPreview_UpdateGeometry(mn_object_t* obj, mn_page_t* page);

// Menu render state:
typedef struct mn_rendstate_s {
    float pageAlpha;
    float textGlitter;
    float textShadow;
    float textColors[MENU_COLOR_COUNT][4];
    fontid_t textFonts[MENU_FONT_COUNT];
} mn_rendstate_t;
extern const mn_rendstate_t* mnRendState;

/**
 * @defgroup menuEffectFlags  Menu Effect Flags
 * @{
 */
#define MEF_TEXT_TYPEIN             (DTF_NO_TYPEIN)
#define MEF_TEXT_SHADOW             (DTF_NO_SHADOW)
#define MEF_TEXT_GLITTER            (DTF_NO_GLITTER)

#define MEF_EVERYTHING              (MEF_TEXT_TYPEIN|MEF_TEXT_SHADOW|MEF_TEXT_GLITTER)
/**@}*/

short MN_MergeMenuEffectWithDrawTextFlags(short f);

int MN_CountObjects(mn_object_t* list);
mn_object_t* MN_MustFindObjectOnPage(mn_page_t* page, int group, int flags);

void MN_DrawPage(mn_page_t* page, float alpha, boolean showFocusCursor);

/**
 * Execute a menu navigation/action command.
 */
void Hu_MenuCommand(menucommand_e cmd);

typedef enum {
    GUI_NONE,
    GUI_BOX,
    GUI_GROUP,
    GUI_HEALTH,
    GUI_ARMOR,
    GUI_KEYS,
    GUI_READYAMMO,
    GUI_FRAGS,
    GUI_LOG,
    GUI_CHAT,
#if __JDOOM__
    GUI_AMMO,
    GUI_WEAPONSLOT,
    GUI_FACE,
    GUI_ARMORICON,
#endif
#if __JHERETIC__
    GUI_TOME,
#endif
#if __JHEXEN__
    GUI_ARMORICONS,
    GUI_WEAPONPIECES,
    GUI_BLUEMANAICON,
    GUI_BLUEMANA,
    GUI_BLUEMANAVIAL,
    GUI_GREENMANAICON,
    GUI_GREENMANA,
    GUI_GREENMANAVIAL,
    GUI_BOOTS,
    GUI_SERVANT,
    GUI_DEFENSE,
    GUI_WORLDTIMER,
#endif
#if __JDOOM__ || __JHERETIC__
    GUI_READYAMMOICON,
    GUI_KEYSLOT,
    GUI_SECRETS,
    GUI_ITEMS,
    GUI_KILLS,
#endif
#if __JHERETIC__ || __JHEXEN__
    GUI_INVENTORY,
    GUI_CHAIN,
    GUI_READYITEM,
    GUI_FLIGHT,
#endif
    GUI_AUTOMAP,
    GUI_MAPNAME
} guiwidgettype_t;

typedef int uiwidgetid_t;

typedef struct uiwidget_s {
    /// Type of this widget.
    guiwidgettype_t type;

    /// Unique identifier associated with this widget.
    uiwidgetid_t id;

    /// @see alignmentFlags
    int alignFlags;

    /// Maximum size of this widget in pixels.
    Size2Raw maxSize;

    /// Geometry of this widget in pixels.
    RectRaw geometry;

    /// Local player number associated with this widget.
    /// \todo refactor away.
    int player;

    /// Current font used for text child objects of this widget.
    fontid_t font;

    /// Current opacity value for this widget. Used during render.
    float alpha;

    void (*updateGeometry) (struct uiwidget_s* obj);
    void (*drawer) (struct uiwidget_s* obj, int x, int y);
    void (*ticker) (struct uiwidget_s* obj, timespan_t ticLength);

    void* typedata;
} uiwidget_t;

/// @return  @c true= the current tic is 'sharp' once ran for @a ticLength
boolean GUI_RunGameTicTrigger(timespan_t ticLength);
boolean GUI_GameTicTriggerIsSharp(void);

void GUI_DrawWidget(uiwidget_t* obj, int x, int y);

/// @return  @see alignmentFlags
int UIWidget_Alignment(uiwidget_t* obj);

float UIWidget_Alpha(uiwidget_t* obj);

const RectRaw* UIWidget_Geometry(uiwidget_t* obj);

int UIWidget_MaximumHeight(uiwidget_t* obj);

const Size2Raw* UIWidget_MaximumSize(uiwidget_t* obj);

int UIWidget_MaximumWidth(uiwidget_t* obj);

const Point2Raw* UIWidget_Origin(uiwidget_t* obj);

/// @return  Local player number of the owner of this widget.
int UIWidget_Player(uiwidget_t* obj);

void UIWidget_RunTic(uiwidget_t* obj, timespan_t ticLength);

void UIWidget_SetAlpha(uiwidget_t* obj, float alpha);

/**
 * @param alignFlags  @see alignmentFlags
 */
void UIWidget_SetAlignment(uiwidget_t* obj, int alignFlags);

void UIWidget_SetMaximumHeight(uiwidget_t* obj, int height);

void UIWidget_SetMaximumSize(uiwidget_t* obj, const Size2Raw* size);

void UIWidget_SetMaximumWidth(uiwidget_t* obj, int width);

/**
 * @defgroup uiWidgetGroupFlags  UI Widget Group Flags.
 */
/*@{*/
#define UWGF_LEFTTORIGHT        0x0001
#define UWGF_RIGHTTOLEFT        0x0002
#define UWGF_VERTICAL           0x0004
/*@}*/

typedef struct {
    int flags;
    int padding;
    int widgetIdCount;
    uiwidgetid_t* widgetIds;
} guidata_group_t;

void UIGroup_AddWidget(uiwidget_t* obj, uiwidget_t* other);
int UIGroup_Flags(uiwidget_t* obj);
void UIGroup_SetFlags(uiwidget_t* obj, int flags);

void UIGroup_UpdateGeometry(uiwidget_t* obj);

typedef struct {
    int value;
} guidata_health_t;

typedef struct {
    int value;
} guidata_armor_t;

typedef struct {
    int value;
} guidata_readyammo_t;

typedef struct {
    int value;
} guidata_frags_t;

#if __JDOOM__ || __JHERETIC__
typedef struct {
    int slot;
    keytype_t keytypeA;
    patchid_t patchId;
#  if __JDOOM__
    keytype_t keytypeB;
    patchid_t patchId2;
#  endif
} guidata_keyslot_t;
#endif

#if __JDOOM__
typedef struct {
    ammotype_t ammotype;
    int value;
} guidata_ammo_t;

typedef struct {
    int slot;
    patchid_t patchId;
} guidata_weaponslot_t;
#endif

#if __JDOOM__
typedef struct {
    int oldHealth; // Used to use appopriately pained face.
    int faceCount; // Count until face changes.
    int faceIndex; // Current face index, used by wFaces.
    int lastAttackDown;
    int priority;
    boolean oldWeaponsOwned[NUM_WEAPON_TYPES];
} guidata_face_t;
#endif

typedef struct {
    boolean keyBoxes[NUM_KEY_TYPES];
} guidata_keys_t;

#if __JDOOM__ || __JHERETIC__
typedef struct {
#if __JHERETIC__
    patchid_t patchId;
#else // __JDOOM__
    int sprite;
#endif
} guidata_readyammoicon_t;

typedef struct {
    int value;
} guidata_secrets_t;

typedef struct {
    int value;
} guidata_items_t;

typedef struct {
    int value;
} guidata_kills_t;
#endif

#if __JDOOM__
typedef struct {
    int sprite;
} guidata_armoricon_t;
#endif

#if __JHERETIC__
typedef struct {
    patchid_t patchId;
    int countdownSeconds; // Number of seconds remaining or zero if disabled.
    int play; // Used with the countdown sound.
} guidata_tomeofpower_t;
#endif

#if __JHEXEN__
typedef struct {
    struct armortype_s {
        int value;
    } types[NUMARMOR];
} guidata_armoricons_t;

typedef struct {
    int pieces;
} guidata_weaponpieces_t;

typedef struct {
    int iconIdx;
} guidata_bluemanaicon_t;

typedef struct {
    int value;
} guidata_bluemana_t;

typedef struct {
    int iconIdx;
    float filled;
} guidata_bluemanavial_t;

typedef struct {
    int iconIdx;
} guidata_greenmanaicon_t;

typedef struct {
    int value;
} guidata_greenmana_t;

typedef struct {
    int iconIdx;
    float filled;
} guidata_greenmanavial_t;

typedef struct {
    patchid_t patchId;
} guidata_boots_t;

typedef struct {
    patchid_t patchId;
} guidata_servant_t;

typedef struct {
    patchid_t patchId;
} guidata_defense_t;

typedef struct {
    int days, hours, minutes, seconds;
} guidata_worldtimer_t;
#endif

#if __JHERETIC__ || __JHEXEN__
typedef struct {
    int healthMarker;
    int wiggle;
} guidata_chain_t;

typedef struct {
    patchid_t patchId;
    int flashCounter;
} guidata_readyitem_t;

typedef struct {
    patchid_t patchId;
    boolean hitCenterFrame;
} guidata_flight_t;
#endif

void GUI_Register(void);

void GUI_Init(void);
void GUI_Shutdown(void);
void GUI_LoadResources(void);
void GUI_ReleaseResources(void);

uiwidget_t* GUI_FindObjectById(uiwidgetid_t id);

/// Identical to GUI_FindObjectById except results in a fatal error if not found.
uiwidget_t* GUI_MustFindObjectById(uiwidgetid_t id);

uiwidgetid_t GUI_CreateWidget(guiwidgettype_t type, int player, fontid_t fontId, float alpha,
    void (*updateGeometry) (uiwidget_t* obj), void (*drawer) (uiwidget_t* obj, int x, int y),
    void (*ticker) (uiwidget_t* obj, timespan_t ticLength), void* typedata);

uiwidgetid_t GUI_CreateGroup(int player, int groupFlags, int alignFlags, int padding);

typedef struct ui_rendstate_s {
    float pageAlpha;
} ui_rendstate_t;
extern const ui_rendstate_t* uiRendState;

#endif /* LIBCOMMON_UI_LIBRARY_H */
