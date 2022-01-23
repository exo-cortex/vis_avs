#pragma once

/**
 * == How to Create a New Effect ==
 *
 * An Effect has at least one "Config" struct, which consists of a set of "Parameters",
 * and an "Info" struct describing the config. To create the config and info, each
 * effect subclasses `Effect_Config` and `Effect_Info`, and creates its own class from
 * the `Configurable_Effect` template with those two classes as template parameters. If
 * you're unfamiliar with C++ templates this may be confusing, but you don't really need
 * to understand how templates work in order to implement a new Effect class.
 *
 * The incantations for a new Effect all follow the same pattern:
 *
 *   1. Declare the Config struct.
 *   2. Declare the Info struct, referencing the config. This is the hard part.
 *   3. Declare the Effect class, using the Config and Info struct names as template
 *      parameters.
 *
 * In more detail, if you wanted to create a new effect "Foo":
 *
 * 1.
 *
 * Declare a `Foo_Config` struct inheriting from `Effect_Config` (which can be empty if
 * your effect has no options). Set default values for every parameter (except lists).
 *
 *     struct Foo_Config : public Effect_Config {
 *         int64_t foo = 1;
 *         double bar = 0.5;
 *         // ...
 *     };
 *
 * Note that no "enabled" parameter is needed, every effect automatically gets one.
 *
 * All members of the config struct must use one of the following C++ types:
 *   - booleans:                 bool
 *   - integer values & selects: int64_t
 *   - floating point values:    double
 *   - color values:             uint64_t
 *   - strings:                  std::string
 *   Special cases:
 *   - parameter lists:          std::vector<T>, where T is the type of the child config
 *   - actions:                  Action parameters don't have a value & don't appear in
 *                               the config struct.
 *
 * You may declare a default constructor (but usually shouldn't need to) if special
 * things need to happen on init of your config (e.g. to fill a list with a default set
 * of entries). If you define a non-default constructor, you have to define the default
 * constructor explicitly too (write `Foo_Config() = default;`). Declaring any other
 * methods inside your struct invokes compiler-specific and possibly undefined behavior
 * because of the way `Effect_Info` is implemented! Use free functions instead, if you
 * must, or rather just add them to your effect class instead of the config.
 *
 * 2.
 *
 * Declare an accompanying `Foo_Info` struct inheriting from `Effect_Info`, which has
 * some required fields:
 *
 *     struct Foo_Info : public Effect_Info {
 *         static constexpr char* group = "Render";
 *         static constexpr char* name = "Foo";
 *
 *         static constexpr uint32_t num_parameters = 2;
 *         static constexpr Parameter parameters[num_parameters] = {
 *             //    CONFIG      FIELD TYPE  NAME   DESC          ONCHANGE
 *             PARAM(Foo_Config, foo,  INT,  "Foo", "Does a foo", NULL),
 *             //           CONFIG      FIELD NAME   DESC        ONCHANGE MIN  MAX
 *             PARAM_FRANGE(Foo_Config, bar,  "Bar", "Does bar", NULL,    0.0, 1.0)
 *         };
 *
 *         EFFECT_INFO_GETTERS;
 *     };
 *
 * Use the `PARAM` macro to safely add an entry to the `parameters` array. Alternatively
 * you can declare `Parameter` structs yourself, and set unneeded fields to 0 or NULL as
 * appropriate. The `PARAM*` macros do this for you. For select- and list-type
 * parameters, use `PARAM_SELECT` and `PARAM_LIST` respectively.
 *
 * There are quite a few requirements on the Info struct and inter-dependencies with the
 * Config struct:
 *
 *   - `group` can be any string. Groups are not part of the identity of the
 *     effect, so you can change the group of the effect and older presets will load
 *     just fine. It's _highly discouraged_ to make up your own category here. Instead
 *     see what other groups already exist, and find one that fits. If the existing
 *     groups won't, there's a "Misc" group for outliers.
 *
 *   - `name` can be any string (can include spaces, etc.). It should be unique among
 *     existing effects.
 *
 *   - `num_parameters` in Info must match the number of parameters defined in Config.
 *     (To be precise, it must not be larger. If it's smaller, then the remaining config
 *     parameters will be inaccessible for editing. Do with that information what you
 *     will.)
 *
 *   - There are a few different macros to choose from when declaring parameters:
 *
 *     - `PARAM` declares a generic parameter.
 *
 *     - `PARAM_IRANGE` and `PARAM_FRANGE` declare limited integer and floating-point
 *       parameters, respectively. Setting a value outside of the min/max range will set
 *       a clamped value.
 *       An editor UI may render a range-limited parameter as a slider.
 *
 *     - `PARAM_SELECT` creates a parameter that can be selected from a list of options.
 *       The value type is actually an integer index (refer to avs_editor.h). To set the
 *       options pass the name of a function that returns a list of strings, and writes
 *       the length of the list to its input parameter. Have a look into existing
 *       effects for examples.
 *       A smart editor UI may turn a short list into a radio select, and a longer list
 *       into a dropdown select.
 *
 *     - `PARAM_LIST` is an advanced feature not needed by most effects. It creates a
 *       list of sets of parameters. Declaring parameter lists will be explained in more
 *       detail further down (TODO).
 *
 *     - `PARAM_ACTION` lets you register a function to be triggered through the API. An
 *       action parameter isn't represented by a config field and cannot be read or set.
 *
 *     - All macros have an `_X`-suffixed version, which excludes them from being saved
 *       in a preset (except for actions, which have no value and thus cannot be saved).
 *       So if you choose `PARAM_X` instead of `PARAM` the parameter will not be saved
 *       and not loaded back.
 *
 *     - The arguments to `PARAM` (which it shares with the others) are as follows:
 *
 *       - `CONFIG` must be the Config struct name.
 *
 *       - `FIELD` must be the Config struct member name.
 *
 *       - `TYPE` must be one of `BOOL`, `INT`, `FLOAT`, `COLOR` or `STRING`. The other
 *         parameter macros lack this argument, since the type is implicit there.
 *
 *       - `NAME` can be any string, and it must not be NULL. It must be unique among
 *         your effect's other parameters. It doesn't have to bear any resemblance to
 *         `FIELD` (but it'd probably be wise). An editor UI may use this to label the
 *         parameter.
 *
 *       - `DESC` can be any string, or NULL. An editor UI may use this to further
 *         describe and explain the parameter if needed (e.g. in a tooltip).
 *
 *       - `ONCHANGE` (or `ONADD` for action parameters) may be a function pointer or
 *         NULL. The function gets called whenever the value is changed (or the action
 *         triggered) through the editor API. The signature is:
 *
 *            void on_change(Effect* component, const Parameter* parameter,
 *                          std::vector<int64_t> parameter_path)
 *
 *         You could use `parameter->name` to identify a parameter if the function
 *         handles multiple parameter changes.
 *
 *         Both the `component` and `parameter` pointers are guaranteed to be non-NULL
 *         and valid. If the handler is for a parameter inside a list, `parameter_path`
 *         is guaranteed to be of the correct length to contain your list index, you
 *         don't need to test `parameter_path.size()` in your handlers. If your effect
 *         doesn't have list-type parameters, it's safe to just ignore `parameter_path`.
 *         For `PARAM_LIST` use `ONADD`, `ONMOVE` & `ONREMOVE` (see below).
 *
 *         Tip: Make the handler function a static member function of the info struct
 *         instead of a free function. This way it will not conflict with other effects'
 *         handler functions (which are often named similarly).
 *
 *     - `MIN` & `MAX` set the minimum and maximum value of a ranged parameter. Note
 *       that if `MIN == MAX`, then the parameter is unlimited!
 *
 *     - `GETOPTS` is a function to retrieve the list of options. This is not a static
 *       list, because some lists aren't known at compile time (e.g. image filenames).
 *       The signature is:
 *
 *         const char* const* get_opts(int64_t* length_out)
 *
 *       The list length must be written to the location pointed to by `length_out`.
 *
 *     - If `LENGTH_MIN` and `LENGTH_MAX` in `PARAM_LIST` are both 0 then the list
 *       length is unlimited. If they are both the same but non-zero, then the list is
 *       fixed at that size (elements can still be moved around).
 *
 *     - `ONADD`, `ONMOVE` & `ONREMOVE` are three callback functions that get called
 *       whenever the respective action was successfully performed on the parameter
 *       list. The signature for all 3 functions is:
 *
 *         void on_change(Effect* component, const Parameter* parameter,
 *                        std::vector<int64_t> parameter_path,
 *                        int64_t index1, int64_t index2)
 *
 *       where:
 *
 *       - for `ONADD` `index1` is the new index and `index2` is always zero.
 *       - for `ONMOVE` `index1` is the previous index and `index2` is the new index.
 *       - for `ONREMOVE` `index1` is the removed index and `index2` is always zero.
 *
 *       With the signatures all the same, you can pass the same function to all three.
 *
 *       Note that the `on_move` handler will not be called if the element is "moved" to
 *       the same index it's already at. It's only called for actual changes.
 *
 *   - Write `EFFECT_INFO_GETTERS;` at the end of the Info class declaration, it will
 *     fill in some methods needed for accessing the fields declared above.
 *
 * 3.
 *
 * Now you're finally ready to declare your `E_Foo` class, specializing the
 * `Configurable_Effect` template, using the Info and Config struct names as template
 * parameters:
 *
 *     class E_Foo : public Configurable_Effect<Foo_Info, Foo_Config> {
 *        public:
 *         E_Foo() {
 *             // Access config through `this->config`.
 *             this->config.foo += 1;
 *         };
 *         // ...
 *     };
 *
 * This is the basic recipe for setting up a configurable effect.
 *
 * TODO: Explain variable length parameter lists in more detail. For now have a look at
 *       ColorMap for an effect with _all_ the bells and whistles.
 * TODO: Move this into an example effect file.
 *
 * Q: Why are Config and Info `struct`s, but Effect is a `class`?
 * A: In C++, for all intents and purposes `class` and `struct` are interchangeable,
 *    with one key difference: `struct` members are public by default, and `class`
 *    members private. So `struct` is just less to write if all members are public.
 *    Additionally, using `struct`s is sometimes used to signal a "simpler" object type
 *    containing predominantly data members (as opposed to methods). This is also the
 *    case here.
 */

