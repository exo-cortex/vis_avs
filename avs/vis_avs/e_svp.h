#pragma once

#include "r_defs.h"

#include "effect.h"
#include "effect_info.h"
#include "svp_vis.h"

#include "../platform.h"

struct SVP_Config : public Effect_Config {
    int64_t library = -1;
};

void set_library(Effect*, const Parameter*, std::vector<int64_t>);

struct SVP_Info : public Effect_Info {
    static constexpr char* group = "Render";
    static constexpr char* name = "SVP";
    static constexpr char* help = "";
    static constexpr int32_t legacy_id = 10;
    static constexpr char* legacy_ape_id = NULL;

    static const char* const* library_files(int64_t* length_out);

    static constexpr uint32_t num_parameters = 1;
    static constexpr Parameter parameters[num_parameters] = {
        P_SELECT(offsetof(SVP_Config, library),
                 "Library",
                 library_files,
                 NULL,
                 set_library),
    };

    EFFECT_INFO_GETTERS;
};

class E_SVP : public Configurable_Effect<SVP_Info, SVP_Config> {
   public:
    E_SVP();
    virtual ~E_SVP();
    virtual int render(char visdata[2][2][576],
                       int isBeat,
                       int* framebuffer,
                       int* fbout,
                       int w,
                       int h);
    virtual void load_legacy(unsigned char* data, int len);
    virtual int save_legacy(unsigned char* data);

    void find_library_files();
    void clear_library_files();
    void set_library();

    static std::vector<std::string> filenames;
    static const char** c_filenames;

    dlib_t* library;
    lock_t* library_lock;
    VisInfo* vi;
    VisData vd;
};
