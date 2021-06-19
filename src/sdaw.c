// sdaw.c

#include "sdaw.h"

#include "common.c"
#include "module.c"
#include "config.c"
#include "lut.c"
#include "debug.c"
#include "memory.c"
#include "list.c"
#include "math_util.c"
#include "arg_parser.c"
#include "image.c"
#include "audio.c"
#include "riff.c"
#include "vorbis.c"
#include "image_seq.c"
#include "gen_audio.c"
#include "image_interp.c"

#ifndef NO_ENGINE
  #include "engine.c"
#else
  #define EngineInit() NoError
  #define EngineFree()
#endif

i32 ImageToAudioGen = 0;
i32 AudioToImageGen = 0;
i32 ImageInterpolation = 0;

static parse_arg Arguments[] = {
  {'a', "audio-gen", "image to audio generator", ArgInt, 0, &ImageToAudioGen},
  {'i', "image-seq", "audio to image sequence generator", ArgInt, 0, &AudioToImageGen},
  {'I', "image-interpolate", "image interpolation", ArgInt, 0, &ImageInterpolation},
};

i32 SdawStart(i32 argc, char** argv) {
  i32 Result = NoError;
  srand(time(NULL));
  LoadConfig();
  if (argc <= 1) {
    Result = EngineInit();
    EngineFree();
  }
  else {
    Result = ParseArgs(Arguments, ArraySize(Arguments), 2 /* parse only the first command */, argv);
    if (Result != NoError) {
      return Result;
    }
    if (ImageToAudioGen) {
      Result = GenAudio(argc - 1, &argv[1]);
    }
    else if (AudioToImageGen) {
      Result = ImageSeq(argc - 1, &argv[1]);
    }
    else if (ImageInterpolation) {
      Result = ImageInterp(argc - 1, &argv[1]);
    }
  }
  if (MemoryTotal() != 0) {
    fprintf(stderr, "Memory leak!\n");
    MemoryPrintInfo(stdout);
    Assert(0);
  }
  return Result;
}