#include "avs_editor.h"
#include "handles.h"

#include <algorithm>  // std::move() for list_move
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>

/**
 * This file encapsulates most of the dirty parts around component- and parameter
 * introspection (with effect.h covering the rest).
 *
 * The goal is to abstract away the introspection (which is bound to be hacky, because
 * C++ is not tuned for this) and keep both the external API _and_ the internal code for
 * individual effects as clean and simple as possible. Beyond declaration of the
 * Effect_Info struct, effects should have simple direct access to a struct with basic
 * config parameters, without any further intermediaries like getters/setters. In the
 * other direction the API should be as simple as C allows, with plain numerical
 * handles and arrays.
 *
 * The result is that the Effect_Info structure declaration is the most verbose and
 * fragile part. But it does make sense to make the trade-off this way, because it's
 * only declared statically at compile time once per effect and after that never really
 * touched again by any outside (API) or inside (effect) code.
 */

class Effect; // declared in effect.h

struct Parameter {
    AVS_Parameter_Handle handle;
    AVS_Parameter_Type type;
    const char* name;
    const char* description;
    bool is_saved;
    size_t offset;
    void (*on_value_change)(Effect* component,
                            const Parameter* parameter,
                            std::vector<int64_t> parameter_path);

    const char* const* (*get_options)(int64_t* length_out);

