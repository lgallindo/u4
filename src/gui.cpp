/*
  XU4 GUI Layout
  Copyright (C) 2022  Karl Robillard

  This file is part of XU4.

  XU4 is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  XU4 is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with XU4.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <assert.h>
#include <cstring>
#include "image32.h"
#include "gpu.h"
#include "gui.h"
#include "screen.h"
#include "txf_draw.h"
#include "xu4.h"

#define ATTR_COUNT  7
#define LO_DEPTH    6

enum AlignBits {
    ALIGN_L = 1,
    ALIGN_R = 2,
    ALIGN_T = 4,
    ALIGN_B = 8,
    BACKWARDS = 0x10,
    H_MASK = ALIGN_L | ALIGN_R | 0xf0,
    V_MASK = ALIGN_T | ALIGN_B | 0xf0
};

/*
enum SizePolicy {
    POL_FIXED,      // Use minimum.
    POL_MINIMAL,    // Prefer minimum.
    POL_EXPAND      // Prefer maximum.
};
*/

typedef struct {
    int16_t minW, minH;
    int16_t maxW, maxH;
    int16_t prefW, prefH;
} SizeCon;

//----------------------------------------------------------------------------

static float* gui_drawRect(float* attr, const int16_t* wbox,
                           const TxfHeader* txf, int colorIndex)
{
    float rect[4];
    float uvs[4];

    rect[0] = (float) wbox[0];
    rect[1] = (float) wbox[1];
    rect[2] = (float) wbox[2];
    rect[3] = (float) wbox[3];

    uvs[0] = ((float) colorIndex + 0.5f) / txf->texW;
    uvs[1] = 0.5f / txf->texH;
    uvs[2] = uvs[0];
    uvs[3] = uvs[1];

    return gpu_emitQuad(attr, rect, uvs);
}

static void button_size(SizeCon* size, TxfDrawState* ds, const uint8_t* text)
{
    float fsize[2];
    txf_emSize(ds->tf, text, strlen((const char*) text), fsize);

    size->minW =
    size->maxW =
    size->prefW = (int16_t) (fsize[0] * ds->psize + 1.2f * ds->psize);

    size->minH =
    size->maxH =
    size->prefH = (int16_t) (fsize[1] * ds->psize * 1.6f);
}

static float* widget_button(float* attr, const GuiRect* wbox,
                            const SizeCon* scon, TxfDrawState* ds,
                            const uint8_t* text)
{
    int textW;
    int quadCount;

    attr = gui_drawRect(attr, &wbox->x, ds->tf, 5);

    textW = scon->prefW - (int16_t) (1.2f * ds->psize);
    ds->x = (float) (wbox->x + ((wbox->w - textW) / 2));
    ds->y = (float) wbox->y - ds->tf->descender * ds->psize + 0.3f * ds->psize;
    quadCount = txf_genText(ds, attr + 3, attr, ATTR_COUNT,
                            text, strlen((const char*) text));
    return attr + (quadCount * 6 * ATTR_COUNT);
}

static void label_size(SizeCon* size, TxfDrawState* ds, const uint8_t* text)
{
    float fsize[2];
    txf_emSize(ds->tf, text, strlen((const char*) text), fsize);
    size->minW = size->maxW = size->prefW = (int16_t) (fsize[0] * ds->psize);
    size->minH = size->maxH = size->prefH = (int16_t) (fsize[1] * ds->psize);
}

static float* widget_label(float* attr, const GuiRect* wbox, TxfDrawState* ds,
                           const uint8_t* text)
{
    int quadCount;

    ds->x = (float) wbox->x;
    ds->y = (float) wbox->y + ds->tf->descender * ds->psize;
    quadCount = txf_genText(ds, attr + 3, attr, ATTR_COUNT,
                            text, strlen((const char*) text));
    return attr + (quadCount * 6 * ATTR_COUNT);
}

static void list_size(SizeCon* size, TxfDrawState* ds, StringTable* st)
{
    float fsize[2];
    int rows;

    if (st->used) {
        const char* text;
        const char* longText;
        int len;
        int maxLen = 0;

        for (uint32_t i = 0; i < st->used; ++i) {
            text = sst_stringL(st, i, &len);
            if (len > maxLen) {
                maxLen = len;
                longText = text;
            }
        }
        txf_emSize(ds->tf, (const uint8_t*) longText, maxLen, fsize);
    } else {
        txf_emSize(ds->tf, (const uint8_t*) "<empty>", 7, fsize);
    }
    fsize[0] *= ds->psize;
    fsize[1] *= ds->psize;

    rows = (st->used < 3) ? 3 : st->used;

    size->minW = (int16_t) (ds->psize * 4.0f);
    size->minH = 3 * (int16_t) fsize[1];
    size->maxW = 512;
    size->maxH = 9000;
    size->prefW = (int16_t) fsize[0];
    size->prefH = (int16_t) (fsize[1] * rows);
}

