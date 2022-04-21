/* See r_colormap.cpp for credits. */
#pragma once

#include "effect.h"
#include "effect_info.h"
#include "handles.h"

#include <immintrin.h>
#include <string>
#include <vector>

#define NUM_COLOR_VALUES               256  // 2 ^ BITS_PER_CHANNEL (i.e. 8)
#define COLORMAP_NUM_MAPS              8
#define COLORMAP_MAX_COLORS            256
#define COLORMAP_MAP_FILENAME_MAXLEN   48
#define COLORMAP_MAP_COLOR_CONFIG_SIZE (3 * 4)          // 3 ints
#define COLORMAP_BASE_CONFIG_SIZE      (3 * 4 + 1 * 4)  // 3 ints + 4 bytes
#define COLORMAP_SAVE_MAP_HEADER_SIZE \
    (sizeof(uint32_t) * 3 + COLORMAP_MAP_FILENAME_MAXLEN)

#define COLORMAP_COLOR_KEY_RED          0
#define COLORMAP_COLOR_KEY_GREEN        1
#define COLORMAP_COLOR_KEY_BLUE         2
#define COLORMAP_COLOR_KEY_RGB_SUM_HALF 3
#define COLORMAP_COLOR_KEY_MAX          4
#define COLORMAP_COLOR_KEY_RGB_AVERAGE  5

#define COLORMAP_MAP_CYCLE_NONE            0
#define COLORMAP_MAP_CYCLE_BEAT_RANDOM     1
#define COLORMAP_MAP_CYCLE_BEAT_SEQUENTIAL 2

#define COLORMAP_MAP_CYCLE_TIMER_ID        1
#define COLORMAP_MAP_CYCLE_SPEED_MIN       1
#define COLORMAP_MAP_CYCLE_SPEED_MAX       64
#define COLORMAP_MAP_CYCLE_ANIMATION_STEPS NUM_COLOR_VALUES

#define COLORMAP_BLENDMODE_REPLACE    0
#define COLORMAP_BLENDMODE_ADDITIVE   1
#define COLORMAP_BLENDMODE_MAXIMUM    2
#define COLORMAP_BLENDMODE_MINIMUM    3
#define COLORMAP_BLENDMODE_5050       4
#define COLORMAP_BLENDMODE_SUB1       5
#define COLORMAP_BLENDMODE_SUB2       6
#define COLORMAP_BLENDMODE_MULTIPLY   7
#define COLORMAP_BLENDMODE_XOR        8
#define COLORMAP_BLENDMODE_ADJUSTABLE 9

#define COLORMAP_ADJUSTABLE_BLEND_MAX 255

#define COLORMAP_NUM_CYCLEMODES 3
#define COLORMAP_NUM_KEYMODES   6
#define COLORMAP_NUM_BLENDMODES 10

struct ColorMap_Map_Color : public Effect_Config {
    int64_t position = 0;
    uint64_t color = 0;
    int64_t color_id = 0;
    static Handles id_factory;

    ColorMap_Map_Color() : color_id(id_factory.get()){};
    ColorMap_Map_Color(int64_t position, uint64_t color)
        : position(position), color(color), color_id(id_factory.get()){};
};

struct ColorMap_Map : public Effect_Config {
    bool enabled = false;
    int64_t length = 2;
    int64_t map_id = 0;
    std::string filepath = "";
    std::vector<ColorMap_Map_Color> colors;
    std::vector<uint64_t> baked_map;
    static Handles id_factory;

    ColorMap_Map() : map_id(id_factory.get()) {
        this->colors.emplace_back(0, 0x000000);
        this->colors.emplace_back(255, 0xffffff);
        this->baked_map.assign(NUM_COLOR_VALUES, 0);
    };
};

