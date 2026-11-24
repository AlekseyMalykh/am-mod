#ifndef AM_UI_LIST_GEOMETRY_H
#define AM_UI_LIST_GEOMETRY_H
/* Screen lists grow downward; the hardware fader grows upward. Knobs and
 * musical pitch rows keep their own direction. Shared 320x240 list geometry. */
static inline int ui_list_fader(unsigned value,int last){
 if(value>127)value=127;
 return last>0?(int)(127-value)*last/127:0;
}
static inline int ui_list_first(int selected,int count,int visible){
 int first=selected-visible/2,max=count>visible?count-visible:0;
 return first<0?0:first>max?max:first;
}
static inline int ui_list_thumb(int first,int count,int visible,int extent,int*offset){
 if(count<=visible||count<=0){*offset=0;return extent;}
 int size=extent*visible/count;if(size<4)size=4;if(size>extent)size=extent;
 int max=count-visible;if(first<0)first=0;if(first>max)first=max;
 *offset=first*(extent-size)/max;return size;
}
#endif
