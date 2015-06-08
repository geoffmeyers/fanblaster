/*
 * maxes out your nvidia gpu fans and attempts to keep them there.
 * will attempt to use audio alerts if the temp creeps up too high
 * or the api fails.
 *
 * api addresses borrowed from (yes, the url is misleading):
 *   http://stackoverflow.com/questions/13291783/how-to-get-the-id-memory-address-of-dll-function
 *   copied those into a addresses.txt in case stack overflow link 
 *   eventually dies
 *
 * structure information borrowed from open-hardware-monitor:
 *   http://code.google.com/p/open-hardware-monitor
 *   calced the version values based on values from this project
 *
 * borrowed heavily from the idea/basic structure presented at:
 *   http://eliang.blogspot.ch/2011/05/getting-nvidia-gpu-usage-in-c.html
 *   (since i couldn't get nvapi.lib to work with mingw)
 *
 * no warranty whatsoever.  if this blows your hardware up, that's on you.
 * that being said: hardware explosions are precisely what i'm trying to 
 * avoid and all the overclocking utilities and whatnot that are out there
 * are notoriously brittle.
 *
 * disclaimer: i'm not entirely sure what will happen if you're running
 * an nvidia chipset on your motherboard.  it could be that this will
 * crank those fans up to 100% too.  maybe tread carefully if that's not
 * desirable to you.
 *
 * reasoning: they say these video cards can run at 85C and be okay, but 
 * i've always found that they start acting flakey when they get that 
 * high, so this program will just set your fans to 100% and keep them 
 * there.  hopefully by repeatedly overriding the fan speed and not doing
 * much else this will be a better solution than afterburner, firestorm,
 * precision, etc. for cases where you're not overclocking -- you're just
 * trying to keep your hardware from cooking.
 *
 * on that note, i've heard some gpus will report temps in fahrenheit,
 * but this shouldn't be a problem as long as you verify what scale you're
 * trying to alert in.
 */
 
#include <windows.h>
#include <iostream>
 
// magic numbers, do not change them
#define NVAPI_MAX_PHYSICAL_GPUS   64
#define NVAPI_MAX_USAGES_PER_GPU  34
#define NVAPI_MAX_COOLER_PER_GPU  20
#define NVAPI_MAX_THERMAL_SENSORS_PER_GPU 3

// does no verification to make sure this exists...
#define AUDIO_CLIP_PATH "C:/WINDOWS/Media/notify.wav"

struct nv_thermal_internal {
  int controller; // GPU_INTERNAL (should be 1, but...)
  unsigned int default_min_temp;
  unsigned int default_max_temp;
  unsigned int current_temp;
  int target; // GPU = 1
};

struct nv_thermals {
  unsigned int version;
  unsigned int count;
  struct nv_thermal_internal internal[NVAPI_MAX_THERMAL_SENSORS_PER_GPU];
};

struct nv_fan_internal {
  int type;
  int controller;
  int default_min;
  int default_max;
  int current_min;
  int current_max;
  int current_level;
  int default_policy;
  int current_policy;
  int target;
  int control_type;
  int active;
};

struct nv_fandata {
  unsigned int version;
  unsigned int count;
  struct nv_fan_internal internal[NVAPI_MAX_COOLER_PER_GPU];
};

struct nv_level_internal {
  int level;
  int policy;
};

struct nv_levels {
  unsigned int version;
  struct nv_level_internal internal[NVAPI_MAX_COOLER_PER_GPU];
};

void play_audio_alert(void) {
  bool played = PlaySound(AUDIO_CLIP_PATH, NULL, SND_FILENAME);
  if(!played) {
    std::cout << "WARNING: tried to play audio to alert you about something!" << std::endl;
  }
}

// for the function pointers handed back by the query interface
typedef int *(*NvAPI_QueryInterface_t)(unsigned int offset);
typedef int (*NvAPI_Initialize_t)();
typedef int (*NvAPI_EnumPhysicalGPUs_t)(int **handles, int *count);
typedef int (*NvAPI_GPU_GetUsages_t)(int *handle, unsigned int *usages);
typedef int (*NvAPI_GPU_GetTachReading_t)(int *handle, unsigned int *value);
typedef int (*NvAPI_GPU_GetCoolerSettings_t)(int *handle, unsigned int, struct nv_fandata &);
typedef int (*NvAPI_GPU_SetCoolerSettings_t)(int *handle, unsigned int, struct nv_levels &);
typedef int (*NvAPI_GPU_GetThermalSettings_t)(int *handle, unsigned int, struct nv_thermals &);