struct ColorMap_Config : public Effect_Config {
    int64_t color_key = 0;
    int64_t blendmode = 0;
    int64_t map_cycle_mode = 0;
    int64_t adjustable_alpha = 0;
    bool dont_skip_fast_beats = false;
    int64_t map_cycle_speed = 11;
    std::vector<ColorMap_Map> maps;
    int64_t current_map = 0;
    int64_t next_map = 0;
    bool disable_map_change = false;

    ColorMap_Config() {
        for (int64_t i = 0; i < 8; i++) {
            this->maps.emplace_back();
        }
        this->maps[0].enabled = true;
    }
};

void bake_full_map(Effect*, const Parameter*, std::vector<int64_t>);
void on_current_map_change(Effect*, const Parameter*, std::vector<int64_t>);
void on_map_colors_change(Effect*,
                          const Parameter*,
                          std::vector<int64_t>,
                          int64_t,
                          int64_t);
void on_map_cycle_mode_change(Effect*, const Parameter*, std::vector<int64_t>);
void flip_map(Effect*, const Parameter*, std::vector<int64_t>);
void clear_map(Effect*, const Parameter*, std::vector<int64_t>);
void save_map(Effect*, const Parameter*, std::vector<int64_t>);
void load_map(Effect*, const Parameter*, std::vector<int64_t>);

struct ColorMap_Info : public Effect_Info {
    static constexpr char* group = "Trans";
    static constexpr char* name = "Color Map";
    static constexpr char* help =
        "Color Map stores 8 different colormaps. For every point on the screen, a key"
        " is calculated and used as an index (value 0-255) into the map. The point's"
        " color will be replaced by the one in the map at that index.\n"
        "If the on-beat cycling option is on, Color Map will cycle between all enabled"
        " maps. If it's turned off, only Map 1 is used.\n"
        "\n"
        "To edit the map, drag the arrows around with the left mouse button. You can"
        " add/edit /remove colors by right-clicking, double-click an empty position to"
        " add a new point there or double-click an existing point to edit it.\n"
        "\n"
        "Note: While editing maps, cycling is temporarily disabled. Switch to another"
        " component or close the AVS editor to enable it again.";
    static constexpr int32_t legacy_id = -1;
    static constexpr char* legacy_ape_id = "Color Map";

    static const char* const* map_cycle_modes(int64_t* length_out) {
        *length_out = 3;
        static const char* const options[3] = {
            "None (single map)", "On-beat random", "On-beat sequential"};
        return options;
    };
    static const char* const* color_keys(int64_t* length_out) {
        *length_out = 6;
        static const char* const options[6] = {"Red Channel",
                                               "Green Channel",
                                               "Blue Channel",
                                               "(R+G+B)/2",
                                               "Maximal Channel",
                                               "(R+G+B)/3"};
        return options;
    };
    static const char* const* blendmodes(int64_t* length_out) {
        *length_out = 10;
        static const char* const options[10] = {"Replace",
                                                "Additive",
                                                "Maximum",
                                                "Minimum",
                                                "50/50",
                                                "Subtractive 1",
                                                "Subtractive 2",
                                                "Multiply",
                                                "XOR",
                                                "Adjustable"};
        return options;
    };

