#pragma once

#include "c__base.h"
#include "effect.h"
#include "effect_info.h"

struct Fadeout_Config : public Effect_Config {
    int64_t fadelen;
    uint64_t color;
};

void maketab(Effect* component,
             const Parameter* parameter,
             std::vector<int64_t> parameter_path);

#define OFFSET(field) offsetof(Fadeout_Config, field)

struct Fadeout_Info : public Effect_Info {
    static constexpr char* group = "Trans";
    static constexpr char* name = "Fadeout";
    static constexpr char* help =
        "Fadeout the whole screen a certain amount per frame.\n";
    static constexpr int32_t legacy_id = 3;
    static constexpr char* legacy_ape_id = NULL;

    static constexpr uint32_t num_parameters = 2;
    static constexpr Parameter parameters[num_parameters] = {
        // config, field, [type,] name, desc, onchange
        P_IRANGE(OFFSET(fadelen), "Fade Velocity", 0, 92, NULL, maketab),
        P_COLOR(OFFSET(color), "Fade to Color", NULL, maketab),
    };

    EFFECT_INFO_GETTERS;
};

class E_Fadeout : public Configurable_Effect<Fadeout_Info, Fadeout_Config> {
   protected:
   public:
    E_Fadeout();
    virtual ~E_Fadeout();
    virtual int render(char visdata[2][2][576],
                       int isBeat,
                       int* framebuffer,
                       int* fbout,
                       int w,
                       int h);
    virtual void load_legacy(unsigned char* data, int len);
    virtual int save_legacy(unsigned char* data);

    void maketab(void);

    unsigned char fadtab[3][256];
};