    int64_t int_min;
    int64_t int_max;
    double float_min;
    double float_max;

    uint32_t num_child_parameters;
    uint32_t num_child_parameters_min;
    uint32_t num_child_parameters_max;
    const Parameter* child_parameters;
    size_t sizeof_child;

    size_t (*list_length)(uint8_t* list_address);
    bool (*list_add)(uint8_t* list_address, uint32_t length_max, int64_t* before);
    bool (*list_move)(uint8_t* list_address, int64_t* from, int64_t* to);
    bool (*list_remove)(uint8_t* list_address, uint32_t length_min, int64_t* to_remove);
    void (*on_list_add)(Effect* component,
                        const Parameter* parameter,
                        std::vector<int64_t> parameter_path,
                        int64_t added,
                        int64_t _unused);
    void (*on_list_move)(Effect* component,
                         const Parameter* parameter,
                         std::vector<int64_t> parameter_path,
                         int64_t from,
                         int64_t to);
    void (*on_list_remove)(Effect* component,
                           const Parameter* parameter,
                           std::vector<int64_t> parameter_path,
                           int64_t removed,
                           int64_t _unused);
};

template <typename T>
size_t list_length(uint8_t* list_address) {
    auto list = (std::vector<T>*)list_address;
    return list->size();
}
template <typename T>
bool list_add(uint8_t* list_address, uint32_t length_max, int64_t* before) {
    auto list = (std::vector<T>*)list_address;
    if (list->size() >= length_max) {
        return false;
    }
    if (*before < 0 || (size_t)(*before < 0 ? 0 : *before) >= list->size()) {
        list->emplace_back();
        *before = list->size() - 1;
    } else {
        list->emplace(list->begin() + *before);
    }
    return true;
}
template <typename T>
bool list_move(uint8_t* list_address, int64_t* from, int64_t* to) {
    auto list = (std::vector<T>*)list_address;
    if (*from < 0) {
        *from = list->size() - 1;
    }
    if (*to < 0) {
        *to = list->size() - 1;
    }
    if (*from == *to) {
        // Technically correct, but don't run the on_move handler.
        return false;
    }
    if ((size_t)*from >= list->size() || (size_t)*to >= list->size()) {
        return false;
    }
    std::vector<T> new_list;
    bool found_from = false;
    bool found_to = false;
    for (auto e = list->begin(); e != list->end(); e++) {
        if (e == list->begin() + *to + found_from) {
            found_to = true;
            new_list.push_back((*list)[*from]);
        }
        if (e != list->begin() + *from) {
            new_list.push_back(*e);
        } else {
            found_from = true;
        }
    }
    if (!found_to) {
        new_list.push_back((*list)[*from]);
    }
    list->swap(new_list);
    return true;
}
template <typename T>
bool list_remove(uint8_t* list_address, uint32_t length_min, int64_t* to_remove) {
    auto list = (std::vector<T>*)list_address;
    if (list->size() <= length_min) {
        return false;
    }
    if ((size_t)(*to_remove < 0 ? 0 : *to_remove) >= list->size()) {
        return false;
    }
    list->erase(std::next(list->begin(), *to_remove));
    return true;
}

