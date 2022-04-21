#include "e_texer2.h"
#include "e_texer2_macros.h"

#include "r_defs.h"

#include "avs_eelif.h"
#include "files.h"
#include "image.h"
#include "pixel_format.h"

#include "../util.h"

#include <math.h>
#include <stdio.h>

constexpr Texer2_Example Texer2_Info::examples[];
constexpr Parameter Texer2_Info::parameters[];

std::vector<std::string> E_Texer2::filenames;
const char** E_Texer2::c_filenames;

void Texer2_Info::on_file_change(Effect* component,
                                 const Parameter*,
                                 std::vector<int64_t>) {
    E_Texer2* texer2 = (E_Texer2*)component;
    texer2->load_image();
}

void Texer2_Info::recompile(Effect* component,
                            const Parameter* parameter,
                            std::vector<int64_t>) {
    auto texer2 = (E_Texer2*)component;
    if (std::string("Init") == parameter->name) {
        texer2->code_init.need_recompile = true;
    } else if (std::string("Frame") == parameter->name) {
        texer2->code_frame.need_recompile = true;
    } else if (std::string("Beat") == parameter->name) {
        texer2->code_beat.need_recompile = true;
    } else if (std::string("Point") == parameter->name) {
        texer2->code_point.need_recompile = true;
    }
    texer2->recompile_if_needed();
}

void Texer2_Info::load_example(Effect* component,
                               const Parameter*,
                               std::vector<int64_t>) {
    auto texer2 = ((E_Texer2*)component);
    const Texer2_Example& to_load = texer2->info.examples[texer2->config.example];
    texer2->config.init = to_load.init;
    texer2->config.frame = to_load.frame;
    texer2->config.beat = to_load.beat;
    texer2->config.point = to_load.point;
    texer2->need_full_recompile();
    texer2->config.resize = to_load.resize;
    texer2->config.wrap = to_load.wrap;
    texer2->config.colorize = to_load.colorize;
}

const char* const* Texer2_Info::image_files(int64_t* length_out) {
    *length_out = E_Texer2::filenames.size();
    auto new_list = (const char**)malloc(E_Texer2::filenames.size() * sizeof(char*));
    for (size_t i = 0; i < E_Texer2::filenames.size(); i++) {
        new_list[i] = E_Texer2::filenames[i].c_str();
    }
    auto old_list = E_Texer2::c_filenames;
    E_Texer2::c_filenames = new_list;
    free(old_list);
    return E_Texer2::c_filenames;
}

Texer2_Config::Texer2_Config()
    : image(0),
      resize(Texer2_Info::examples[0].resize),
      wrap(Texer2_Info::examples[0].wrap),
      colorize(Texer2_Info::examples[0].colorize),
      init(Texer2_Info::examples[0].init),
      frame(Texer2_Info::examples[0].frame),
      beat(Texer2_Info::examples[0].beat),
      point(Texer2_Info::examples[0].point) {}

E_Texer2::E_Texer2()
    : iw(0),
      ih(0),
      image_normal(NULL),
      image_flipped(NULL),
      image_mirrored(NULL),
      image_rot180(NULL),
      image_lock(lock_init()) {
    this->config.init.assign(this->info.examples[0].init);
    this->config.frame.assign(this->info.examples[0].frame);
    this->config.beat.assign(this->info.examples[0].beat);
    this->config.point.assign(this->info.examples[0].point);
    this->need_full_recompile();
    this->find_image_files();
    this->load_image();
}

E_Texer2::~E_Texer2() {
    lock_lock(this->image_lock);
    if (this->image_normal) {
        this->delete_image();
    }
    lock_unlock(this->image_lock);
    lock_destroy(this->image_lock);
}

static void add_file_callback(const char* file, void* data) {
    ((E_Texer2*)data)->filenames.push_back(file);
}

void E_Texer2::find_image_files() {
    this->clear_image_files();
    this->filenames.push_back("(default image)");
    const int num_extensions = 5;
    char* extensions[num_extensions] = {".bmp", ".png", ".jpg", ".jpeg", ".gif"};
    find_files_by_extensions(g_path,
                             extensions,
                             num_extensions,
                             add_file_callback,
                             this,
                             sizeof(*this),
                             /*recursive*/ false,
                             /*background*/ false);
}

void E_Texer2::clear_image_files() { this->filenames.clear(); }

void E_Texer2::delete_image() {
    if (this->image_normal) {
        iw = 0;
        ih = 0;
        delete this->image_normal;
        delete this->image_flipped;
        delete this->image_mirrored;
        delete this->image_rot180;
    }
}

extern unsigned char rawData[1323];  // example pic
void E_Texer2::load_image() {
    lock_lock(this->image_lock);
    if (this->image_normal) this->delete_image();
    if (this->config.image == 0) {
        this->load_default_image();
        lock_unlock(this->image_lock);
        return;
    }
    char filename[MAX_PATH];
    int printed = snprintf(
        filename, MAX_PATH, "%s\\%s", g_path, this->filenames[config.image].c_str());
    if (printed >= MAX_PATH) {
        filename[MAX_PATH - 1] = '\0';
    }
    AVS_Image* tmp_image = image_load(filename);
    if (tmp_image->data == NULL || tmp_image->w == 0 || tmp_image->h == 0
        || tmp_image->error != NULL) {
        image_free(tmp_image);
        this->load_default_image();
        lock_unlock(this->image_lock);
        return;
    }
    this->iw = tmp_image->w;
    this->ih = tmp_image->h;
    int image_size = this->ih * this->iw;
    this->image_normal = new pixel_rgb0_8[image_size];
    this->image_flipped = new pixel_rgb0_8[image_size];
    this->image_mirrored = new pixel_rgb0_8[image_size];
    this->image_rot180 = new pixel_rgb0_8[image_size];
    memcpy(this->image_normal, tmp_image->data, image_size * sizeof(pixel_rgb0_8));
    int y = 0;
    while (y < ih) {
        int x = 0;
        while (x < iw) {
            pixel_rgb0_8 value = this->image_normal[y * iw + x];
            this->image_flipped[(ih - y - 1) * iw + x] = value;
            this->image_mirrored[(y + 1) * iw - x - 1] = value;
            this->image_rot180[(ih - y) * iw - x - 1] = value;
            x++;
        }
        y++;
    }
    lock_unlock(this->image_lock);
    image_free(tmp_image);
}

void E_Texer2::load_default_image() {
    this->iw = 21;
    this->ih = 21;
    this->image_normal = new pixel_rgb0_8[this->iw * this->ih];
    this->image_flipped = new pixel_rgb0_8[this->iw * this->ih];
    this->image_mirrored = new pixel_rgb0_8[this->iw * this->ih];
    this->image_rot180 = new pixel_rgb0_8[this->iw * this->ih];
    for (int i = 0; i < this->iw * this->ih; ++i) {
        // the default image is symmetrical in all directions
        image_normal[i] = *(int*)&rawData[i * 3];
        image_flipped[i] = *(int*)&rawData[i * 3];
        image_mirrored[i] = *(int*)&rawData[i * 3];
        image_rot180[i] = *(int*)&rawData[i * 3];
    }
}

struct RECT {
    int left;
    int top;
    int right;
    int bottom;
};

struct RECTf {
    double left;
    double top;
    double right;
    double bottom;
};