    static constexpr uint32_t num_map_color_params = 3;
    static constexpr Parameter map_color_params[num_map_color_params] = {
        PARAM_IRANGE(ColorMap_Map_Color,
                     position,
                     "Position",
                     NULL,
                     bake_full_map,
                     0,
                     255),
        PARAM(ColorMap_Map_Color, color, COLOR, "Color", NULL, bake_full_map),
        PARAM_X(ColorMap_Map_Color, color_id, INT, "Color ID", NULL, NULL),
    };
    static constexpr uint32_t num_map_params = 9;
    static constexpr Parameter map_params[num_map_params] = {
        PARAM(ColorMap_Map, enabled, BOOL, "Enabled", NULL, NULL),
        PARAM(ColorMap_Map, map_id, INT, "Map ID", NULL, NULL),
        PARAM(ColorMap_Map, filepath, STRING, "File Path", NULL, NULL),
        PARAM_LIST(ColorMap_Map,
                   colors,
                   "Colors",
                   NULL,
                   num_map_color_params,
                   1,
                   255,
                   map_color_params,
                   ColorMap_Map_Color,
                   on_map_colors_change,
                   on_map_colors_change,
                   on_map_colors_change),
        PARAM_ACTION("Flip Map", "Reverse direction of the map", flip_map),
        PARAM_ACTION("Clear Map", "Return map to the default", clear_map),
        PARAM_ACTION("Save Map", "Save map to a file", save_map),
        PARAM_ACTION("Load Map", "Load map from a file", load_map),
        PARAM_X(ColorMap_Map, baked_map, COLOR_ARRAY, "Baked Map", NULL, NULL),
    };
    static constexpr uint32_t num_parameters = 10;
    static constexpr Parameter parameters[num_parameters] = {
        PARAM_SELECT(ColorMap_Config, color_key, "Key", NULL, NULL, color_keys),
        PARAM_SELECT(ColorMap_Config, blendmode, "Blendmode", NULL, NULL, blendmodes),
        PARAM_SELECT(ColorMap_Config,
                     map_cycle_mode,
                     "Map Cycle Mode",
                     NULL,
                     on_map_cycle_mode_change,
                     map_cycle_modes),
        PARAM_IRANGE(ColorMap_Config,
                     adjustable_alpha,
                     "Adjustable Alpha",
                     NULL,
                     NULL,
                     0,
                     255),
        PARAM(ColorMap_Config,
              dont_skip_fast_beats,
              BOOL,
              "Noskip",
              "Don't skip fast beats",
              NULL),
        PARAM_IRANGE(ColorMap_Config,
                     map_cycle_speed,
                     "Map Cycle Speed",
                     NULL,
                     NULL,
                     1,
                     64),
        PARAM_LIST(ColorMap_Config,
                   maps,
                   "Maps",
                   NULL,
                   num_map_params,
                   8,
                   8,
                   map_params,
                   ColorMap_Map,
                   NULL,
                   NULL,
                   NULL),
        PARAM_X(ColorMap_Config, current_map, INT, "Current Map", NULL, bake_full_map),
        PARAM_X(ColorMap_Config, next_map, INT, "Next Map", NULL, NULL),
        PARAM_X(ColorMap_Config,
                disable_map_change,
                BOOL,
                "Disable Map Change",
                NULL,
                NULL),
    };

    EFFECT_INFO_GETTERS;
};

class E_ColorMap : public Configurable_Effect<ColorMap_Info, ColorMap_Config> {
   public:
    E_ColorMap();

    /* APE interface */
    virtual int render(char visdata[2][2][576],
                       int isBeat,
                       int* framebuffer,
                       int* fbout,
                       int w,
                       int);
    virtual void load_legacy(unsigned char* data, int len);
    virtual int save_legacy(unsigned char* data);

    /* Other utilities */
    void flip_map(size_t map_index);
    void clear_map(size_t map_index);
    void save_map(size_t map_index);
    void load_map(size_t map_index);
    void bake_full_map(size_t map_index);

    std::vector<uint64_t> tween_map;
    int32_t change_animation_step;

   protected:
    int next_id = 1337;
    int get_new_id();
    bool any_maps_enabled();
    void animation_step();
    void animation_step_sse2();
    void reset_tween_map();
    std::vector<uint64_t>& animate_map_frame(int is_beat);
    int get_key(int color);
    void blend(std::vector<uint64_t>& blend_map, int* framebuffer, int w, int h);
    __m128i get_key_ssse3(__m128i color4);
    void blend_ssse3(std::vector<uint64_t>& blend_map, int* framebuffer, int w, int h);
    size_t load_map_header(unsigned char* data, int len, size_t map_index, int pos);
    bool load_map_colors(unsigned char* data,
                         int len,
                         int pos,
                         size_t map_index,
                         uint32_t map_length);

   public:
};
