#pragma once

#include "effect.h"
#include "effect_info.h"

enum ColorReduction_Levels {
    COLRED_LEVELS_2 = 0,
    COLRED_LEVELS_4 = 1,
    COLRED_LEVELS_8 = 2,
    COLRED_LEVELS_16 = 3,
    COLRED_LEVELS_32 = 4,
    COLRED_LEVELS_64 = 5,
    COLRED_LEVELS_128 = 6,
    COLRED_LEVELS_256 = 7,
};

struct ColorReduction_Config : public Effect_Config {
    int64_t levels = COLRED_LEVELS_256;
};

struct ColorReduction_Info : public Effect_Info {
    static constexpr char* group = "Trans";
    static constexpr char* name = "Color Reduction";
    static constexpr char* help = "";
    static constexpr int32_t legacy_id = -1;
    static constexpr char* legacy_ape_id = "Color Reduction";

    static const char* const* levels(int64_t* length_out) {
        *length_out = 8;
        static const char* const options[8] = {
            "2",
            "4",
            "8",
            "16",
            "32",
            "64",
            "128",
            "256",
        };
        return options;
    };

    static constexpr uint32_t num_parameters = 1;
    static constexpr Parameter parameters[num_parameters] = {
        P_SELECT(offsetof(ColorReduction_Config, levels), "Color Levels", levels),
    };

    EFFECT_INFO_GETTERS;
};

class E_ColorReduction
    : public Configurable_Effect<ColorReduction_Info, ColorReduction_Config> {
   public:
    E_ColorReduction();
    virtual ~E_ColorReduction();
    virtual int render(char visdata[2][2][576],
                       int isBeat,
                       int* framebuffer,
                       int* fbout,
                       int w,
                       int h);
    virtual void load_legacy(unsigned char* data, int len);
    virtual int save_legacy(unsigned char* data);
};