static float* widget_list(float* attr, const GuiRect* wbox, TxfDrawState* ds,
                          StringTable* st)
{
    const uint8_t* strings = (const uint8_t*) sst_strings(st);
    const StringEntry* it  = st->table;
    const StringEntry* end = it + st->used;
    float left = (float) wbox->x;
    int quadCount;

    //attr = gui_drawRect(attr, &wbox->x, ds->tf, 3);

    ds->y = (float) (wbox->y + wbox->h) - ds->tf->descender * ds->psize;

    for (; it != end; ++it) {
        ds->x = left;
        ds->y -= ds->lineSpacing;
        quadCount = txf_genText(ds, attr + 3, attr, ATTR_COUNT,
                                strings + it->start, it->len);
        attr += quadCount * 6 * ATTR_COUNT;
        ds->x = left;
    }
    return attr;
}

//----------------------------------------------------------------------------

struct LayoutBox {
    int16_t x, y, w, h;         // Pixel units.  X,Y is the bottom left.
    int16_t nextPos;            // X or Y position for the next widget.
    uint8_t form;               // LAYOUT_H, LAYOUT_V, LAYOUT_GRID
    uint8_t align;              // AlignBits
    uint8_t fixedW;             // Assign fixed width to children.
    uint8_t fixedH;             // Assign fixed height to children.
    uint8_t spacing;            // Pixel gap between widgets.
    uint8_t columns;            // For LAYOUT_GRID.
};

static void gui_align(GuiRect* wbox, LayoutBox* lo, const SizeCon* cons)
{
#if 0
    printf("KR lo:%d,%d fw:%d,%d cons:%d,%d\n",
           lo->w, lo->h, lo->fixedW, lo->fixedH, cons->prefW, cons->prefH);
#endif

    wbox->w = cons->prefW;
    if (lo->fixedW && wbox->w < lo->fixedW)
        wbox->w = lo->fixedW;

    wbox->h = cons->prefH;
    if (lo->fixedH && wbox->h < lo->fixedH)
        wbox->h = lo->fixedH;

    if (lo->form == LAYOUT_H) {
        if (lo->align & BACKWARDS) {
            // Layout right to left.
            wbox->x = lo->nextPos - wbox->w;
            lo->nextPos = wbox->x - lo->spacing;
        } else {
            // Layout left to right.
            wbox->x = lo->nextPos;
            lo->nextPos += wbox->w + lo->spacing;
        }

        if (lo->align & ALIGN_B)
            wbox->y = lo->y;
        else if (lo->align & ALIGN_T)
            wbox->y = (lo->y + lo->h) - wbox->h;
        else
            wbox->y = (lo->y + lo->h / 2) - (wbox->h / 2);
    } else {
        if (lo->align & BACKWARDS) {
            // Layout bottom to top.
            wbox->y = lo->nextPos;
            lo->nextPos += wbox->h + lo->spacing;
        } else {
            // Layout top to bottom.
            wbox->y = lo->nextPos - wbox->h;
            lo->nextPos = wbox->y - lo->spacing;
        }

        if (lo->align & ALIGN_L)
            wbox->x = lo->x;
        else if (lo->align & ALIGN_R)
            wbox->x = (lo->x + lo->w) - wbox->w;
        else
            wbox->x = (lo->x + lo->w / 2) - (wbox->w / 2);
    }
}