#define _PARAMETER_BASIC(SAVED, CONFIG, FIELD, TYPE, NAME, DESC, ONCHANGE)          \
    .handle = Handles::comptime_get(#CONFIG #FIELD #DESC #NAME),                    \
    .type = AVS_PARAM_##TYPE, .name = NAME, .description = DESC, .is_saved = SAVED, \
    .offset = offsetof(CONFIG, FIELD), .on_value_change = ONCHANGE

#define _PARAMETER_OPTIONS(GETOPTS) .get_options = GETOPTS
#define _PARAMETER_NOOPTIONS        .get_options = NULL

#define _PARAMETER_IRANGE(MIN, MAX) .int_min = MIN, .int_max = MAX
#define _PARAMETER_NOIRANGE         .int_min = 0, .int_max = 0
#define _PARAMETER_FRANGE(MIN, MAX) .float_min = MIN, .float_max = MAX
#define _PARAMETER_NOFRANGE         .float_min = 0.0, .float_max = 0.0

#define _PARAMETER_LIST(                                                         \
    LENGTH, LENGTH_MIN, LENGTH_MAX, LIST, SUBTYPE, ONADD, ONMOVE, ONREMOVE)      \
    .num_child_parameters = LENGTH, .num_child_parameters_min = LENGTH_MIN,      \
    .num_child_parameters_max =                                                  \
        (LENGTH_MAX == LENGTH_MIN && LENGTH_MAX == 0) ? UINT32_MAX : LENGTH_MAX, \
    .child_parameters = LIST, .sizeof_child = sizeof(SUBTYPE),                   \
    .list_length = list_length<SUBTYPE>, .list_add = list_add<SUBTYPE>,          \
    .list_move = list_move<SUBTYPE>, .list_remove = list_remove<SUBTYPE>,        \
    .on_list_add = ONADD, .on_list_move = ONMOVE, .on_list_remove = ONREMOVE
