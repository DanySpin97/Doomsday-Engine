/** @file p_intercept.cpp World map line / object interception.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de_base.h"
#include "de_console.h"
#include "de_play.h"

#include "world/map.h"

#define MININTERCEPTS       128

struct InterceptNode
{
    InterceptNode *next;
    InterceptNode *prev;
    intercept_t intercept;
};

// Blockset from which intercepts are allocated.
static zblockset_t *interceptNodeSet;
// Head of the used intercept list.
static InterceptNode *interceptFirst;

// Trace nodes.
static InterceptNode head;
static InterceptNode tail;
static InterceptNode *mru;

static inline bool isSentinel(InterceptNode const &node)
{
    return &node == &tail || &node == &head;
}

static InterceptNode *newInterceptNode()
{
    // Can we reuse an existing intercept?
    if(!isSentinel(*interceptFirst))
    {
        InterceptNode *node = interceptFirst;
        interceptFirst = node->next;
        return node;
    }
    return (InterceptNode *) ZBlockSet_Allocate(interceptNodeSet);
}

void P_ClearIntercepts()
{
    if(!interceptNodeSet)
    {
        interceptNodeSet = ZBlockSet_New(sizeof(InterceptNode), MININTERCEPTS, PU_APPSTATIC);

        // Configure the static head and tail.
        head.intercept.distance = 0.0f;
        head.next = &tail;
        head.prev = NULL;
        tail.intercept.distance = 1.0f;
        tail.prev = &head;
        tail.next = NULL;
    }

    // Start reusing intercepts (may point to a sentinel but that is Ok).
    if(!interceptFirst)
    {
        interceptFirst = head.next;
    }
    else if(head.next != &tail)
    {
        InterceptNode *existing = interceptFirst;
        interceptFirst = head.next;
        tail.prev->next = existing;
    }

    // Reset the trace.
    head.next = &tail;
    tail.prev = &head;
    mru = NULL;
}

InterceptNode *P_AddIntercept(intercepttype_t type, float distance, void *object)
{
    DENG_ASSERT(object != 0);

    // First reject vs our sentinels
    if(distance < head.intercept.distance) return NULL;
    if(distance > tail.intercept.distance) return NULL;

    // Find the new intercept's ordered place along the trace.
    InterceptNode *before;
    if(mru && mru->intercept.distance <= distance)
        before = mru->next;
    else
        before = head.next;

    while(before->next && distance >= before->intercept.distance)
    {
        before = before->next;
    }

    // Pull a new intercept from the used queue.
    InterceptNode *newNode = newInterceptNode();

    // Configure the new intercept.
    intercept_t *in = &newNode->intercept;
    in->type = type;
    in->distance = distance;
    switch(in->type)
    {
    case ICPT_MOBJ:
        in->d.mobj = (mobj_s *) object;
        break;
    case ICPT_LINE:
        in->d.line = (Line *) object;
        break;
    }

    // Link it in.
    newNode->next = before;
    newNode->prev = before->prev;

    newNode->prev->next = newNode;
    newNode->next->prev = newNode;

    mru = newNode;
    return newNode;
}

int P_TraverseIntercepts(traverser_t callback, void *context)
{
    for(InterceptNode *node = head.next; !isSentinel(*node); node = node->next)
    {
        if(int result = callback(&node->intercept, context))
            return result; // Stop iteration.
    }
    return false; // Continue iteration.
}