void E_Texer2::DrawParticle(int* framebuffer,
                            pixel_rgb0_8* texture,
                            int w,
                            int h,
                            double x,
                            double y,
                            double sizex,
                            double sizey,
                            unsigned int color) {
    // Adjust width/height
    --w;
    --h;
    --(this->iw);  // member vars, restore later!
    --(this->ih);

    // Texture Coordinates
    double x0 = 0.0;
    double y0 = 0.0;

    /***************************************************************************/
    /***************************************************************************/
    /*   Scaling renderer                                                      */
    /***************************************************************************/
    /***************************************************************************/
    if (this->config.resize) {
        RECTf r;
        // Determine area rectangle,
        // correct with half pixel for correct pixel coverage
        r.left = -iw * .5 * sizex + 0.5 + (x * .5 + .5) * w;
        r.top = -ih * .5 * sizey + 0.5 + (y * .5 + .5) * h;
        r.right = (iw - 1) * .5 * sizex + 0.5 + (x * .5 + .5) * w;
        r.bottom = (ih - 1) * .5 * sizey + 0.5 + (y * .5 + .5) * h;

        RECT r2;
        r2.left = RoundToInt(r.left);
        r2.top = RoundToInt(r.top);
        r2.right = RoundToInt(r.right);
        r2.bottom = RoundToInt(r.bottom);

        // Visiblity culling
        if ((r2.right < 0.0f) || (r2.left > w) || (r2.bottom < 0.0f) || (r2.top > h)) {
            goto skippart;
        }

        // Subpixel adjustment for first texel
        x0 = (0.5 - Fractional(r.left + 0.5)) / (r.right - r.left);
        y0 = (0.5 - Fractional(r.top + 0.5)) / (r.bottom - r.top);

        // Window edge clipping
        if (r.left < 0.0f) {
            x0 = -r.left / (r.right - r.left);
            r2.left = 0;
        }
        if (r.top < 0.0f) {
            y0 = -r.top / (r.bottom - r.top);
            r2.top = 0;
        }
        if (r.right > w) {
            r2.right = w;
        }
        if (r.bottom > h) {
            r2.bottom = h;
        }

        {
            double fx0 = x0 * iw;
            double fy0 = y0 * ih;

            int cx0 = RoundToInt(fx0);
            int cy0 = RoundToInt(fy0);

            // fixed point fractional part of first coordinate
            int dx = 65535 - FloorToInt((.5f - (fx0 - cx0)) * 65536.0);
            int dy = 65535 - FloorToInt((.5f - (fy0 - cy0)) * 65536.0);

            // 'texel per pixel' steps
            double scx = (iw - 1) / (r.right - r.left + 1);
            double scy = (ih - 1) / (r.bottom - r.top + 1);
            int sdx = FloorToInt(scx * 65536.0);
            int sdy = FloorToInt(scy * 65536.0);

            // fixed point corrected coordinate
            cx0 = (cx0 << 16) + dx;
            cy0 = (cy0 << 16) + dy;

            if (cx0 < 0) {
                cx0 += sdx;
                ++r2.left;
            }
            if (cy0 < 0) {
                cy0 += sdy;
                ++r2.top;
            }

            // Cull subpixel sized particles
            if ((r2.right <= r2.left) || (r2.bottom <= r2.top)) {
                goto skippart;
            }

            // Prepare filter color
            T2_PREP_MASK_COLOR

            switch (g_line_blend_mode & 0xFF) {
                case BLEND_REPLACE: {
                    __int64 mmxxor = 0x00FF00FF00FF00FF;
                    int tot = r2.right - r2.left;
                    int* outp = &framebuffer[r2.top * (w + 1) + r2.left];
                    for (int y = r2.top; y <= r2.bottom; ++y) {
                        T2_SCALE_BLEND_ASM_ENTER(t2_scale_loop_rep)
#ifdef _MSC_VER
                        __asm {
                            psrlw mm0, 8
                            packuswb mm0, mm0
                        }
#else  // GCC
                        "psrlw     %%mm0, 8\n\t"
                        "packuswb  %%mm0, %%mm0\n\t"
#endif
                        T2_SCALE_BLEND_ASM_LEAVE(t2_scale_loop_rep)
                        cy0 += sdy;
                        outp += w + 1;
                    }
                    break;
                }

                case BLEND_ADDITIVE: {
                    __int64 mmxxor = 0x00FF00FF00FF00FF;
                    int tot = r2.right - r2.left;
                    int* outp = &framebuffer[r2.top * (w + 1) + r2.left];
                    for (int y = r2.top; y <= r2.bottom; ++y) {
                        T2_SCALE_BLEND_ASM_ENTER(t2_scale_loop_add)
#ifdef _MSC_VER
                        __asm {
                            psrlw mm0, 8
                            packuswb mm0, mm0

                                // save
                            paddusb mm0, qword ptr [edi]
                        }
#else  // GCC
                        "psrlw      %%mm0, 8\n\t"
                        "packuswb   %%mm0, %%mm0\n\t"
                        "paddusb    %%mm0, qword ptr [%%edi]\n\t"
#endif
                        T2_SCALE_BLEND_ASM_LEAVE(t2_scale_loop_add)
                        cy0 += sdy;
                        outp += w + 1;
                    }
                    break;
                }

                case BLEND_MAXIMUM: {
                    int signmask = 0x808080;
                    T2_SCALE_MINMAX_SIGNMASK
                    __int64 mmxxor = 0x00FF00FF00FF00FF;
                    int tot = r2.right - r2.left;
                    int* outp = &framebuffer[r2.top * (w + 1) + r2.left];
                    for (int y = r2.top; y <= r2.bottom; ++y) {
                        T2_SCALE_BLEND_ASM_ENTER(t2_scale_loop_max)
#ifdef _MSC_VER
                        __asm {
                            // movd mm6, signmask;
                            psrlw mm0, 8
                            packuswb mm0, mm0

                                // save
                            movd mm1, dword ptr [edi]
                            pxor mm0, mm6
                            mov eax, 0xFFFFFF
                            pxor mm1, mm6
                            movd mm5, eax
                            movq mm2, mm1
                            pcmpgtb mm2, mm0
                            movq mm3, mm2
                            pxor mm3, mm5
                            pxor mm5, mm5
                            pand mm0, mm3
                            pand mm1, mm2
                            por mm0, mm1
                            pxor mm0, mm6
                        }
#else  // GCC
                        /*"movd      %%mm6, %[signmask]\n\t"*/
                        "psrlw     %%mm0, 8\n\t"
                        "packuswb  %%mm0, %%mm0\n\t"

                        // save
                        "movd      %%mm1, dword ptr [%%edi]\n\t"
                        "pxor      %%mm0, %%mm6\n\t"
                        "mov       %%eax, 0xFFFFFF\n\t"
                        "pxor      %%mm1, %%mm6\n\t"
                        "movd      %%mm5, %%eax\n\t"
                        "movq      %%mm2, %%mm1\n\t"
                        "pcmpgtb   %%mm2, %%mm0\n\t"
                        "movq      %%mm3, %%mm2\n\t"
                        "pxor      %%mm3, %%mm5\n\t"
                        "pxor      %%mm5, %%mm5\n\t"
                        "pand      %%mm0, %%mm3\n\t"
                        "pand      %%mm1, %%mm2\n\t"
                        "por       %%mm0, %%mm1\n\t"
                        "pxor      %%mm0, %%mm6\n\t"
#endif
                        T2_SCALE_BLEND_ASM_LEAVE(t2_scale_loop_max)
                        cy0 += sdy;
                        outp += w + 1;
                    }
                    break;
                }

                case BLEND_5050: {
                    __int64 mmxxor = 0x00FF00FF00FF00FF;
                    int tot = r2.right - r2.left;
                    int* outp = &framebuffer[r2.top * (w + 1) + r2.left];
                    for (int y = r2.top; y <= r2.bottom; ++y) {
                        T2_SCALE_BLEND_ASM_ENTER(t2_scale_loop_50)
#ifdef _MSC_VER
                        __asm {

                            // save
                            movd mm1, dword ptr [edi]
                            psrlw mm0, 8
                            punpcklbw mm1, mm5
                            paddusw mm0, mm1
                            psrlw mm0, 1
                            packuswb mm0, mm0
                        }
#else  // GCC
                        "movd       %%mm1, dword ptr [%%edi]\n\t"
                        "psrlw      %%mm0, 8\n\t"
                        "punpcklbw  %%mm1, %%mm5\n\t"
                        "paddusw    %%mm0, %%mm1\n\t"
                        "psrlw      %%mm0, 1\n\t"
                        "packuswb   %%mm0, %%mm0\n\t"
#endif
                        T2_SCALE_BLEND_ASM_LEAVE(t2_scale_loop_50)
                        cy0 += sdy;
                        outp += w + 1;
                    }
                    break;
                }

                case BLEND_SUB1: {
                    __int64 mmxxor = 0x00FF00FF00FF00FF;
                    int tot = r2.right - r2.left;
                    int* outp = &framebuffer[r2.top * (w + 1) + r2.left];
                    for (int y = r2.top; y <= r2.bottom; ++y) {
                        T2_SCALE_BLEND_ASM_ENTER(t2_scale_loop_sub1)
#ifdef _MSC_VER
                        __asm {
                            psrlw mm0, 8
                            packuswb mm0, mm0

                                // save
                            movd mm1, dword ptr [edi]
                            psubusb mm1, mm0
                            movq mm0, mm1
                        }
#else  // GCC
                        "psrlw %%mm0, 8\n\t"
                        "packuswb %%mm0, %%mm0\n\t"

                        // save
                        "movd %%mm1, dword ptr [%%edi]\n\t"
                        "psubusb %%mm1, %%mm0\n\t"
                        "movq %%mm0, %%mm1\n\t"
#endif
                        T2_SCALE_BLEND_ASM_LEAVE(t2_scale_loop_sub1)
                        cy0 += sdy;
                        outp += w + 1;
                    }
                    break;
                }

                case BLEND_SUB2: {
                    __int64 mmxxor = 0x00FF00FF00FF00FF;
                    int tot = r2.right - r2.left;
                    int* outp = &framebuffer[r2.top * (w + 1) + r2.left];
                    for (int y = r2.top; y <= r2.bottom; ++y) {
                        T2_SCALE_BLEND_ASM_ENTER(t2_scale_loop_sub2)
#ifdef _MSC_VER
                        __asm {
                            psrlw mm0, 8
                            packuswb mm0, mm0

                                // save
                            psubusb mm0, qword ptr [edi]
                        }
#else  // GCC
                        "psrlw %%mm0, 8\n\t"
                        "packuswb %%mm0, %%mm0\n\t"

                        // save
                        "psubusb %%mm0, qword ptr [%%edi]\n\t"
#endif
                        T2_SCALE_BLEND_ASM_LEAVE(t2_scale_loop_sub2)
                        cy0 += sdy;
                        outp += w + 1;
                    }
                    break;
                }

                case BLEND_MULTIPLY: {
                    __int64 mmxxor = 0x00FF00FF00FF00FF;
                    int tot = r2.right - r2.left;
                    int* outp = &framebuffer[r2.top * (w + 1) + r2.left];
                    for (int y = r2.top; y <= r2.bottom; ++y) {
                        T2_SCALE_BLEND_ASM_ENTER(t2_scale_loop_mul)
#ifdef _MSC_VER
                        __asm {
                            psrlw mm0, 8

                            // save
                            movd mm1, dword ptr [edi]
                            punpcklbw mm1, mm5
                            pmullw mm1, mm0
                            psrlw mm1, 8
                            packuswb mm1, mm1
                        }
#else  // GCC
                        "psrlw %%mm0, 8\n\t"

                        // save
                        "movd %%mm1, dword ptr [%%edi]\n\t"
                        "punpcklbw %%mm1, %%mm5\n\t"
                        "pmullw %%mm1, %%mm0\n\t"
                        "psrlw %%mm1, 8\n\t"
                        "packuswb %%mm1, %%mm1\n\t"
#endif
                        T2_SCALE_BLEND_ASM_LEAVE(t2_scale_loop_mul)
                        cy0 += sdy;
                        outp += w + 1;
                    }
                    break;
                }

                case BLEND_ADJUSTABLE: {
                    __int64 alphavalue = 0x0;
                    __int64* alpha = &alphavalue;
                    int t = (g_line_blend_mode & 0xFF00) >> 8;
                    __int64 mmxxor = 0x00FF00FF00FF00FF;
                    T2_SCALE_BLEND_AND_STORE_ALPHA
                    int tot = r2.right - r2.left;
                    int* outp = &framebuffer[r2.top * (w + 1) + r2.left];
                    for (int y = r2.top; y <= r2.bottom; ++y) {
                        T2_SCALE_BLEND_ASM_ENTER(t2_scale_loop_adj)
#ifdef _MSC_VER
                        __asm {
                            psrlw mm0, 8

                            // Merged filter/alpha
                            // save
                            movd mm1, dword ptr [edi]
                            punpcklbw mm1, mm5
                            pmullw mm1, mm6
                            psrlw mm1, 8
                            pmullw mm0, alpha
                            psrlw mm0, 8
                            paddusw mm0, mm1
                            packuswb mm0, mm0
                        }
#else  // GCC
                        "psrlw      %%mm0, 8\n\t"

                        // Merged filter/alpha
                        // save
                        "movd       %%mm1, dword ptr [%%edi]\n\t"
                        "punpcklbw  %%mm1, %%mm5\n\t"
                        "pmullw     %%mm1, %%mm6\n\t"
                        "psrlw      %%mm1, 8\n\t"
                        "pmullw     %%mm0, qword ptr %[alpha]\n\t"
                        "psrlw      %%mm0, 8\n\t"
                        "paddusw    %%mm0, %%mm1\n\t"
                        "packuswb   %%mm0, %%mm0\n\t"
#endif
                        T2_SCALE_BLEND_ASM_LEAVE(t2_scale_loop_adj, ASM_M_ARG(alpha))
                        cy0 += sdy;
                        outp += w + 1;
                    }
                    break;
                }

                case BLEND_XOR: {
                    __int64 mmxxor = 0x00FF00FF00FF00FF;
                    int tot = r2.right - r2.left;
                    int* outp = &framebuffer[r2.top * (w + 1) + r2.left];
                    for (int y = r2.top; y <= r2.bottom; ++y) {
                        T2_SCALE_BLEND_ASM_ENTER(t2_scale_loop_xor)
#ifdef _MSC_VER
                        __asm {
                            psrlw mm0, 8
                            packuswb mm0, mm0
                            pxor mm0, qword ptr [edi]
                        }
#else  // GCC
                        "psrlw  %%mm0, 8\n\t"
                        "packuswb %%mm0, %%mm0\n\t"
                        "pxor   %%mm0, qword ptr [%%edi]\n\t"
#endif
                        T2_SCALE_BLEND_ASM_LEAVE(t2_scale_loop_xor)
                        cy0 += sdy;
                        outp += w + 1;
                    }
                    break;
                }

                case BLEND_MINIMUM: {
                    int signmask = 0x808080;
                    __int64 mmxxor = 0x00FF00FF00FF00FF;
                    int tot = r2.right - r2.left;
                    int* outp = &framebuffer[r2.top * (w + 1) + r2.left];
                    for (int y = r2.top; y <= r2.bottom; ++y) {
                        T2_SCALE_BLEND_ASM_ENTER(t2_scale_loop_min)
#ifdef _MSC_VER
                        __asm {
                            movd mm6, signmask
                            psrlw mm0, 8
                            packuswb mm0, mm0

                                // save
                            movd mm1, dword ptr [edi]
                            pxor mm0, mm6
                            mov eax, 0xFFFFFF
                            pxor mm1, mm6
                            movd mm5, eax
                            movq mm2, mm1
                            pcmpgtb mm2, mm0
                            movq mm3, mm2
                            pxor mm3, mm5
                            pxor mm5, mm5
                            pand mm0, mm2
                            pand mm1, mm3
                            por mm0, mm1
                            pxor mm0, mm6
                        }
#else  // GCC
                        "movd      %%mm6, %[signmask]\n\t"
                        "psrlw     %%mm0, 8\n\t"
                        "packuswb  %%mm0, %%mm0\n\t"

                        // save
                        "movd      %%mm1, dword ptr [%%edi]\n\t"
                        "pxor      %%mm0, %%mm6\n\t"
                        "mov       %%eax, 0xFFFFFF\n\t"
                        "pxor      %%mm1, %%mm6\n\t"
                        "movd      %%mm5, %%eax\n\t"
                        "movq      %%mm2, %%mm1\n\t"
                        "pcmpgtb   %%mm2, %%mm0\n\t"
                        "movq      %%mm3, %%mm2\n\t"
                        "pxor      %%mm3, %%mm5\n\t"
                        "pxor      %%mm5, %%mm5\n\t"
                        "pand      %%mm0, %%mm2\n\t"
                        "pand      %%mm1, %%mm3\n\t"
                        "por       %%mm0, %%mm1\n\t"
                        "pxor      %%mm0, %%mm6\n\t"
#endif
                        T2_SCALE_BLEND_ASM_LEAVE(t2_scale_loop_min, ASM_M_ARG(signmask))
                        cy0 += sdy;
                        outp += w + 1;
                    }
                    break;
                }
            }
        }
    } else {
        /***************************************************************************/
        /***************************************************************************/
        /*   Non-scaling renderer                                                  */
        /***************************************************************************/
        /***************************************************************************/

        RECT r;
        // Determine exact position, original size
        r.left = (RoundToInt((x * .5f + .5f) * w) - iw / 2);
        r.top = (RoundToInt((y * .5f + .5f) * h) - ih / 2);
        r.right = r.left + iw - 1;
        r.bottom = r.top + ih - 1;

        RECT r2;
        memcpy(&r2, &r, sizeof(RECT));

        // Visiblity culling
        if ((r2.right < 0) || (r2.left > w) || (r2.bottom < 0) || (r2.top > h)) {
            goto skippart;
        }

        // Window edge clipping
        if (r.left < 0) {
            x0 = -((double)r.left) / (double)(r.right - r.left);
            r2.left = 0;
        }
        if (r.top < 0) {
            y0 = -((double)r.top) / (double)(r.bottom - r.top);
            r2.top = 0;
        }
        if (r.right > w) {
            r2.right = w;
        }
        if (r.bottom > h) {
            r2.bottom = h;
        }

        if ((r2.right <= r2.left) || (r2.bottom <= r2.top)) {
            goto skippart;
        }

        int cx0 = RoundToInt(x0 * iw);
        int cy0 = RoundToInt(y0 * ih);

        int ty = cy0;

        if (this->config.colorize) {
            // Second easiest path, masking, but no scaling
            T2_PREP_MASK_COLOR
            switch (g_line_blend_mode & 0xFF) {
                case BLEND_REPLACE: {
                    T2_NONSCALE_PUSH_ESI_EDI
                    for (int y = r2.top; y <= r2.bottom; ++y) {
                        int* outp = &framebuffer[y * (w + 1) + r2.left];
                        pixel_rgb0_8* inp = &texture[ty * (iw + 1) + cx0];
                        int tot = (r2.right - r2.left);
                        T2_NONSCALE_BLEND_ASM_ENTER(t2_nonscale_mask_loop_rep)
#ifdef _MSC_VER
                        __asm {
                            movd mm0, dword ptr [esi]

                            punpcklbw mm0, mm5
                            pmullw mm0, mm7
                            psrlw mm0, 8
                            packuswb mm0, mm0

                            movd dword ptr [edi], mm0
                        }
#else  // GCC
                        "movd       %%mm0, dword ptr [%%esi]\n\t"

                            "punpcklbw  %%mm0, %%mm5\n\t"
                            "pmullw     %%mm0, %%mm7\n\t"
                            "psrlw      %%mm0, 8\n\t"
                            "packuswb   %%mm0, %%mm0\n\t"

                            "movd       dword ptr [%%edi], %%mm0\n\t"
#endif
                        T2_NONSCALE_BLEND_ASM_LEAVE(t2_nonscale_mask_loop_rep)
                        ++ty;
                    }
                    T2_NONSCALE_POP_EDI_ESI
                    break;
                }

                case BLEND_ADDITIVE: {
                    T2_NONSCALE_PUSH_ESI_EDI
                    for (int y = r2.top; y <= r2.bottom; ++y) {
                        int* outp = &framebuffer[y * (w + 1) + r2.left];
                        pixel_rgb0_8* inp = &texture[ty * (iw + 1) + cx0];
                        int tot = (r2.right - r2.left);
                        T2_NONSCALE_BLEND_ASM_ENTER(t2_nonscale_mask_loop_add)
#ifdef _MSC_VER
                        __asm {
                            movd mm0, dword ptr [edi]
                            movd mm1, dword ptr [esi]

                            punpcklbw mm1, mm5
                            pmullw mm1, mm7
                            psrlw mm1, 8
                            packuswb mm1, mm1

                            paddusb mm0, mm1
                            movd dword ptr [edi], mm0
                        }
#else  // GCC
                        "movd       %%mm0, dword ptr [%%edi]\n\t"
                            "movd       %%mm1, dword ptr [%%esi]\n\t"

                            "punpcklbw  %%mm1, %%mm5\n\t"
                            "pmullw     %%mm1, %%mm7\n\t"
                            "psrlw      %%mm1, 8\n\t"
                            "packuswb   %%mm1, %%mm1\n\t"

                            "paddusb    %%mm0, %%mm1\n\t"
                            "movd       dword ptr [%%edi], %%mm0\n\t"
#endif
                        T2_NONSCALE_BLEND_ASM_LEAVE(t2_nonscale_mask_loop_add)
                        ++ty;
                    }
                    T2_NONSCALE_POP_EDI_ESI
                    break;
                }

                case BLEND_MAXIMUM: {
                    int maxmask = 0xFFFFFF;   // -> mm4
                    int signmask = 0x808080;  // -> mm6
                    T2_NONSCALE_PUSH_ESI_EDI
                    T2_NONSCALE_MINMAX_MASKS
                    for (int y = r2.top; y <= r2.bottom; ++y) {
                        int* outp = &framebuffer[y * (w + 1) + r2.left];
                        pixel_rgb0_8* inp = &texture[ty * (iw + 1) + cx0];
                        int tot = (r2.right - r2.left);
                        T2_NONSCALE_BLEND_ASM_ENTER(t2_nonscale_mask_loop_max)
#ifdef _MSC_VER
                        __asm {
                            movd mm0, dword ptr [edi]
                            movd mm1, dword ptr [esi]

                            punpcklbw mm1, mm5
                            pmullw mm1, mm7
                            psrlw mm1, 8
                            packuswb mm1, mm1

                            pxor mm0, mm6
                            pxor mm1, mm6
                            movq mm2, mm1
                            pcmpgtb mm2, mm0
                            movq mm3, mm2
                            pxor mm3, mm4
                            pand mm0, mm3
                            pand mm1, mm2
                            por mm0, mm1
                            pxor mm0, mm6
                            movd dword ptr [edi], mm0
                        }
#else  // GCC
                        "movd       %%mm0, dword ptr [%%edi]\n\t"
                            "movd       %%mm1, dword ptr [%%esi]\n\t"

                            "punpcklbw  %%mm1, %%mm5\n\t"
                            "pmullw     %%mm1, %%mm7\n\t"
                            "psrlw      %%mm1, 8\n\t"
                            "packuswb   %%mm1, %%mm1\n\t"

                            "pxor       %%mm0, %%mm6\n\t"
                            "pxor       %%mm1, %%mm6\n\t"
                            "movq       %%mm2, %%mm1\n\t"
                            "pcmpgtb    %%mm2, %%mm0\n\t"
                            "movq       %%mm3, %%mm2\n\t"
                            "pxor       %%mm3, %%mm4\n\t"
                            "pand       %%mm0, %%mm3\n\t"
                            "pand       %%mm1, %%mm2\n\t"
                            "por        %%mm0, %%mm1\n\t"
                            "pxor       %%mm0, %%mm6\n\t"
                            "movd       dword ptr [%%edi], %%mm0\n\t"
#endif
                        T2_NONSCALE_BLEND_ASM_LEAVE(t2_nonscale_mask_loop_max)
                        ++ty;
                    }
                    T2_NONSCALE_POP_EDI_ESI
                    break;
                }

                case BLEND_5050: {
                    T2_NONSCALE_PUSH_ESI_EDI
                    for (int y = r2.top; y <= r2.bottom; ++y) {
                        int* outp = &framebuffer[y * (w + 1) + r2.left];
                        pixel_rgb0_8* inp = &texture[ty * (iw + 1) + cx0];
                        int tot = (r2.right - r2.left);
                        T2_NONSCALE_BLEND_ASM_ENTER(t2_nonscale_mask_loop_50)
#ifdef _MSC_VER
                        __asm {
                            movd mm0, dword ptr [edi]
                            movd mm1, dword ptr [esi]

                            punpcklbw mm1, mm5
                            pmullw mm1, mm7
                            psrlw mm1, 8
                            packuswb mm1, mm1

                            punpcklbw mm0, mm5
                            punpcklbw mm1, mm5
                            paddusw mm0, mm1
                            psrlw mm0, 1
                            packuswb mm0, mm0
                            movd dword ptr [edi], mm0
                        }
#else  // GCC
                        "movd       %%mm0, dword ptr [%%edi]\n\t"
                            "movd       %%mm1, dword ptr [%%esi]\n\t"

                            "punpcklbw  %%mm1, %%mm5\n\t"
                            "pmullw     %%mm1, %%mm7\n\t"
                            "psrlw      %%mm1, 8\n\t"
                            "packuswb   %%mm1, %%mm1\n\t"

                            "punpcklbw  %%mm0, %%mm5\n\t"
                            "punpcklbw  %%mm1, %%mm5\n\t"
                            "paddusw    %%mm0, %%mm1\n\t"
                            "psrlw      %%mm0, 1\n\t"
                            "packuswb   %%mm0, %%mm0\n\t"
                            "movd       dword ptr [%%edi], %%mm0\n\t"
#endif
                        T2_NONSCALE_BLEND_ASM_LEAVE(t2_nonscale_mask_loop_50)
                        ++ty;
                    }
                    T2_NONSCALE_POP_EDI_ESI
                    break;
                }

                case BLEND_SUB1: {
                    T2_NONSCALE_PUSH_ESI_EDI
                    for (int y = r2.top; y <= r2.bottom; ++y) {
                        int* outp = &framebuffer[y * (w + 1) + r2.left];
                        pixel_rgb0_8* inp = &texture[ty * (iw + 1) + cx0];
                        int tot = (r2.right - r2.left);
                        T2_NONSCALE_BLEND_ASM_ENTER(t2_nonscale_mask_loop_sub1)
#ifdef _MSC_VER
                        __asm {
                            movd mm0, dword ptr [edi]
                            movd mm1, dword ptr [esi]

                            punpcklbw mm1, mm5
                            pmullw mm1, mm7
                            psrlw mm1, 8
                            packuswb mm1, mm1

                            psubusb mm0, mm1
                            movd dword ptr [edi], mm0
                        }
#else  // GCC
                        "movd       %%mm0, dword ptr [%%edi]\n\t"
                            "movd       %%mm1, dword ptr [%%esi]\n\t"

                            "punpcklbw  %%mm1, %%mm5\n\t"
                            "pmullw     %%mm1, %%mm7\n\t"
                            "psrlw      %%mm1, 8\n\t"
                            "packuswb   %%mm1, %%mm1\n\t"

                            "psubusb    %%mm0, %%mm1\n\t"
                            "movd       dword ptr [%%edi], %%mm0\n\t"
#endif
                        T2_NONSCALE_BLEND_ASM_LEAVE(t2_nonscale_mask_loop_sub1)
                        ++ty;
                    }
                    T2_NONSCALE_POP_EDI_ESI
                    break;
                }

                case BLEND_SUB2: {
                    T2_NONSCALE_PUSH_ESI_EDI
                    for (int y = r2.top; y <= r2.bottom; ++y) {
                        int* outp = &framebuffer[y * (w + 1) + r2.left];
                        pixel_rgb0_8* inp = &texture[ty * (iw + 1) + cx0];
                        int tot = (r2.right - r2.left);
                        T2_NONSCALE_BLEND_ASM_ENTER(t2_nonscale_mask_loop_sub2)
#ifdef _MSC_VER
                        __asm {
                            movd mm0, dword ptr [esi]

                            punpcklbw mm0, mm5
                            pmullw mm0, mm7
                            psrlw mm0, 8
                            packuswb mm0, mm0

                            psubusb mm0, qword ptr [edi]
                            movd dword ptr [edi], mm0
                        }
#else  // GCC
                        "movd       %%mm0, dword ptr [%%esi]\n\t"

                            "punpcklbw  %%mm0, %%mm5\n\t"
                            "pmullw     %%mm0, %%mm7\n\t"
                            "psrlw      %%mm0, 8\n\t"
                            "packuswb   %%mm0, %%mm0\n\t"

                            "psubusb    %%mm0, qword ptr [%%edi]\n\t"
                            "movd       dword ptr [%%edi], %%mm0\n\t"
#endif
                        T2_NONSCALE_BLEND_ASM_LEAVE(t2_nonscale_mask_loop_sub2)
                        ++ty;
                    }
                    T2_NONSCALE_POP_EDI_ESI
                    break;
                }

                case BLEND_MULTIPLY: {
                    T2_NONSCALE_PUSH_ESI_EDI
                    for (int y = r2.top; y <= r2.bottom; ++y) {
                        int* outp = &framebuffer[y * (w + 1) + r2.left];
                        pixel_rgb0_8* inp = &texture[ty * (iw + 1) + cx0];
                        int tot = (r2.right - r2.left);
                        T2_NONSCALE_BLEND_ASM_ENTER(t2_nonscale_mask_loop_mul)
#ifdef _MSC_VER
                        __asm {
                            movd mm0, dword ptr [edi]
                            movd mm1, dword ptr [esi]

                            punpcklbw mm0, mm5
                            punpcklbw mm1, mm5
                            pmullw mm0, mm1
                            psrlw mm0, 8
                            // Merged filter/mul
                            pmullw mm0, mm7
                            psrlw mm0, 8
                            packuswb mm0, mm0
                            movd dword ptr [edi], mm0
                        }
#else  // GCC
                        "movd       %%mm0, dword ptr [%%edi]\n\t"
                            "movd       %%mm1, dword ptr [%%esi]\n\t"
                            "punpcklbw  %%mm0, %%mm5\n\t"
                            "punpcklbw  %%mm1, %%mm5\n\t"
                            "pmullw     %%mm0, %%mm1\n\t"
                            "psrlw      %%mm0, 8\n\t"
                            // Merged filter/mul
                            "pmullw     %%mm0, %%mm7\n\t"
                            "psrlw      %%mm0, 8\n\t"
                            "packuswb   %%mm0, %%mm0\n\t"
                            "movd       dword ptr [%%edi], %%mm0\n\t"
#endif
                        T2_NONSCALE_BLEND_ASM_LEAVE(t2_nonscale_mask_loop_mul)
                        ++ty;
                    }
                    T2_NONSCALE_POP_EDI_ESI
                    break;
                }

                case BLEND_ADJUSTABLE: {
                    __int64 alphavalue = 0x0;
                    __int64* alpha = &alphavalue;
                    // TODO [bugfix]: shouldn't salpha be 255, not 256?
                    // it's used to calculate 256 - alpha, and if max_alpha = 255,
                    // then...
                    __int64 salpha = 0x0100010001000100;
                    int t = (g_line_blend_mode & 0xFF00) >> 8;
                    T2_NONSCALE_BLEND_AND_STORE_ALPHA
                    T2_NONSCALE_PUSH_ESI_EDI
                    for (int y = r2.top; y <= r2.bottom; ++y) {
                        int* outp = &framebuffer[y * (w + 1) + r2.left];
                        pixel_rgb0_8* inp = &texture[ty * (iw + 1) + cx0];
                        int tot = (r2.right - r2.left);
                        T2_NONSCALE_BLEND_ASM_ENTER(t2_nonscale_mask_loop_adj)
#ifdef _MSC_VER
                        __asm {
                            movd mm0, dword ptr [edi]
                            movd mm1, dword ptr [esi]

                            punpcklbw mm0, mm5
                            punpcklbw mm1, mm5
                                // Merged filter/alpha
                            pmullw    mm1, mm7
                            psrlw     mm1, 8

                            pmullw    mm0, mm6;  // * alph
                            pmullw    mm1, mm3;  // * 256 - alph

                            paddusw   mm0, mm1
                            psrlw     mm0, 8
                            packuswb  mm0, mm0
                            movd dword ptr [edi], mm0
                        }
#else  // GCC
                        "movd       %%mm0, dword ptr [%%edi]\n\t"
                            "movd       %%mm1, dword ptr [%%esi]\n\t"

                            "punpcklbw  %%mm0, %%mm5\n\t"
                            "punpcklbw  %%mm1, %%mm5\n\t"
                            // Merged filter/alpha
                            "pmullw     %%mm1, %%mm7\n\t"
                            "psrlw      %%mm1, 8\n\t"

                            "pmullw     %%mm0, %%mm6\n\t" // * alpha
                            "pmullw     %%mm1, %%mm3\n\t" // * 256 - alpha

                            "paddusw    %%mm0, %%mm1\n\t"
                            "psrlw      %%mm0, 8\n\t"
                            "packuswb   %%mm0, %%mm0\n\t"
                            "movd       dword ptr [%%edi], %%mm0\n\t"
#endif
                        T2_NONSCALE_BLEND_ASM_LEAVE(t2_nonscale_mask_loop_adj)
                        ++ty;
                    }
                    T2_NONSCALE_POP_EDI_ESI
                    break;
                }

                case BLEND_XOR: {
                    T2_NONSCALE_PUSH_ESI_EDI
                    for (int y = r2.top; y <= r2.bottom; ++y) {
                        int* outp = &framebuffer[y * (w + 1) + r2.left];
                        pixel_rgb0_8* inp = &texture[ty * (iw + 1) + cx0];
                        int tot = (r2.right - r2.left);
                        T2_NONSCALE_BLEND_ASM_ENTER(t2_nonscale_mask_loop_xor)
#ifdef _MSC_VER
                        __asm {
                            movd mm0, dword ptr [edi]
                            movd mm1, dword ptr [esi]

                            punpcklbw mm1, mm5
                            pmullw mm1, mm7
                            psrlw mm1, 8
                            packuswb mm1, mm1

                            pxor mm0, mm1
                            movd dword ptr [edi], mm0
                        }
#else  // GCC
                        "movd       %%mm0, dword ptr [%%edi]\n\t"
                            "movd       %%mm1, dword ptr [%%esi]\n\t"

                            "punpcklbw  %%mm1, %%mm5\n\t"
                            "pmullw     %%mm1, %%mm7\n\t"
                            "psrlw      %%mm1, 8\n\t"
                            "packuswb   %%mm1, %%mm1\n\t"

                            "pxor       %%mm0, %%mm1\n\t"
                            "movd       dword ptr [%%edi], %%mm0\n\t"
#endif
                        T2_NONSCALE_BLEND_ASM_LEAVE(t2_nonscale_mask_loop_xor)
                        ++ty;
                    }
                    T2_NONSCALE_POP_EDI_ESI
                    break;
                }

                case BLEND_MINIMUM: {
                    int maxmask = 0xFFFFFF;
                    int signmask = 0x808080;
                    T2_NONSCALE_PUSH_ESI_EDI
                    T2_NONSCALE_MINMAX_MASKS
                    for (int y = r2.top; y <= r2.bottom; ++y) {
                        int* outp = &framebuffer[y * (w + 1) + r2.left];
                        pixel_rgb0_8* inp = &texture[ty * (iw + 1) + cx0];
                        int tot = (r2.right - r2.left);
                        T2_NONSCALE_BLEND_ASM_ENTER(t2_nonscale_mask_loop_min)
#ifdef _MSC_VER
                        __asm {
                            movd mm0, dword ptr [edi]
                            movd mm1, dword ptr [esi]

                            punpcklbw mm1, mm5
                            pmullw mm1, mm7
                            psrlw mm1, 8
                            packuswb mm1, mm1

                            pxor mm0, mm6
                            pxor mm1, mm6
                            movq mm2, mm1
                            pcmpgtb mm2, mm0
                            movq mm3, mm2
                            pxor mm3, mm4
                            pand mm0, mm2
                            pand mm1, mm3
                            por mm0, mm1
                            pxor mm0, mm6
                            movd dword ptr [edi], mm0
                        }
#else  // GCC
                        "movd       %%mm0, dword ptr [%%edi]\n\t"
                            "movd       %%mm1, dword ptr [%%esi]\n\t"

                            "punpcklbw  %%mm1, %%mm5\n\t"
                            "pmullw     %%mm1, %%mm7\n\t"
                            "psrlw      %%mm1, 8\n\t"
                            "packuswb   %%mm1, %%mm1\n\t"

                            "pxor       %%mm0, %%mm6\n\t"
                            "pxor       %%mm1, %%mm6\n\t"
                            "movq       %%mm2, %%mm1\n\t"
                            "pcmpgtb    %%mm2, %%mm0\n\t"
                            "movq       %%mm3, %%mm2\n\t"
                            "pxor       %%mm3, %%mm4\n\t"
                            "pand       %%mm0, %%mm2\n\t"
                            "pand       %%mm1, %%mm3\n\t"
                            "por        %%mm0, %%mm1\n\t"
                            "pxor       %%mm0, %%mm6\n\t"
                            "movd       dword ptr [%%edi], %%mm0\n\t"
#endif
                        T2_NONSCALE_BLEND_ASM_LEAVE(t2_nonscale_mask_loop_min)
                        ++ty;
                    }
                    T2_NONSCALE_POP_EDI_ESI
                    break;
                }
            }
        } else {
            // Most basic path, no scaling or masking
            T2_ZERO_MM5
            switch (g_line_blend_mode & 0xFF) {
                case BLEND_REPLACE: {
                    // the push order was reversed (edi, esi) in the original code here
                    // (and only here) -- i assume that was not intentional...
                    T2_NONSCALE_PUSH_ESI_EDI
                    for (int y = r2.top; y <= r2.bottom; ++y) {
                        int* outp = &framebuffer[y * (w + 1) + r2.left];
                        pixel_rgb0_8* inp = &texture[ty * (iw + 1) + cx0];
                        int tot = (r2.right - r2.left);
                        T2_NONSCALE_BLEND_ASM_ENTER(t2_nonscale_nonmask_loop_rep)
#ifdef _MSC_VER
                        __asm {
                            movd mm0, dword ptr [esi]
                            movd dword ptr [edi], mm0
                        }
#else  // GCC
                        "movd  %%mm0, dword ptr [%%esi]\n\t"
                            "movd  dword ptr [%%edi], %%mm0\n\t"
#endif
                        T2_NONSCALE_BLEND_ASM_LEAVE(t2_nonscale_nonmask_loop_rep)
                        ++ty;
                    }
                    T2_NONSCALE_POP_EDI_ESI
                    break;
                }

                case BLEND_ADDITIVE: {
                    T2_NONSCALE_PUSH_ESI_EDI
                    for (int y = r2.top; y <= r2.bottom; ++y) {
                        int* outp = &framebuffer[y * (w + 1) + r2.left];
                        pixel_rgb0_8* inp = &texture[ty * (iw + 1) + cx0];
                        int tot = (r2.right - r2.left);
                        T2_NONSCALE_BLEND_ASM_ENTER(t2_nonscale_nonmask_loop_add)
#ifdef _MSC_VER
                        __asm {
                            movd mm0, dword ptr [edi]
                            paddusb mm0, qword ptr [esi]
                            movd dword ptr [edi], mm0
                        }
#else  // GCC
                        "movd     %%mm0, dword ptr [%%edi]\n\t"
                            "paddusb  %%mm0, qword ptr [%%esi]\n\t"
                            "movd     dword ptr [%%edi], %%mm0\n\t"
#endif
                        T2_NONSCALE_BLEND_ASM_LEAVE(t2_nonscale_nonmask_loop_add)
                        ++ty;
                    }
                    T2_NONSCALE_POP_EDI_ESI
                    break;
                }

                case BLEND_MAXIMUM: {
                    int maxmask = 0xFFFFFF;
                    int signmask = 0x808080;
                    T2_NONSCALE_PUSH_ESI_EDI
                    T2_NONSCALE_MINMAX_MASKS
                    for (int y = r2.top; y <= r2.bottom; ++y) {
                        int* outp = &framebuffer[y * (w + 1) + r2.left];
                        pixel_rgb0_8* inp = &texture[ty * (iw + 1) + cx0];
                        int tot = (r2.right - r2.left);
                        T2_NONSCALE_BLEND_ASM_ENTER(t2_nonscale_nonmask_loop_max)
#ifdef _MSC_VER
                        __asm {
                            movd mm0, dword ptr [edi]
                            movd mm1, dword ptr [esi]
                            pxor mm0, mm6
                            pxor mm1, mm6
                            movq mm2, mm1
                            pcmpgtb mm2, mm0
                            movq mm3, mm2
                            pxor mm3, mm4
                            pand mm0, mm3
                            pand mm1, mm2
                            por mm0, mm1
                            pxor mm0, mm6
                            movd dword ptr [edi], mm0
                        }
#else  // GCC
                        "movd     %%mm0, dword ptr [%%edi]\n\t"
                            "movd     %%mm1, dword ptr [%%esi]\n\t"
                            "pxor     %%mm0, %%mm6\n\t"
                            "pxor     %%mm1, %%mm6\n\t"
                            "movq     %%mm2, %%mm1\n\t"
                            "pcmpgtb  %%mm2, %%mm0\n\t"
                            "movq     %%mm3, %%mm2\n\t"
                            "pxor     %%mm3, %%mm4\n\t"
                            "pand     %%mm0, %%mm3\n\t"
                            "pand     %%mm1, %%mm2\n\t"
                            "por      %%mm0, %%mm1\n\t"
                            "pxor     %%mm0, %%mm6\n\t"
                            "movd     dword ptr [%%edi], %%mm0\n\t"
#endif
                        T2_NONSCALE_BLEND_ASM_LEAVE(t2_nonscale_nonmask_loop_max)
                        ++ty;
                    }
                    T2_NONSCALE_POP_EDI_ESI
                    break;
                }

                case BLEND_5050: {
                    T2_NONSCALE_PUSH_ESI_EDI
                    for (int y = r2.top; y <= r2.bottom; ++y) {
                        int* outp = &framebuffer[y * (w + 1) + r2.left];
                        pixel_rgb0_8* inp = &texture[ty * (iw + 1) + cx0];
                        int tot = (r2.right - r2.left);
                        T2_NONSCALE_BLEND_ASM_ENTER(t2_nonscale_nonmask_loop_50)
#ifdef _MSC_VER
                        __asm {
                            movd mm0, dword ptr [edi]
                            movd mm1, dword ptr [esi]
                            punpcklbw mm0, mm5
                            punpcklbw mm1, mm5
                            paddusw mm0, mm1
                            psrlw mm0, 1
                            packuswb mm0, mm0
                            movd dword ptr [edi], mm0
                        }
#else  // GCC
                        "movd       %%mm0, dword ptr [%%edi]\n\t"
                            "movd       %%mm1, dword ptr [%%esi]\n\t"
                            "punpcklbw  %%mm0, %%mm5\n\t"
                            "punpcklbw  %%mm1, %%mm5\n\t"
                            "paddusw    %%mm0, %%mm1\n\t"
                            "psrlw      %%mm0, 1\n\t"
                            "packuswb   %%mm0, %%mm0\n\t"
                            "movd       dword ptr [%%edi], %%mm0\n\t"
#endif
                        T2_NONSCALE_BLEND_ASM_LEAVE(t2_nonscale_nonmask_loop_50)
                        ++ty;
                    }
                    T2_NONSCALE_POP_EDI_ESI
                    break;
                }

                case BLEND_SUB1: {
                    T2_NONSCALE_PUSH_ESI_EDI
                    for (int y = r2.top; y <= r2.bottom; ++y) {
                        int* outp = &framebuffer[y * (w + 1) + r2.left];
                        pixel_rgb0_8* inp = &texture[ty * (iw + 1) + cx0];
                        int tot = (r2.right - r2.left);
                        T2_NONSCALE_BLEND_ASM_ENTER(t2_nonscale_nonmask_loop_sub1)
#ifdef _MSC_VER
                        __asm {
                            movd mm0, dword ptr [edi]
                            psubusb mm0, qword ptr [esi]
                            movd dword ptr [edi], mm0
                        }
#else  // GCC
                        "movd     %%mm0, dword ptr [%%edi]\n\t"
                            "psubusb  %%mm0, qword ptr [%%esi]\n\t"
                            "movd     dword ptr [%%edi], %%mm0\n\t"
#endif
                        T2_NONSCALE_BLEND_ASM_LEAVE(t2_nonscale_nonmask_loop_sub1)
                        ++ty;
                    }
                    T2_NONSCALE_POP_EDI_ESI
                    break;
                }

                case BLEND_SUB2: {
                    T2_NONSCALE_PUSH_ESI_EDI
                    for (int y = r2.top; y <= r2.bottom; ++y) {
                        int* outp = &framebuffer[y * (w + 1) + r2.left];
                        pixel_rgb0_8* inp = &texture[ty * (iw + 1) + cx0];
                        int tot = (r2.right - r2.left);
                        T2_NONSCALE_BLEND_ASM_ENTER(t2_nonscale_nonmask_loop_sub2)
#ifdef _MSC_VER
                        __asm {
                            movd mm0, dword ptr [esi]
                            psubusb mm0, qword ptr [edi]
                            movd dword ptr [edi], mm0
                        }
#else  // GCC
                        "movd     %%mm0, dword ptr [%%esi]\n\t"
                            "psubusb  %%mm0, qword ptr [%%edi]\n\t"
                            "movd     dword ptr [%%edi], %%mm0\n\t"
#endif
                        T2_NONSCALE_BLEND_ASM_LEAVE(t2_nonscale_nonmask_loop_sub2)
                        ++ty;
                    }
                    T2_NONSCALE_POP_EDI_ESI
                    break;
                }

                case BLEND_MULTIPLY: {
                    T2_NONSCALE_PUSH_ESI_EDI
                    for (int y = r2.top; y <= r2.bottom; ++y) {
                        int* outp = &framebuffer[y * (w + 1) + r2.left];
                        pixel_rgb0_8* inp = &texture[ty * (iw + 1) + cx0];
                        int tot = (r2.right - r2.left);
                        T2_NONSCALE_BLEND_ASM_ENTER(t2_nonscale_nonmask_loop_mul)
#ifdef _MSC_VER
                        __asm {
                            movd mm0, dword ptr [edi]
                            movd mm1, dword ptr [esi]
                            punpcklbw mm0, mm5
                            punpcklbw mm1, mm5
                            pmullw mm0, mm1
                            psrlw mm0, 8
                            packuswb mm0, mm0
                            movd dword ptr [edi], mm0
                        }
#else  // GCC
                        "movd       %%mm0, dword ptr [%%edi]\n\t"
                            "movd       %%mm1, dword ptr [%%esi]\n\t"
                            "punpcklbw  %%mm0, %%mm5\n\t"
                            "punpcklbw  %%mm1, %%mm5\n\t"
                            "pmullw     %%mm0, %%mm1\n\t"
                            "psrlw      %%mm0, 8\n\t"
                            "packuswb   %%mm0, %%mm0\n\t"
                            "movd       dword ptr [%%edi], %%mm0\n\t"
#endif
                        T2_NONSCALE_BLEND_ASM_LEAVE(t2_nonscale_nonmask_loop_mul)
                        ++ty;
                    }
                    T2_NONSCALE_POP_EDI_ESI
                    break;
                }

                case BLEND_ADJUSTABLE: {
                    __int64 alphavalue = 0x0;
                    __int64* alpha = &alphavalue;
                    // TODO [bugfix]: shouldn't salpha be 255, not 256?
                    // it's used to calculate 256 - alpha, and if max_alpha = 255,
                    // then...
                    __int64 salpha = 0x0100010001000100;
                    int t = (g_line_blend_mode & 0xFF00) >> 8;
                    T2_NONSCALE_BLEND_AND_STORE_ALPHA
                    T2_NONSCALE_PUSH_ESI_EDI
                    for (int y = r2.top; y <= r2.bottom; ++y) {
                        int* outp = &framebuffer[y * (w + 1) + r2.left];
                        pixel_rgb0_8* inp = &texture[ty * (iw + 1) + cx0];
                        int tot = (r2.right - r2.left);
                        T2_NONSCALE_BLEND_ASM_ENTER(t2_nonscale_nonmask_loop_adj)
#ifdef _MSC_VER
                        __asm {
                            movd mm0, dword ptr [edi]
                            movd mm1, dword ptr [esi]

                            punpcklbw mm0, mm5
                            punpcklbw mm1, mm5

                            pmullw    mm0, mm6;  // * alph
                            pmullw    mm1, mm3;  // * 256 - alph

                            paddusw   mm0, mm1
                            psrlw     mm0, 8
                            packuswb  mm0, mm0
                            movd dword ptr [edi], mm0
                        }
#else  // GCC
                        "movd       %%mm0, dword ptr [%%edi]\n\t"
                            "movd       %%mm1, dword ptr [%%esi]\n\t"

                            "punpcklbw  %%mm0, %%mm5\n\t"
                            "punpcklbw  %%mm1, %%mm5\n\t"

                            "pmullw     %%mm0, %%mm6\n\t" // * alpha
                            "pmullw     %%mm1, %%mm3\n\t" // * 256 - alpha

                            "paddusw    %%mm0, %%mm1\n\t"
                            "psrlw      %%mm0, 8\n\t"
                            "packuswb   %%mm0, %%mm0\n\t"
                            "movd       dword ptr [%%edi], %%mm0\n\t"
#endif
                        T2_NONSCALE_BLEND_ASM_LEAVE(t2_nonscale_nonmask_loop_adj)
                        ++ty;
                    }
                    T2_NONSCALE_POP_EDI_ESI
                    break;
                }

                case BLEND_XOR: {
                    T2_NONSCALE_PUSH_ESI_EDI
                    for (int y = r2.top; y <= r2.bottom; ++y) {
                        int* outp = &framebuffer[y * (w + 1) + r2.left];
                        pixel_rgb0_8* inp = &texture[ty * (iw + 1) + cx0];
                        int tot = (r2.right - r2.left);
                        T2_NONSCALE_BLEND_ASM_ENTER(t2_nonscale_nonmask_loop_xor)
#ifdef _MSC_VER
                        __asm {
                            movd mm0, dword ptr [edi]
                            pxor mm0, qword ptr [esi]
                            movd dword ptr [edi], mm0
                        }
#else  // GCC
                        "movd  %%mm0, dword ptr [%%edi]\n\t"
                            "pxor  %%mm0, qword ptr [%%esi]\n\t"
                            "movd  dword ptr [%%edi], %%mm0\n\t"
#endif
                        T2_NONSCALE_BLEND_ASM_LEAVE(t2_nonscale_nonmask_loop_xor)
                        ++ty;
                    }
                    T2_NONSCALE_POP_EDI_ESI
                    break;
                }

                case BLEND_MINIMUM: {
                    int maxmask = 0xFFFFFF;
                    int signmask = 0x808080;
                    T2_NONSCALE_PUSH_ESI_EDI
                    T2_NONSCALE_MINMAX_MASKS
                    for (int y = r2.top; y <= r2.bottom; ++y) {
                        int* outp = &framebuffer[y * (w + 1) + r2.left];
                        pixel_rgb0_8* inp = &texture[ty * (iw + 1) + cx0];
                        int tot = (r2.right - r2.left);
                        T2_NONSCALE_BLEND_ASM_ENTER(t2_nonscale_nonmask_loop_min)
#ifdef _MSC_VER
                        __asm {
                            movd mm0, dword ptr [edi]
                            movd mm1, dword ptr [esi]
                            pxor mm0, mm6
                            pxor mm1, mm6
                            movq mm2, mm1
                            pcmpgtb mm2, mm0
                            movq mm3, mm2
                            pxor mm3, mm4
                            pand mm0, mm2
                            pand mm1, mm3
                            por mm0, mm1
                            pxor mm0, mm6
                            movd dword ptr [edi], mm0
                        }
#else  // GCC
                        "movd     %%mm0, dword ptr [%%edi]\n\t"
                            "movd     %%mm1, dword ptr [%%esi]\n\t"
                            "pxor     %%mm0, %%mm6\n\t"
                            "pxor     %%mm1, %%mm6\n\t"
                            "movq     %%mm2, %%mm1\n\t"
                            "pcmpgtb  %%mm2, %%mm0\n\t"
                            "movq     %%mm3, %%mm2\n\t"
                            "pxor     %%mm3, %%mm4\n\t"
                            "pand     %%mm0, %%mm2\n\t"
                            "pand     %%mm1, %%mm3\n\t"
                            "por      %%mm0, %%mm1\n\t"
                            "pxor     %%mm0, %%mm6\n\t"
                            "movd     dword ptr [%%edi], %%mm0\n\t"
#endif
                        T2_NONSCALE_BLEND_ASM_LEAVE(t2_nonscale_nonmask_loop_min)
                        ++ty;
                    }
                    T2_NONSCALE_POP_EDI_ESI
                    break;
                }
            }
        }
    }

skippart:
    ++(this->iw);  // restore member vars!
    ++(this->ih);

    // Perhaps removing all fpu stuff here and doing one emms globally would be better?
    EMMS
}

