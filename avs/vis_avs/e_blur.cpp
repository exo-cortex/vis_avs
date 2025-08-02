/*
  LICENSE
  -------
Copyright 2005 Nullsoft, Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

  * Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

  * Neither the name of Nullsoft nor the names of its contributors may be used to
    endorse or promote products derived from this software without specific prior
    written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/
// alphachannel safe 11/21/99

#include "e_blur.h"

#define PUT_INT(y)                   \
    data[pos] = (y) & 255;           \
    data[pos + 1] = (y >> 8) & 255;  \
    data[pos + 2] = (y >> 16) & 255; \
    data[pos + 3] = (y >> 24) & 255
#define GET_INT() \
    (data[pos] | (data[pos + 1] << 8) | (data[pos + 2] << 16) | (data[pos + 3] << 24))

#define MASK_SH1 (~(((1u << 7u) | (1u << 15u) | (1u << 23u)) << 1u))
#define MASK_SH2 (~(((3u << 6u) | (3u << 14u) | (3u << 22u)) << 2u))
#define MASK_SH3 (~(((7u << 5u) | (7u << 13u) | (7u << 21u)) << 3u))
#define MASK_SH4 (~(((15u << 4u) | (15u << 12u) | (15u << 20u)) << 4u))
static unsigned int mmx_mask1[2] = {MASK_SH1, MASK_SH1};
static unsigned int mmx_mask2[2] = {MASK_SH2, MASK_SH2};
static unsigned int mmx_mask3[2] = {MASK_SH3, MASK_SH3};
static unsigned int mmx_mask4[2] = {MASK_SH4, MASK_SH4};

#define DIV_2(x)  (((x) & MASK_SH1) >> 1)
#define DIV_4(x)  (((x) & MASK_SH2) >> 2)
#define DIV_8(x)  (((x) & MASK_SH3) >> 3)
#define DIV_16(x) (((x) & MASK_SH4) >> 4)

constexpr Parameter Blur_Info::parameters[];

E_Blur::E_Blur(AVS_Instance* avs) : Configurable_Effect(avs) {}
E_Blur::~E_Blur() {}

int threading_stuff(int thread_index,
                    int num_threads,
                    unsigned int* framebuffer_in,
                    unsigned int* framebuffer_out,
                    int width,
                    int height,
                    int* at_top,
                    int* at_bottom,
                    int* outh) {
    if (num_threads < 1) {
        num_threads = 1;
    }

    int start_line = (thread_index * height) / num_threads;
    int end_line;

    if (thread_index >= num_threads - 1) {
        end_line = height;
    } else {
        end_line = ((thread_index + 1) * height) / num_threads;
    }

    *outh = end_line - start_line;
    if (*outh < 1) {
        return 1;
    }

    int skip_pix = start_line * width;

    framebuffer_in += skip_pix;
    framebuffer_out += skip_pix;

    if (!thread_index) {
        *at_top = 1;
    }
    if (thread_index >= num_threads - 1) {
        *at_bottom = 1;
    }

    return 0;
}

void top(int this_thread,
         int max_threads,
         unsigned int* f,
         unsigned int* of,
         int width,
         int height) {}

void E_Blur::smp_render(int this_thread,
                        int max_threads,
                        char[2][2][576],
                        int,
                        int* framebuffer,
                        int* fbout,
                        int w,
                        int h) {
    unsigned int* f = (unsigned int*)framebuffer;
    unsigned int* of = (unsigned int*)fbout;

    int at_top = 0;
    int at_bottom = 0;
    int outh;

    if (threading_stuff(
            this_thread, max_threads, f, of, w, h, &at_top, &at_bottom, &outh)) {
        return;
    }

    if (this->config.level == BLUR_LIGHT) {
        // top line
        if (at_top) {
            unsigned int* f2 = f + w;
            int x;
            int adj_tl = 0;
            int adj_tl2 = 0;

            if (this->config.round == BLUR_ROUND_UP) {
                adj_tl = 0x03030303;

                adj_tl2 = 0x04040404;
            }
            // top left
            *of++ = DIV_2(f[0]) + DIV_4(f[0]) + DIV_8(f[1]) + DIV_8(f2[0]) + adj_tl;
            f++;
            f2++;
            // top center
            x = (w - 2) / 4;
            while (x--) {
                of[0] = DIV_2(f[0]) + DIV_8(f[0]) + DIV_8(f[1]) + DIV_8(f[-1])
                        + DIV_8(f2[0]) + adj_tl2;
                of[1] = DIV_2(f[1]) + DIV_8(f[1]) + DIV_8(f[2]) + DIV_8(f[0])
                        + DIV_8(f2[1]) + adj_tl2;
                of[2] = DIV_2(f[2]) + DIV_8(f[2]) + DIV_8(f[3]) + DIV_8(f[1])
                        + DIV_8(f2[2]) + adj_tl2;
                of[3] = DIV_2(f[3]) + DIV_8(f[3]) + DIV_8(f[4]) + DIV_8(f[2])
                        + DIV_8(f2[3]) + adj_tl2;
                f += 4;
                f2 += 4;
                of += 4;
            }
            x = (w - 2) & 3;
            while (x--) {
                *of++ = DIV_2(f[0]) + DIV_8(f[0]) + DIV_8(f[1]) + DIV_8(f[-1])
                        + DIV_8(f2[0]) + adj_tl2;
                f++;
                f2++;
            }
            // top right
            *of++ = DIV_2(f[0]) + DIV_4(f[0]) + DIV_8(f[-1]) + DIV_8(f2[0]) + adj_tl;
            f++;
            f2++;
        }

        // middle block
        {
            int y = outh - at_top - at_bottom;
            unsigned int adj_tl1 = 0, adj_tl2 = 0;
            uint64_t adj2 = 0;
            if (this->config.round == BLUR_ROUND_UP) {
                adj_tl1 = 0x04040404;
                adj_tl2 = 0x05050505;
                adj2 = 0x0505050505050505L;
            }
            while (y--) {
                int x;
                unsigned int* f2 = f + w;
                unsigned int* f3 = f - w;

                // left edge
                *of++ = DIV_2(f[0]) + DIV_8(f[0]) + DIV_8(f[1]) + DIV_8(f2[0])
                        + DIV_8(f3[0]) + adj_tl1;
                f++;
                f2++;
                f3++;

                // middle of line

                x = (w - 2) / 4;
                if (this->config.round == BLUR_ROUND_UP) {
                    while (x--) {
                        of[0] = DIV_2(f[0]) + DIV_4(f[0]) + DIV_16(f[1]) + DIV_16(f[-1])
                                + DIV_16(f2[0]) + DIV_16(f3[0]) + 0x05050505;
                        of[1] = DIV_2(f[1]) + DIV_4(f[1]) + DIV_16(f[2]) + DIV_16(f[0])
                                + DIV_16(f2[1]) + DIV_16(f3[1]) + 0x05050505;
                        of[2] = DIV_2(f[2]) + DIV_4(f[2]) + DIV_16(f[3]) + DIV_16(f[1])
                                + DIV_16(f2[2]) + DIV_16(f3[2]) + 0x05050505;
                        of[3] = DIV_2(f[3]) + DIV_4(f[3]) + DIV_16(f[4]) + DIV_16(f[2])
                                + DIV_16(f2[3]) + DIV_16(f3[3]) + 0x05050505;
                        f += 4;
                        f2 += 4;
                        f3 += 4;
                        of += 4;
                    }
                } else {
                    while (x--) {
                        of[0] = DIV_2(f[0]) + DIV_4(f[0]) + DIV_16(f[1]) + DIV_16(f[-1])
                                + DIV_16(f2[0]) + DIV_16(f3[0]);
                        of[1] = DIV_2(f[1]) + DIV_4(f[1]) + DIV_16(f[2]) + DIV_16(f[0])
                                + DIV_16(f2[1]) + DIV_16(f3[1]);
                        of[2] = DIV_2(f[2]) + DIV_4(f[2]) + DIV_16(f[3]) + DIV_16(f[1])
                                + DIV_16(f2[2]) + DIV_16(f3[2]);
                        of[3] = DIV_2(f[3]) + DIV_4(f[3]) + DIV_16(f[4]) + DIV_16(f[2])
                                + DIV_16(f2[3]) + DIV_16(f3[3]);
                        f += 4;
                        f2 += 4;
                        f3 += 4;
                        of += 4;
                    }
                }

                x = (w - 2) & 3;
                while (x--) {
                    *of++ = DIV_2(f[0]) + DIV_4(f[0]) + DIV_16(f[1]) + DIV_16(f[-1])
                            + DIV_16(f2[0]) + DIV_16(f3[0]) + adj_tl2;
                    f++;
                    f2++;
                    f3++;
                }

                // right block
                *of++ = DIV_2(f[0]) + DIV_8(f[0]) + DIV_8(f[-1]) + DIV_8(f2[0])
                        + DIV_8(f3[0]) + adj_tl1;
                f++;
            }
        }
        // bottom block
        if (at_bottom) {
            unsigned int* f2 = f - w;
            int x;
            int adj_tl = 0, adj_tl2 = 0;
            if (this->config.round == BLUR_ROUND_UP) {
                adj_tl = 0x03030303;
                adj_tl2 = 0x04040404;
            }
            // bottom left
            *of++ = DIV_2(f[0]) + DIV_4(f[0]) + DIV_8(f[1]) + DIV_8(f2[0]) + adj_tl;
            f++;
            f2++;
            // bottom center
            x = (w - 2) / 4;
            while (x--) {
                of[0] = DIV_2(f[0]) + DIV_8(f[0]) + DIV_8(f[1]) + DIV_8(f[-1])
                        + DIV_8(f2[0]) + adj_tl2;
                of[1] = DIV_2(f[1]) + DIV_8(f[1]) + DIV_8(f[2]) + DIV_8(f[0])
                        + DIV_8(f2[1]) + adj_tl2;
                of[2] = DIV_2(f[2]) + DIV_8(f[2]) + DIV_8(f[3]) + DIV_8(f[1])
                        + DIV_8(f2[2]) + adj_tl2;
                of[3] = DIV_2(f[3]) + DIV_8(f[3]) + DIV_8(f[4]) + DIV_8(f[2])
                        + DIV_8(f2[3]) + adj_tl2;
                f += 4;
                f2 += 4;
                of += 4;
            }
            x = (w - 2) & 3;
            while (x--) {
                *of++ = DIV_2(f[0]) + DIV_8(f[0]) + DIV_8(f[1]) + DIV_8(f[-1])
                        + DIV_8(f2[0]) + adj_tl2;
                f++;
                f2++;
            }
            // bottom right
            *of++ = DIV_2(f[0]) + DIV_4(f[0]) + DIV_8(f[-1]) + DIV_8(f2[0]) + adj_tl;
            f++;
            f2++;
        }
    } else if (this->config.level == BLUR_HEAVY) {
        // more blur
        // top line
        if (at_top) {
            unsigned int* f2 = f + w;
            int x;
            int adj_tl = 0, adj_tl2 = 0;
            if (this->config.round == BLUR_ROUND_UP) {
                adj_tl = 0x02020202;
                adj_tl2 = 0x01010101;
            }
            // top left
            *of++ = DIV_2(f[1]) + DIV_2(f2[0]) + adj_tl2;
            f++;
            f2++;
            // top center
            x = (w - 2) / 4;
            while (x--) {
                of[0] = DIV_4(f[1]) + DIV_4(f[-1]) + DIV_2(f2[0]) + adj_tl;
                of[1] = DIV_4(f[2]) + DIV_4(f[0]) + DIV_2(f2[1]) + adj_tl;
                of[2] = DIV_4(f[3]) + DIV_4(f[1]) + DIV_2(f2[2]) + adj_tl;
                of[3] = DIV_4(f[4]) + DIV_4(f[2]) + DIV_2(f2[3]) + adj_tl;
                f += 4;
                f2 += 4;
                of += 4;
            }
            x = (w - 2) & 3;
            while (x--) {
                *of++ = DIV_4(f[1]) + DIV_4(f[-1]) + DIV_2(f2[0]) + adj_tl;
                f++;
                f2++;
            }
            // top right
            *of++ = DIV_2(f[-1]) + DIV_2(f2[0]) + adj_tl2;
            f++;
            f2++;
        }

        // middle block
        {
            int y = outh - at_top - at_bottom;
            int adj_tl1 = 0, adj_tl2 = 0;
            uint64_t adj2 = 0;
            if (this->config.round == BLUR_ROUND_UP) {
                adj_tl1 = 0x02020202;
                adj_tl2 = 0x03030303;
                adj2 = 0x0303030303030303L;
            }

            while (y--) {
                int x;
                unsigned int* f2 = f + w;
                unsigned int* f3 = f - w;

                // left edge
                *of++ = DIV_2(f[1]) + DIV_4(f2[0]) + DIV_4(f3[0]) + adj_tl1;
                f++;
                f2++;
                f3++;

                // middle of line
                x = (w - 2) / 4;
                if (this->config.round == BLUR_ROUND_UP) {
                    while (x--) {
                        of[0] = DIV_4(f[1]) + DIV_4(f[-1]) + DIV_4(f2[0]) + DIV_4(f3[0])
                                + 0x03030303;
                        of[1] = DIV_4(f[2]) + DIV_4(f[0]) + DIV_4(f2[1]) + DIV_4(f3[1])
                                + 0x03030303;
                        of[2] = DIV_4(f[3]) + DIV_4(f[1]) + DIV_4(f2[2]) + DIV_4(f3[2])
                                + 0x03030303;
                        of[3] = DIV_4(f[4]) + DIV_4(f[2]) + DIV_4(f2[3]) + DIV_4(f3[3])
                                + 0x03030303;
                        f += 4;
                        f2 += 4;
                        f3 += 4;
                        of += 4;
                    }
                } else {
                    while (x--) {
                        of[0] =
                            DIV_4(f[1]) + DIV_4(f[-1]) + DIV_4(f2[0]) + DIV_4(f3[0]);
                        of[1] = DIV_4(f[2]) + DIV_4(f[0]) + DIV_4(f2[1]) + DIV_4(f3[1]);
                        of[2] = DIV_4(f[3]) + DIV_4(f[1]) + DIV_4(f2[2]) + DIV_4(f3[2]);
                        of[3] = DIV_4(f[4]) + DIV_4(f[2]) + DIV_4(f2[3]) + DIV_4(f3[3]);
                        f += 4;
                        f2 += 4;
                        f3 += 4;
                        of += 4;
                    }
                }

                x = (w - 2) & 3;
                while (x--) {
                    *of++ = DIV_4(f[1]) + DIV_4(f[-1]) + DIV_4(f2[0]) + DIV_4(f3[0])
                            + adj_tl2;
                    f++;
                    f2++;
                    f3++;
                }

                // right block
                *of++ = DIV_2(f[-1]) + DIV_4(f2[0]) + DIV_4(f3[0]) + adj_tl1;
                f++;
            }
        }

        // bottom block
        if (at_bottom) {
            unsigned int* f2 = f - w;
            int x;
            int adj_tl = 0, adj_tl2 = 0;
            if (this->config.round == BLUR_ROUND_UP) {
                adj_tl = 0x02020202;
                adj_tl2 = 0x01010101;
            }
            // bottom left
            *of++ = DIV_2(f[1]) + DIV_2(f2[0]) + adj_tl2;
            f++;
            f2++;
            // bottom center
            x = (w - 2) / 4;
            while (x--) {
                of[0] = DIV_4(f[1]) + DIV_4(f[-1]) + DIV_2(f2[0]) + adj_tl;
                of[1] = DIV_4(f[2]) + DIV_4(f[0]) + DIV_2(f2[1]) + adj_tl;
                of[2] = DIV_4(f[3]) + DIV_4(f[1]) + DIV_2(f2[2]) + adj_tl;
                of[3] = DIV_4(f[4]) + DIV_4(f[2]) + DIV_2(f2[3]) + adj_tl;
                f += 4;
                f2 += 4;
                of += 4;
            }
            x = (w - 2) & 3;
            while (x--) {
                *of++ = DIV_4(f[1]) + DIV_4(f[-1]) + DIV_2(f2[0]) + adj_tl;
                f++;
                f2++;
            }
            // bottom right
            *of++ = DIV_2(f[-1]) + DIV_2(f2[0]) + adj_tl2;
            f++;
            f2++;
        }
    } else {
        // top line
        if (at_top) {
            unsigned int* f2 = f + w;
            int x;
            int adj_tl = 0, adj_tl2 = 0;
            if (this->config.round == BLUR_ROUND_UP) {
                adj_tl = 0x02020202;
                adj_tl2 = 0x03030303;
            }
            // top left
            *of++ = DIV_2(f[0]) + DIV_4(f[1]) + DIV_4(f2[0]) + adj_tl;
            f++;
            f2++;
            // top center
            x = (w - 2) / 4;
            while (x--) {
                of[0] =
                    DIV_4(f[0]) + DIV_4(f[1]) + DIV_4(f[-1]) + DIV_4(f2[0]) + adj_tl2;
                of[1] =
                    DIV_4(f[1]) + DIV_4(f[2]) + DIV_4(f[0]) + DIV_4(f2[1]) + adj_tl2;
                of[2] =
                    DIV_4(f[2]) + DIV_4(f[3]) + DIV_4(f[1]) + DIV_4(f2[2]) + adj_tl2;
                of[3] =
                    DIV_4(f[3]) + DIV_4(f[4]) + DIV_4(f[2]) + DIV_4(f2[3]) + adj_tl2;
                f += 4;
                f2 += 4;
                of += 4;
            }
            x = (w - 2) & 3;
            while (x--) {
                *of++ =
                    DIV_4(f[0]) + DIV_4(f[1]) + DIV_4(f[-1]) + DIV_4(f2[0]) + adj_tl2;
                f++;
                f2++;
            }
            // top right
            *of++ = DIV_2(f[0]) + DIV_4(f[-1]) + DIV_4(f2[0]) + adj_tl;
            f++;
            f2++;
        }

        // middle block
        {
            int y = outh - at_top - at_bottom;
            int adj_tl1 = 0, adj_tl2 = 0;
            uint64_t adj2 = 0;
            if (this->config.round == BLUR_ROUND_UP) {
                adj_tl1 = 0x03030303;
                adj_tl2 = 0x04040404;
                adj2 = 0x0404040404040404L;
            }
            while (y--) {
                int x;
                unsigned int* f2 = f + w;
                unsigned int* f3 = f - w;

                // left edge
                *of++ =
                    DIV_4(f[0]) + DIV_4(f[1]) + DIV_4(f2[0]) + DIV_4(f3[0]) + adj_tl1;
                f++;
                f2++;
                f3++;

                // middle of line
                x = (w - 2) / 4;
                if (this->config.round == BLUR_ROUND_UP) {
                    while (x--) {
                        of[0] = DIV_2(f[0]) + DIV_8(f[1]) + DIV_8(f[-1]) + DIV_8(f2[0])
                                + DIV_8(f3[0]) + 0x04040404;
                        of[1] = DIV_2(f[1]) + DIV_8(f[2]) + DIV_8(f[0]) + DIV_8(f2[1])
                                + DIV_8(f3[1]) + 0x04040404;
                        of[2] = DIV_2(f[2]) + DIV_8(f[3]) + DIV_8(f[1]) + DIV_8(f2[2])
                                + DIV_8(f3[2]) + 0x04040404;
                        of[3] = DIV_2(f[3]) + DIV_8(f[4]) + DIV_8(f[2]) + DIV_8(f2[3])
                                + DIV_8(f3[3]) + 0x04040404;
                        f += 4;
                        f2 += 4;
                        f3 += 4;
                        of += 4;
                    }
                } else {
                    while (x--) {
                        of[0] = DIV_2(f[0]) + DIV_8(f[1]) + DIV_8(f[-1]) + DIV_8(f2[0])
                                + DIV_8(f3[0]);
                        of[1] = DIV_2(f[1]) + DIV_8(f[2]) + DIV_8(f[0]) + DIV_8(f2[1])
                                + DIV_8(f3[1]);
                        of[2] = DIV_2(f[2]) + DIV_8(f[3]) + DIV_8(f[1]) + DIV_8(f2[2])
                                + DIV_8(f3[2]);
                        of[3] = DIV_2(f[3]) + DIV_8(f[4]) + DIV_8(f[2]) + DIV_8(f2[3])
                                + DIV_8(f3[3]);
                        f += 4;
                        f2 += 4;
                        f3 += 4;
                        of += 4;
                    }
                }

                x = (w - 2) & 3;
                while (x--) {
                    *of++ = DIV_2(f[0]) + DIV_8(f[1]) + DIV_8(f[-1]) + DIV_8(f2[0])
                            + DIV_8(f3[0]) + adj_tl2;
                    f++;
                    f2++;
                    f3++;
                }

                // right block
                *of++ =
                    DIV_4(f[0]) + DIV_4(f[-1]) + DIV_4(f2[0]) + DIV_4(f3[0]) + adj_tl1;
                f++;
            }
        }

        // bottom block
        if (at_bottom) {
            unsigned int* f2 = f - w;
            int adj_tl = 0, adj_tl2 = 0;
            if (this->config.round == BLUR_ROUND_UP) {
                adj_tl = 0x02020202;
                adj_tl2 = 0x03030303;
            }
            int x;
            // bottom left
            *of++ = DIV_2(f[0]) + DIV_4(f[1]) + DIV_4(f2[0]) + adj_tl;
            f++;
            f2++;
            // bottom center
            x = (w - 2) / 4;
            while (x--) {
                of[0] =
                    DIV_4(f[0]) + DIV_4(f[1]) + DIV_4(f[-1]) + DIV_4(f2[0]) + adj_tl2;
                of[1] =
                    DIV_4(f[1]) + DIV_4(f[2]) + DIV_4(f[0]) + DIV_4(f2[1]) + adj_tl2;
                of[2] =
                    DIV_4(f[2]) + DIV_4(f[3]) + DIV_4(f[1]) + DIV_4(f2[2]) + adj_tl2;
                of[3] =
                    DIV_4(f[3]) + DIV_4(f[4]) + DIV_4(f[2]) + DIV_4(f2[3]) + adj_tl2;
                f += 4;
                f2 += 4;
                of += 4;
            }
            x = (w - 2) & 3;
            while (x--) {
                *of++ =
                    DIV_4(f[0]) + DIV_4(f[1]) + DIV_4(f[-1]) + DIV_4(f2[0]) + adj_tl2;
                f++;
                f2++;
            }
            // bottom right
            *of++ = DIV_2(f[0]) + DIV_4(f[-1]) + DIV_4(f2[0]) + adj_tl;
            f++;
            f2++;
        }
    }
}

int E_Blur::smp_begin(int max_threads, char[2][2][576], int, int*, int*, int, int) {
    return max_threads;
}

int E_Blur::smp_finish(char[2][2][576], int, int*, int*, int, int) { return 1; }

int E_Blur::render(char visdata[2][2][576],
                   int is_beat,
                   int* framebuffer,
                   int* fbout,
                   int w,
                   int h) {
    this->smp_begin(1, visdata, is_beat, framebuffer, fbout, w, h);
    if (is_beat & 0x80000000) {
        return 0;
    }

    this->smp_render(0, 1, visdata, is_beat, framebuffer, fbout, w, h);
    return this->smp_finish(visdata, is_beat, framebuffer, fbout, w, h);
}

void E_Blur::load_legacy(unsigned char* data, int len) {
    int pos = 0;
    uint32_t blur_level = BLUR_MEDIUM;
    if (len - pos >= 4) {
        blur_level = GET_INT();
        this->enabled = true;
        // Note that in the legacy save format Medium is 1 and Light is 2.
        // This is not a mistake.
        switch (blur_level) {
            case 0: this->enabled = false; break;
            default:
            case 1: this->config.level = BLUR_MEDIUM; break;
            case 2: this->config.level = BLUR_LIGHT; break;
            case 3: this->config.level = BLUR_HEAVY; break;
        }
        pos += 4;
    }
    if (len - pos >= 4) {
        this->config.round = GET_INT() == 1 ? BLUR_ROUND_UP : BLUR_ROUND_DOWN;
        pos += 4;
    } else {
        this->config.round = BLUR_ROUND_DOWN;
    }
}

int E_Blur::save_legacy(unsigned char* data) {
    int pos = 0;
    uint32_t blur_level;
    if (this->enabled) {
        switch (this->config.level) {
            case BLUR_LIGHT: blur_level = 2; break;
            default:
            case BLUR_MEDIUM: blur_level = 1; break;
            case BLUR_HEAVY: blur_level = 3; break;
        }
    } else {
        blur_level = 0;
    }
    PUT_INT(blur_level);
    pos += 4;
    PUT_INT(this->config.round == BLUR_ROUND_UP ? 1 : 0);
    pos += 4;
    return pos;
}

Effect_Info* create_Blur_Info() { return new Blur_Info(); }
Effect* create_Blur(AVS_Instance* avs) { return new E_Blur(avs); }
void set_Blur_desc(char* desc) { E_Blur::set_desc(desc); }