int main(int argc, char *argv[]) {   

  HMODULE hmod = LoadLibraryA("nvapi.dll");

  if (hmod == NULL) {
    std::cout << "couldn't find nvapi.dll" << std::endl;
    play_audio_alert();
    return 1;
  }
 
  unsigned int thermal_alert = 0;
  if(argc != 2) {
    std::cout << "usage: " << argv[0] << " [temp to alert at, 0 to disable]" << std::endl;
    play_audio_alert();
    return 1;
  }
  thermal_alert = atoi(argv[1]);

  // nvapi.dll internal function pointers
  NvAPI_QueryInterface_t         NvAPI_QueryInterface         = NULL;
  NvAPI_Initialize_t             NvAPI_Initialize             = NULL;
  NvAPI_EnumPhysicalGPUs_t       NvAPI_EnumPhysicalGPUs       = NULL;
  NvAPI_GPU_GetUsages_t          NvAPI_GPU_GetUsages          = NULL;
  NvAPI_GPU_GetTachReading_t     NvAPI_GPU_GetTachReading     = NULL;
  NvAPI_GPU_GetCoolerSettings_t  NvAPI_GPU_GetCoolerSettings  = NULL;
  NvAPI_GPU_SetCoolerSettings_t  NvAPI_GPU_SetCoolerSettings  = NULL;
  NvAPI_GPU_GetThermalSettings_t NvAPI_GPU_GetThermalSettings = NULL;
 
  // nvapi_QueryInterface is a function used to retrieve other internal functions in nvapi.dll
  NvAPI_QueryInterface     = (NvAPI_QueryInterface_t) GetProcAddress(hmod, "nvapi_QueryInterface");
 
  // some useful internal functions that aren't exported by nvapi.dll
  NvAPI_Initialize             = (NvAPI_Initialize_t) NvAPI_QueryInterface(0x0150E828);
  NvAPI_EnumPhysicalGPUs       = (NvAPI_EnumPhysicalGPUs_t) NvAPI_QueryInterface(0xE5AC921F);
  NvAPI_GPU_GetUsages          = (NvAPI_GPU_GetUsages_t) NvAPI_QueryInterface(0x189A1FDF);
  NvAPI_GPU_GetTachReading     = (NvAPI_GPU_GetTachReading_t) NvAPI_QueryInterface(0x5F608315);
  NvAPI_GPU_GetCoolerSettings  = (NvAPI_GPU_GetCoolerSettings_t) NvAPI_QueryInterface(0xDA141340);
  NvAPI_GPU_SetCoolerSettings  = (NvAPI_GPU_SetCoolerSettings_t) NvAPI_QueryInterface(0x891FA0AE);
  NvAPI_GPU_GetThermalSettings = (NvAPI_GPU_GetThermalSettings_t) NvAPI_QueryInterface(0xE3640A56);
 
  if(NvAPI_Initialize             == NULL ||
     NvAPI_EnumPhysicalGPUs       == NULL ||
     NvAPI_EnumPhysicalGPUs       == NULL ||
     NvAPI_GPU_GetUsages          == NULL ||
     NvAPI_GPU_GetTachReading     == NULL ||
     NvAPI_GPU_GetCoolerSettings  == NULL ||
     NvAPI_GPU_SetCoolerSettings  == NULL ||
     NvAPI_GPU_GetThermalSettings == NULL) {
    std::cout << "Couldn't get functions in nvapi.dll" << std::endl;
    play_audio_alert();
    return 1;
  }
 
  int retval;
  int gpu_count = 0;
  int *gpu_handles[NVAPI_MAX_PHYSICAL_GPUS] = {NULL};
  unsigned int gpu_tach[NVAPI_MAX_COOLER_PER_GPU] = {0};

  // initializes the library
  retval = NvAPI_Initialize();
  if(retval != 0) {
    std::cout << "couldn't initialize nvapi" << std::endl;
    play_audio_alert();
    return 1;
  }

  // the version information here tells the API how we expect our results back.
  // this probably isn't the safest as the source for these values calculates them
  // whereas i just calculated the values once in mono, found they worked, and moved on.
  // (the structures/values were from open-hardware-monitor)
  struct nv_fandata cooler;
  cooler.version = 132040; 
  cooler.count = 1;

  struct nv_levels levels;
  levels.version = 65700;

  struct nv_thermals thermals;
  thermals.version = 65604;

  unsigned int gpu_usages[NVAPI_MAX_USAGES_PER_GPU];
  gpu_usages[0] = 65672; // acts as version

  // how many gpus do we have?
  retval = NvAPI_EnumPhysicalGPUs(gpu_handles, &gpu_count);
  if(retval != 0) {
    std::cout << "nvapi couldn't detect a gpu" << std::endl;
    return 1;
  }
  std::cout << "api detected " << gpu_count << " nvidia gpu(s)." << std::endl << std::endl;
 
  while(1) {

    if(!thermal_alert) {
      std::cout << "WARNING: thermal alert disabled, pass non-zero value on command line to enable" << std::endl << std::endl;
    }
    else {
      std::cout << "thermal alert enabled if temp goes above: " << thermal_alert << std::endl << std::endl;
    }
    
    for(int gpu_index = 0; gpu_index < gpu_count; gpu_index++) {
      
      bool play_alert = false;

      retval = NvAPI_GPU_GetTachReading(gpu_handles[gpu_index], gpu_tach);
      if(retval != 0) {
        std::cout << "WARNING: NvAPI_GPU_GetTachReadings retval was not 0!  got instead: " << retval << std::endl;
        play_audio_alert();
        continue;
      }

      retval = NvAPI_GPU_GetThermalSettings(gpu_handles[gpu_index], 0, thermals);
      if(retval != 0) {
        std::cout << "WARNING: NvAPI_GPU_GetThermalSettings retval was not 0!  got instead: " << retval << std::endl;
        play_audio_alert();
        continue;
      }

      retval = NvAPI_GPU_GetCoolerSettings(gpu_handles[gpu_index], 0, cooler);
      if(retval != 0) {
        std::cout << "WARNING: NvAPI_GPU_GetCoolerSettings retval was not 0!  got instead: " << retval << std::endl;
        play_audio_alert();
        continue;
      }

      retval = NvAPI_GPU_GetUsages(gpu_handles[gpu_index], gpu_usages);
      if(retval != 0) {
        std::cout << "WARNING: NvAPI_GPU_GetUsages retval was not 0!  got instead: " << retval << std::endl;
        play_audio_alert();
        continue;
      }

      int usage = gpu_usages[3];
      std::cout << "gpu: " << gpu_index << std::endl;
      std::cout << "  - usage percentage: " << usage << std::endl;
      
      // thermal sensor reporting
      for(unsigned int x = 0; x < thermals.count; x++) {
        std::cout << "  - thermal sensor: " << x << std::endl;
        std::cout << "    - controller: " << thermals.internal[x].controller << std::endl;
        std::cout << "    - current temp: " << thermals.internal[x].current_temp << std::endl;
        std::cout << "    - default min temp: " << thermals.internal[x].default_min_temp << std::endl;
        std::cout << "    - default max temp: " << thermals.internal[x].default_max_temp << std::endl;
        std::cout << "    - target: " << thermals.internal[x].target << std::endl;
        if(thermal_alert and thermals.internal[x].current_temp > thermal_alert) {
          std::cout << "WARNING: NvAPI_GPU_GetThermalSettings says temp is too high on gpu " << gpu_index << " sensor " << x << std::endl;
          play_alert = true;
        }
      }

      // queried the fan just so we can report about it
      for(unsigned int x = 0; x < cooler.count; x++) {
        std::cout << "  - cooler: " << x << std::endl;
        std::cout << "    - type: " << cooler.internal[x].type << std::endl;
        std::cout << "    - controller: " << cooler.internal[x].controller << std::endl;
        std::cout << "    - defaultmin: " << cooler.internal[x].default_min << std::endl;
        std::cout << "    - defaultmax: " << cooler.internal[x].default_max << std::endl;
        std::cout << "    - currentmin: " << cooler.internal[x].current_min << std::endl;
        std::cout << "    - currentmax: " << cooler.internal[x].current_max << std::endl;
        std::cout << "    - currentlevel: " << cooler.internal[x].current_level << std::endl;
        std::cout << "    - defaultpolicy: " << cooler.internal[x].default_policy << std::endl;
        std::cout << "    - currentpolicy: " << cooler.internal[x].current_policy << std::endl;
        std::cout << "    - target: " << cooler.internal[x].target << std::endl;
        std::cout << "    - controltype: " << cooler.internal[x].control_type << std::endl;
        std::cout << "    - active: " << cooler.internal[x].active << std::endl;
        std::cout << "    - rpms: " << gpu_tach[x] << std::endl;
      }

      // now we set the fan speed
      for(unsigned int x = 0; x < cooler.count; x++) {
        levels.internal[x].level = 100; // gogo full blast
        levels.internal[x].policy = cooler.internal[x].current_policy;
        retval = NvAPI_GPU_SetCoolerSettings(gpu_handles[gpu_index], 0, levels);
        if(retval != 0) {
          std::cout << "WARNING: NvAPI_GPU_SetCoolerSettings retval was not 0!  got instead: " << retval << std::endl;
          play_alert = true;
        }
      }

      std::cout << "-----------------" << std::endl;

      if(play_alert) {
        play_audio_alert();
      }
    }

    Sleep(1000);
  }
 
  // should never get here
  return 0;
}