inline double wrap_diff_to_plusminus1(double x) { return round(x / 2.0) * 2.0; }

/* For AVS 2.81d compatibility. Texer2 v1.0 only wrapped once. */
inline double wrap_once_diff_to_plusminus1(double x) {
    return (x > 1.0) ? +2.0 : (x < -1.0 ? -2.0 : 0.0);
}

bool overlaps_edge(double wrapped_coord,
                   double img_size,
                   int img_size_px,
                   int screen_size_px) {
    double abs_wrapped_coord = fabs(wrapped_coord);
    // a /2 seems missing, but screen has size 2, hence /2(half) *2(screen size) => *1
    // image pixel size needs reduction by one pixel
    double rel_size_half = img_size * (img_size_px - 1) / screen_size_px;
    return ((abs_wrapped_coord + rel_size_half) > 1.0)
           && ((abs_wrapped_coord - rel_size_half) < 1.0);
}

int E_Texer2::render(char visdata[2][2][576],
                     int is_beat,
                     int* framebuffer,
                     int*,
                     int w,
                     int h) {
    this->recompile_if_needed();
    this->init_variables(w, h, is_beat, this->iw, this->ih);
    if (this->need_init || (is_beat & 0x80000000)) {
        *this->vars.n = 0.0f;
        this->code_init.exec(visdata);
        this->need_init = false;
    }
    this->code_frame.exec(visdata);
    if ((is_beat & 0x00000001)) {
        this->code_beat.exec(visdata);
    }
    int n = RoundToInt((double)*this->vars.n);
    n = max(0, min(65536, n));

    if (n <= 0) {
        return 0;
    }

    lock_lock(this->image_lock);
    double step = 1.0 / (n - 1);
    double i = 0.0;
    for (int j = 0; j < n; ++j) {
        *this->vars.i = i;
        *this->vars.skip = 0.0;
        *this->vars.v =
            ((int)visdata[1][0][j * 575 / n] + (int)visdata[1][1][j * 575 / n]) / 256.0;
        i += step;

        this->code_point.exec(visdata);

        if (*this->vars.skip != 0.0) {
            continue;
        }

        double szx = fabs(*this->vars.sizex);
        double szy = fabs(*this->vars.sizey);

        // TODO [bugfix]: really large images would be clipped while still
        // potentially visible, should be relative to image size
        if ((szx <= .01) || (szy <= .01)) {
            continue;
        }
        unsigned int color =
            min(255, max(0, RoundToInt(255.0f * (double)*this->vars.blue)));
        color |= min(255, max(0, RoundToInt(255.0f * (double)*this->vars.green))) << 8;
        color |= min(255, max(0, RoundToInt(255.0f * (double)*this->vars.red))) << 16;

        if (!this->config.colorize) {
            color = 0xFFFFFF;
        }

        pixel_rgb0_8* texture = this->image_normal;
        if (*this->vars.sizex < 0 && *this->vars.sizey > 0) {
            texture = this->image_mirrored;
        } else if (*this->vars.sizex > 0 && *this->vars.sizey < 0) {
            texture = this->image_flipped;
        } else if (*this->vars.sizex < 0 && *this->vars.sizey < 0) {
            texture = this->image_rot180;
        }

        double x = *this->vars.x;
        double y = *this->vars.y;
        if (this->config.wrap) {
            bool overlaps_x, overlaps_y;
            if (this->config.version == TEXER_II_VERSION_V2_81D) {
                /* Wrap only once (when crossing +/-1, not when crossing +/-3 etc. */
                x -= wrap_once_diff_to_plusminus1(*this->vars.x);
                y -= wrap_once_diff_to_plusminus1(*this->vars.y);
                overlaps_x = overlaps_edge(*this->vars.x, szx, this->iw, w);
                overlaps_y = overlaps_edge(*this->vars.y, szy, this->ih, h);
            } else {
                x = *this->vars.x - wrap_diff_to_plusminus1(*this->vars.x);
                y = *this->vars.y - wrap_diff_to_plusminus1(*this->vars.y);
                overlaps_x = overlaps_edge(x, szx, this->iw, w);
                overlaps_y = overlaps_edge(y, szy, this->ih, h);
            }
            double sign_x2 = x > 0 ? 2.0 : -2.0;
            double sign_y2 = y > 0 ? 2.0 : -2.0;
            if (overlaps_x) {
                DrawParticle(
                    framebuffer, texture, w, h, x - sign_x2, y, szx, szy, color);
            }
            if (overlaps_y) {
                DrawParticle(
                    framebuffer, texture, w, h, x, y - sign_y2, szx, szy, color);
            }
            if (overlaps_x && overlaps_y) {
                DrawParticle(framebuffer,
                             texture,
                             w,
                             h,
                             x - sign_x2,
                             y - sign_y2,
                             szx,
                             szy,
                             color);
            }
        }
        DrawParticle(framebuffer, texture, w, h, x, y, szx, szy, color);
    }
    lock_unlock(this->image_lock);
    return 0;
}

