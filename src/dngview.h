/*
 * $Id$
 */

#ifndef DNGVIEW_H
#define DNGVIEW_H

#ifdef __cplusplus
extern "C" {
#endif

struct _ListNode;

typedef enum {
    DNGGRAPHIC_NONE,
    DNGGRAPHIC_WALL,
    DNGGRAPHIC_LADDERUP,
    DNGGRAPHIC_LADDERDOWN,
    DNGGRAPHIC_LADDERUPDOWN,
    DNGGRAPHIC_DOOR,
    DNGGRAPHIC_DNGTILE,
    DNGGRAPHIC_BASETILE
} DungeonGraphicType;

struct _ListNode *dungeonViewGetTiles(int fwd, int side);
DungeonGraphicType dungeonViewTilesToGraphic(struct _ListNode *tiles);

#ifdef __cplusplus
}
#endif

#endif