#define _PARAMETER_NOLIST                                                          \
    .num_child_parameters = 0, .num_child_parameters_min = 0,                      \
    .num_child_parameters_max = 0, .child_parameters = NULL, .sizeof_child = 1,    \
    .list_length = NULL, .list_add = NULL, .list_move = NULL, .list_remove = NULL, \
    .on_list_add = NULL, .on_list_move = NULL, .on_list_remove = NULL

#define _PARAM(SAVED, CONFIG, FIELD, TYPE, NAME, DESC, ONCHANGE)            \
    {                                                                       \
        _PARAMETER_BASIC(SAVED, CONFIG, FIELD, TYPE, NAME, DESC, ONCHANGE), \
            _PARAMETER_NOOPTIONS, _PARAMETER_NOIRANGE, _PARAMETER_NOFRANGE, \
            _PARAMETER_NOLIST                                               \
    }
#define PARAM(CONFIG, FIELD, TYPE, NAME, DESC, ONCHANGE) \
    _PARAM(/*saved*/ true, CONFIG, FIELD, TYPE, NAME, DESC, ONCHANGE)
#define PARAM_X(CONFIG, FIELD, TYPE, NAME, DESC, ONCHANGE) \
    _PARAM(/*saved*/ false, CONFIG, FIELD, TYPE, NAME, DESC, ONCHANGE)

#define _PARAM_IRANGE(SAVED, CONFIG, FIELD, NAME, DESC, ONCHANGE, MIN, MAX)         \
    {                                                                               \
        _PARAMETER_BASIC(SAVED, CONFIG, FIELD, INT, NAME, DESC, ONCHANGE),          \
            _PARAMETER_NOOPTIONS, _PARAMETER_IRANGE(MIN, MAX), _PARAMETER_NOFRANGE, \
            _PARAMETER_NOLIST                                                       \
    }
#define PARAM_IRANGE(CONFIG, FIELD, NAME, DESC, ONCHANGE, MIN, MAX) \
    _PARAM_IRANGE(true, CONFIG, FIELD, NAME, DESC, ONCHANGE, MIN, MAX)
#define PARAM_IRANGE_X(CONFIG, FIELD, NAME, DESC, ONCHANGE, MIN, MAX) \
    _PARAM_IRANGE(false, CONFIG, FIELD, NAME, DESC, ONCHANGE, MIN, MAX)

#define _PARAM_FRANGE(SAVED, CONFIG, FIELD, NAME, DESC, ONCHANGE, MIN, MAX)         \
    {                                                                               \
        _PARAMETER_BASIC(SAVED, CONFIG, FIELD, FLOAT, NAME, DESC, ONCHANGE),        \
            _PARAMETER_NOOPTIONS, _PARAMETER_NOIRANGE, _PARAMETER_FRANGE(MIN, MAX), \
            _PARAMETER_NOLIST                                                       \
    }
#define PARAM_FRANGE(CONFIG, FIELD, NAME, DESC, ONCHANGE, MIN, MAX) \
    _PARAM_FRANGE(true, CONFIG, FIELD, NAME, DESC, ONCHANGE, MIN, MAX)
#define PARAM_FRANGE_X(CONFIG, FIELD, NAME, DESC, ONCHANGE, MIN, MAX) \
    _PARAM_FRANGE(false, CONFIG, FIELD, NAME, DESC, ONCHANGE, MIN, MAX)