void Texer2_Vars::register_(void* vm_context) {
    this->n = NSEEL_VM_regvar(vm_context, "n");
    this->i = NSEEL_VM_regvar(vm_context, "i");
    this->x = NSEEL_VM_regvar(vm_context, "x");
    this->y = NSEEL_VM_regvar(vm_context, "y");
    this->v = NSEEL_VM_regvar(vm_context, "v");
    this->b = NSEEL_VM_regvar(vm_context, "b");
    this->w = NSEEL_VM_regvar(vm_context, "w");
    this->h = NSEEL_VM_regvar(vm_context, "h");
    this->iw = NSEEL_VM_regvar(vm_context, "iw");
    this->ih = NSEEL_VM_regvar(vm_context, "ih");
    this->sizex = NSEEL_VM_regvar(vm_context, "sizex");
    this->sizey = NSEEL_VM_regvar(vm_context, "sizey");
    this->red = NSEEL_VM_regvar(vm_context, "red");
    this->green = NSEEL_VM_regvar(vm_context, "green");
    this->blue = NSEEL_VM_regvar(vm_context, "blue");
    this->skip = NSEEL_VM_regvar(vm_context, "skip");
}

void Texer2_Vars::init(int w, int h, int is_beat, va_list extra_args) {
    *this->i = 0.0f;
    *this->x = 0.0f;
    *this->y = 0.0f;
    *this->v = 0.0f;
    *this->w = w;
    *this->h = h;
    *this->b = is_beat ? 1.0f : 0.0f;
    *this->iw = va_arg(extra_args, int);
    *this->ih = va_arg(extra_args, int);
    *this->sizex = 1.0f;
    *this->sizey = 1.0f;
    *this->red = 1.0f;
    *this->green = 1.0f;
    *this->blue = 1.0f;
    *this->skip = 0.0f;
}

