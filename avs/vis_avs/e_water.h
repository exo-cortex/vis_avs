#pragma once

#include "effect.h"
#include "effect_info.h"

struct Water_Config : public Effect_Config {};

struct Water_Info : public Effect_Info {
    static constexpr char* group = "Trans";
    static constexpr char* name = "Water";
    static constexpr char* help = "";
    static constexpr int32_t legacy_id = 20;
    static constexpr char* legacy_ape_id = NULL;

    EFFECT_INFO_GETTERS_NO_PARAMETERS;
};

class E_Water : public Configurable_Effect<Water_Info, Water_Config> {
   public:
    E_Water();
    virtual ~E_Water();
    virtual int render(char visdata[2][2][576],
                       int isBeat,
                       int* framebuffer,
                       int* fbout,
                       int w,
                       int h);
    virtual void load_legacy(unsigned char* data, int len);
    virtual int save_legacy(unsigned char* data);

    virtual bool can_multithread() { return true; };
    virtual int smp_begin(int max_threads,
                          char visdata[2][2][576],
                          int isBeat,
                          int* framebuffer,
                          int* fbout,
                          int w,
                          int h);
    virtual void smp_render(int this_thread,
                            int max_threads,
                            char visdata[2][2][576],
                            int isBeat,
                            int* framebuffer,
                            int* fbout,
                            int w,
                            int h);
    virtual int smp_finish(char visdata[2][2][576],
                           int isBeat,
                           int* framebuffer,
                           int* fbout,
                           int w,
                           int h);  // return value is that of render() for fbstuff etc

    unsigned int* lastframe;
    int lastframe_size;
};