/*
  Create a GPU draw list for widgets using a bytecode language and a
  single-pass layout algorithm.

  The layout program must begin with a LAYOUT_* instruction and ends with a
  paired LAYOUT_END instruction.

  \param primList   The list identifier used with gpu_beginTris()/gpu_endTris().
  \param root       A rectangular pixel area for this layout, or NULL to
                    use screen size.
  \param bytecode   A program of GuiOpcode instructions.
  \param data       A pointer array of data referenced by bytecode program.
*/
void gui_layout(int primList, const GuiRect* root, const TxfHeader* txf,
                const uint8_t* bytecode, const void** data)
{
    TxfDrawState ds;
    SizeCon cons;
    LayoutBox loStack[LO_DEPTH];
    LayoutBox* lo = NULL;
    GuiRect wbox;
    float* attr;
    int arg;
    const uint8_t* pc = bytecode;

    // Default to 'natural' size of font.
    txf_begin(&ds, txf, txf->fontSize, 0.0f, 0.0f);

    attr = gpu_beginTris(xu4.gpu, primList);

#define INIT_LO(lo,type) \

    for(;;) {
        switch (*pc++) {
        default:
        case GUI_END:
            goto done;

        // Position Pen
        case PEN_PER:           // percent x, percent y
            arg = *pc++;
            ds.x = arg * lo->w / 100;
            arg = *pc++;
            ds.y = arg * lo->h / 100;
            break;

        // Layout
        case LAYOUT_V:
        case LAYOUT_H:
            if (lo == NULL) {
                lo = loStack;
                if (root) {
                    memcpy(&lo->x, root, sizeof(GuiRect));
                } else {
                    const ScreenState* ss = screenState();
                    lo->x = lo->y = 0;
                    lo->w = ss->displayW;
                    lo->h = ss->displayH;
                }
            } else {
                assert(lo != loStack+LO_DEPTH-1);
                if (lo->form == LAYOUT_H) {
                    lo[1].x = lo->nextPos;
                    lo[1].y = lo->y;
                    lo[1].w = lo->w - lo->nextPos;
                    lo[1].h = lo->h;
                } else {
                    lo[1].x = lo->x;
                    lo[1].y = lo->y;
                    lo[1].w = lo->w;
                    lo[1].h = lo->nextPos - lo->y;
                }
                ++lo;
            }

            lo->form = pc[-1];
            lo->nextPos = (lo->form == LAYOUT_H) ? lo->x : lo->y + lo->h;
            lo->align = 0;      // Place forwards & centered.
            lo->fixedW = lo->fixedH = lo->spacing = 0;

            ds.y = (float) lo->h;
            break;

        case LAYOUT_GRID:    // columns
            // TODO: Implement this.
            lo->form = pc[-1];
            lo->columns = *pc++;
            break;

        case LAYOUT_END:
            if (lo == loStack)
                goto done;
            --lo;
            break;

        case MARGIN_PER:     // percent
        case MARGIN_V_PER:   // percent
            arg = *pc++;
            arg = arg * lo->h / 100;
            lo->y += arg;
            lo->h -= arg + arg;
            if (lo->form == LAYOUT_V) {
                if (lo->align & BACKWARDS)
                    lo->nextPos = lo->y;
                else
                    lo->nextPos = lo->y + lo->h;
            }
            if (pc[-2] == MARGIN_V_PER)
                break;
            // Fall through...

        case MARGIN_H_PER:   // percent
            arg = *pc++;
            arg = arg * lo->w / 100;
            lo->x += arg;
            lo->w -= arg + arg;
            if (lo->form == LAYOUT_H) {
                lo->nextPos = lo->x;
                if (lo->align & BACKWARDS)
                    lo->nextPos += lo->w;
            }
            break;

        /*
        case MARGIN_EM:      // font-em-tenth
        case MARGIN_V_EM:    // font-em-tenth
        case MARGIN_H_EM:    // font-em-tenth
            arg = *pc++;
            break;
        */

        case SPACING_PER:    // percent
            arg = *pc++;
            lo->spacing = arg * ((lo->form == LAYOUT_H) ? lo->w : lo->h) / 100;
            break;

        case SPACING_EM:     // font-em-tenth
            arg = *pc++;
            lo->spacing = arg * ds.psize / 10;
            break;

        case FIX_WIDTH_PER:  // percent
            arg = *pc++;
            lo->fixedW = arg * lo->w / 100;
            break;

        case FIX_HEIGHT_PER: // percent
            arg = *pc++;
            lo->fixedH = arg * lo->h / 100;
            break;

        case FROM_BOTTOM:
            lo->align |= BACKWARDS;
            lo->nextPos = lo->y;
            break;

        case FROM_RIGHT:
            lo->align |= BACKWARDS;
            lo->nextPos = lo->x + lo->w;
            break;

        case ALIGN_LEFT:
            lo->align = (lo->align & V_MASK) | ALIGN_L;
            break;

        case ALIGN_RIGHT:
            lo->align = (lo->align & V_MASK) | ALIGN_R;
            break;

        case ALIGN_TOP:
            lo->align = (lo->align & H_MASK) | ALIGN_T;
            break;

        case ALIGN_BOTTOM:
            lo->align = (lo->align & H_MASK) | ALIGN_B;
            break;

        case ALIGN_H_CENTER:
            lo->align &= V_MASK;
            break;

        case ALIGN_V_CENTER:
            lo->align &= H_MASK;
            break;

        case ALIGN_CENTER: 
            lo->align = 0;
            break;

        case GAP_PER:
            arg = *pc++;
            arg = arg * ((lo->form == LAYOUT_H) ? lo->w : lo->h) / 100;
            if (lo->align & BACKWARDS)
                lo->nextPos -= arg;
            else
                lo->nextPos += arg;
            break;

        case FONT_SIZE:      // point-size
            txf_setFontSize(&ds, (float) *pc++);
            break;

        // Drawing
        case BG_COLOR_CI:   // color-index
            attr = gui_drawRect(attr, &lo->x, ds.tf, *pc++);
            break;

        // Widgets
        case BUTTON_DT_S:
            button_size(&cons, &ds, (const uint8_t*) *data);
            gui_align(&wbox, lo, &cons);
            attr = widget_button(attr, &wbox, &cons, &ds,
                                 (const uint8_t*) *data++);
            break;

        case LABEL_DT_S:
            label_size(&cons, &ds, (const uint8_t*) *data);
            gui_align(&wbox, lo, &cons);
            attr = widget_label(attr, &wbox, &ds, (const uint8_t*) *data++);
            break;

        case LIST_DT_ST:
            list_size(&cons, &ds, (StringTable*) *data);
            gui_align(&wbox, lo, &cons);
            attr = widget_list(attr, &wbox, &ds, (StringTable*) *data++);
            break;

        case STORE_DT_AREA:
            {
            GuiRect* dst = (GuiRect*) *data++;
            memcpy(dst, &wbox, sizeof(GuiRect));
            }
            break;
        }
    }

done:
    gpu_endTris(xu4.gpu, primList, attr);
}