void E_Texer2::load_legacy(unsigned char* data, int len) {
    char* str_data = (char*)data;
    int pos = 0;
    this->config.version = *(int32_t*)&data[pos];
    if (this->config.version < TEXER_II_VERSION_V2_81D
        || this->config.version > TEXER_II_VERSION_CURRENT) {
        // If the version value is not in the known set, assume an old preset version.
        this->config.version = TEXER_II_VERSION_V2_81D;
    }
    pos += 4;
    std::string search_filename;
    this->string_nt_load_legacy(&str_data[pos], search_filename, LEGACY_SAVE_PATH_LEN);
    bool found = false;
    for (size_t i = 0; i < this->filenames.size(); i++) {
        if (this->filenames[i] == search_filename) {
            found = true;
            this->config.image = i;
        }
    }
    if (!found) {
        this->config.image = 0;
    }
    pos += LEGACY_SAVE_PATH_LEN;
    this->config.resize = *(int32_t*)&data[pos];
    pos += 4;
    this->config.wrap = *(int32_t*)&data[pos];
    pos += 4;
    this->config.colorize = *(int32_t*)&data[pos];
    pos += 4;
    pos += 4;  // unused 4 bytes
    pos += this->string_load_legacy(&str_data[pos], this->config.init, len - pos);
    pos += this->string_load_legacy(&str_data[pos], this->config.frame, len - pos);
    pos += this->string_load_legacy(&str_data[pos], this->config.beat, len - pos);
    pos += this->string_load_legacy(&str_data[pos], this->config.point, len - pos);
    this->need_full_recompile();
    this->load_image();
}