#define _PARAM_SELECT(SAVED, CONFIG, FIELD, NAME, DESC, ONCHANGE, GETOPTS)         \
    {                                                                              \
        _PARAMETER_BASIC(SAVED, CONFIG, FIELD, SELECT, NAME, DESC, ONCHANGE),      \
            _PARAMETER_OPTIONS(GETOPTS), _PARAMETER_NOIRANGE, _PARAMETER_NOFRANGE, \
            _PARAMETER_NOLIST                                                      \
    }
#define PARAM_SELECT(CONFIG, FIELD, NAME, DESC, ONCHANGE, GETOPTS) \
    _PARAM_SELECT(true, CONFIG, FIELD, NAME, DESC, ONCHANGE, GETOPTS)
#define PARAM_SELECT_X(CONFIG, FIELD, NAME, DESC, ONCHANGE, GETOPTS) \
    _PARAM_SELECT(false, CONFIG, FIELD, NAME, DESC, ONCHANGE, GETOPTS)

#define _PARAM_LIST(SAVED,                                                  \
                    CONFIG,                                                 \
                    FIELD,                                                  \
                    NAME,                                                   \
                    DESC,                                                   \
                    LENGTH,                                                 \
                    LENGTH_MIN,                                             \
                    LENGTH_MAX,                                             \
                    LIST_,                                                  \
                    SUBTYPE,                                                \
                    ONADD,                                                  \
                    ONMOVE,                                                 \
                    ONREMOVE)                                               \
    {                                                                       \
        _PARAMETER_BASIC(SAVED, CONFIG, FIELD, LIST, NAME, DESC, NULL),     \
            _PARAMETER_NOOPTIONS, _PARAMETER_NOIRANGE, _PARAMETER_NOFRANGE, \
            _PARAMETER_LIST(LENGTH,                                         \
                            LENGTH_MIN,                                     \
                            LENGTH_MAX,                                     \
                            LIST_,                                          \
                            SUBTYPE,                                        \
                            ONADD,                                          \
                            ONMOVE,                                         \
                            ONREMOVE)                                       \
    }
#define PARAM_LIST(CONFIG,      \
                   FIELD,       \
                   NAME,        \
                   DESC,        \
                   LENGTH,      \
                   LENGTH_MIN,  \
                   LENGTH_MAX,  \
                   LIST_,       \
                   SUBTYPE,     \
                   ONADD,       \
                   ONMOVE,      \
                   ONREMOVE)    \
    _PARAM_LIST(/*saved*/ true, \
                CONFIG,         \
                FIELD,          \
                NAME,           \
                DESC,           \
                LENGTH,         \
                LENGTH_MIN,     \
                LENGTH_MAX,     \
                LIST_,          \
                SUBTYPE,        \
                ONADD,          \
                ONMOVE,         \
                ONREMOVE)
#define PARAM_LIST_X(                                                           \
    CONFIG, FIELD, NAME, DESC, LENGTH, LIST_, SUBTYPE, ONADD, ONMOVE, ONREMOVE) \
    _PARAM_LIST(/*saved*/ false,                                                \
                CONFIG,                                                         \
                FIELD,                                                          \
                NAME,                                                           \
                DESC,                                                           \
                LENGTH,                                                         \
                LENGTH_MIN,                                                     \
                LENGTH_MAX,                                                     \
                LIST_,                                                          \
                SUBTYPE,                                                        \
                ONADD,                                                          \
                ONMOVE,                                                         \
                ONREMOVE)

