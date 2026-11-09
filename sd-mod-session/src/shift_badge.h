#ifndef AM_SHIFT_BADGE_H
#define AM_SHIFT_BADGE_H
/* Four frame strokes followed by the arrow, drawn without a font glyph. */
static const unsigned char shift_badge_rects[][4]={
    {0,0,10,1},{0,11,10,1},{0,1,1,10},{9,1,1,10},
    {4,2,2,1},{3,3,4,1},{2,4,6,1},{4,5,2,4}
};
#define SHIFT_BADGE_RECTS(draw,g,x,y) do { \
    for(unsigned i=0;i<sizeof(shift_badge_rects)/sizeof(shift_badge_rects[0]);++i){ \
        const unsigned char*r=shift_badge_rects[i]; \
        (draw)((g),(x)+r[0],(y)+r[1],r[2],r[3]); \
    } \
} while(0)
#define SHIFT_BADGE_COLOURED(draw,g,x,y,ink) do { \
    for(unsigned i=0;i<sizeof(shift_badge_rects)/sizeof(shift_badge_rects[0]);++i){ \
        const unsigned char*r=shift_badge_rects[i]; \
        (draw)((g),(x)+r[0],(y)+r[1],r[2],r[3],(ink)); \
    } \
} while(0)
#endif