int E_Texer2::save_legacy(unsigned char* data) {
    char* str_data = (char*)data;
    int pos = 0;
    *(int32_t*)&data[pos] = this->config.version;
    pos += 4;
    auto str_len = string_nt_save_legacy(
        this->filenames[this->config.image], &str_data[pos], LEGACY_SAVE_PATH_LEN);
    memset(&str_data[pos + str_len], '\0', LEGACY_SAVE_PATH_LEN - str_len);
    pos += LEGACY_SAVE_PATH_LEN;
    *(int32_t*)&data[pos] = this->config.resize;
    pos += 4;
    *(int32_t*)&data[pos] = this->config.wrap;
    pos += 4;
    *(int32_t*)&data[pos] = this->config.colorize;
    pos += 4;
    *(int32_t*)&data[pos] = 0;  // unused 4 bytes
    pos += 4;
    pos += this->string_save_legacy(
        this->config.init, &str_data[pos], MAX_CODE_LEN - 1 - pos);
    pos += this->string_save_legacy(
        this->config.frame, &str_data[pos], MAX_CODE_LEN - 1 - pos);
    pos += this->string_save_legacy(
        this->config.beat, &str_data[pos], MAX_CODE_LEN - 1 - pos);
    pos += this->string_save_legacy(
        this->config.point, &str_data[pos], MAX_CODE_LEN - 1 - pos);
    return pos;
}

Effect_Info* create_Texer2_Info() { return new Texer2_Info(); }
Effect* create_Texer2() { return new E_Texer2(); }
