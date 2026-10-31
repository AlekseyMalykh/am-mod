#include "src/resample_output_file.h"
#include <cassert>
static ResampleOutputFile output;
extern "C" int output_callback_capture(const char*want,const char*actual){return output.capture(want,actual,44100,2);}
extern "C" int resample_fast_native_prepare_import(const char*path){assert(path&&path[0]=='/');return output.pending()?output.commit(path):1;}