#define PARAM_ACTION(NAME, DESC, ONRUN)                                                \
    {                                                                                  \
        .handle = Handles::comptime_get(#NAME #DESC #ONRUN), .type = AVS_PARAM_ACTION, \
        .name = NAME, .description = DESC, .is_saved = false, .offset = 0,             \
        .on_value_change = ONRUN, _PARAMETER_NOOPTIONS, _PARAMETER_NOIRANGE,           \
        _PARAMETER_NOFRANGE, _PARAMETER_NOLIST                                         \
    }

struct Config_Parameter {
    const Parameter* info;
    uint8_t* value_addr;
};

struct Effect_Info {
    virtual const char* get_group() const = 0;
    virtual const char* get_name() const = 0;
    virtual const char* get_help() const = 0;
    virtual uint32_t get_num_parameters() const = 0;
    virtual const Parameter* get_parameters() const = 0;
    virtual const AVS_Parameter_Handle* get_parameters_for_api() const = 0;
    virtual bool can_have_child_components() const { return false; };
    const Parameter* get_parameter_from_handle(AVS_Parameter_Handle to_find) {
        return this->_get_parameter_from_handle(
            this->get_num_parameters(), this->get_parameters(), to_find);
    }
    static const Parameter* _get_parameter_from_handle(uint32_t num_parameters,
                                                       const Parameter* parameters,
                                                       AVS_Parameter_Handle to_find) {
        for (uint32_t i = 0; i < num_parameters; i++) {
            if (parameters[i].handle == to_find) {
                return &parameters[i];
            }
            if (parameters[i].type == AVS_PARAM_LIST) {
                const Parameter* subtree_result =
                    Effect_Info::_get_parameter_from_handle(
                        parameters[i].num_child_parameters,
                        parameters[i].child_parameters,
                        to_find);
                if (subtree_result != NULL) {
                    return subtree_result;
                }
            }
        }
        return NULL;
    }
    /**
     * This is the heart of ugliness around parameter introspection. It makes use of
     * `sizeof` and `offsetof`, both recorded in each field's Parameter struct, to find
     * and return the data address for a specific parameter in the config struct.
     */
    static Config_Parameter get_config_parameter(Effect_Config& config,
                                                 AVS_Parameter_Handle parameter,
                                                 uint32_t num_params,
                                                 const Parameter* param_list,
                                                 std::vector<int64_t>& parameter_path,
                                                 uint32_t cur_depth) {
        for (uint32_t i = 0; i < num_params; i++) {
            uint8_t* config_data = (uint8_t*)&config;
            uint8_t* parameter_address = &config_data[param_list[i].offset];
            if (param_list[i].type == AVS_PARAM_LIST
                && cur_depth < parameter_path.size()) {
                auto list = (std::vector<Effect_Config>*)parameter_address;
                size_t list_size =
                    list->size() * sizeof(Effect_Config) / param_list[i].sizeof_child;
                int64_t clamped_size =
#if SIZE_MAX > INT64_MAX
                    (list_size > INT64_MAX ? INT64_MAX : (int64_t)list_size);
#else
                    (int64_t)list_size;
#endif
                int64_t list_index = parameter_path[cur_depth];
                if (list_index < 0) {
                    // wrap -1, -2, etc. around to offset-from-back
                    list_index += list_size;
                }
                if (list_index < 0 || list_index >= clamped_size) {
                    continue;
                }
                auto child_config =
                    (list->data() + list_index * param_list[i].sizeof_child);
                auto subtree_result = Effect_Info::get_config_parameter(
                    *child_config,
                    parameter,
                    param_list[i].num_child_parameters,
                    param_list[i].child_parameters,
                    parameter_path,
                    cur_depth + 1);
                if (subtree_result.info != NULL) {
                    return subtree_result;
                }
            }
            if (parameter == param_list[i].handle) {
                return Config_Parameter{&param_list[i], parameter_address};
            }
        }
        return {NULL, NULL};
    };

    /**
     * In the case of nested parameters, the child handles have to be returned as a
     * plain array in the API. But the handles are inside the Parameter's children
     * structs. So, upon first request, cache all child handles in a vector in the
     * global 'h_parameter_children' map, keyed by their parent parameter's handle.
     * Finally, return the plain C array from the vector.
     */
    const AVS_Parameter_Handle* get_parameter_children_for_api(
        const Parameter* parameter,
        bool reset = false) const {
        if (parameter->type != AVS_PARAM_LIST) {
            return NULL;
        }
        if (h_parameter_children.find(parameter->handle) == h_parameter_children.end()
            || reset) {
            h_parameter_children[parameter->handle].clear();
            for (uint32_t i = 0; i < parameter->num_child_parameters; i++) {
                h_parameter_children[parameter->handle].push_back(
                    parameter->child_parameters[i].handle);
            }
        }
        return h_parameter_children[parameter->handle].data();
    };
};

/**
 * Every struct inheriting from Effect_Info should add this macro to the bottom of the
 * effect's info-struct declaration. This macro defines all the accessor methods to the
 * static members an Effect_Info subclass should define.
 * We need this construct because static members in the child class cannot be accessed
 * by automatically inherited virtual functions. So we need to explicitly override each
 * one in each child class.
 */
#define EFFECT_INFO_GETTERS                                                       \
    virtual const char* get_group() const { return this->group; };                \
    virtual const char* get_name() const { return this->name; };                  \
    virtual const char* get_help() const { return this->help; };                  \
    virtual uint32_t get_num_parameters() const { return this->num_parameters; }; \
    virtual const Parameter* get_parameters() const { return this->parameters; }; \
    virtual const AVS_Parameter_Handle* get_parameters_for_api() const {          \
        static AVS_Parameter_Handle parameters_for_api[this->num_parameters];     \
        for (uint32_t i = 0; i < this->num_parameters; i++) {                     \
            parameters_for_api[i] = this->parameters[i].handle;                   \
        }                                                                         \
        return parameters_for_api;                                                \
    };

/**
 * This one is only useful for Root and Effect List. All other effects simply inherit
 * the `can_have_child_components()` method (which always returns `false`) from the
 * `Effect_Info` base.
 */
#define EFFECT_INFO_GETTERS_WITH_CHILDREN \
    EFFECT_INFO_GETTERS;                  \
    virtual bool can_have_child_components() const { return true; }

/**
 * This is needed by the `Configurable_Effect` template class, but it's here since it
 * fits the context better, and effect.h shouldn't be _too_ messy.
 *
 * Every parameter type needs a `get()`, `set()` and `zero()` method. By explicitly
 * specializing a template struct for this collection of methods, but only for each of
 * the allowed scalar parameter types, the correct value and return types for these
 * methods can be checked in `Configurable_Effect`'s `get()` and `set()` at compile
 * time.
 */
template <typename T>
struct parameter_dispatch;

template <>
struct parameter_dispatch<bool> {
    static bool get(uint8_t* config_data) { return *(bool*)config_data; };
    static void set(Config_Parameter config_param, bool value) {
        *(bool*)config_param.value_addr = value;
    }
    static bool zero() { return false; };
};

template <>
struct parameter_dispatch<int64_t> {
    static int64_t get(uint8_t* config_data) { return *((int64_t*)config_data); };
    static void set(const Config_Parameter& config_param, int64_t value) {
        auto _min = config_param.info->int_min;
        auto _max = config_param.info->int_max;
        if (_min != _max) {
            value = value < _min ? _min : value > _max ? _max : value;
        }
        *(int64_t*)config_param.value_addr = value;
    }
    static int64_t zero() { return 0; };
};

template <>
struct parameter_dispatch<double> {
    static double get(uint8_t* config_data) { return *(double*)config_data; };
    static void set(const Config_Parameter& config_param, double value) {
        auto _min = config_param.info->float_min;
        auto _max = config_param.info->float_max;
        if (_min != _max) {
            value = value < _min ? _min : value > _max ? _max : value;
        }
        *(double*)config_param.value_addr = value;
    }
    static double zero() { return 0.0; };
};

template <>
struct parameter_dispatch<uint64_t> {
    static uint64_t get(uint8_t* config_data) { return *((uint64_t*)config_data); };
    static void set(const Config_Parameter& config_param, uint64_t value) {
        *(uint64_t*)config_param.value_addr = value;
    }
    static uint64_t zero() { return 0; };
};

template <>
struct parameter_dispatch<const char*> {
    static const char* get(uint8_t* config_data) {
        return ((std::string*)config_data)->c_str();
    };
    static void set(const Config_Parameter& config_param, const char* value) {
        *(std::string*)config_param.value_addr = value;
    };

    static const char* zero() { return ""; };
};