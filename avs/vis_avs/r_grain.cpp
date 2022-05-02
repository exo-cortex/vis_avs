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
#include "c_grain.h"

#include "r_defs.h"

#include <stdlib.h>

#define GET_INT() \
    (data[pos] | (data[pos + 1] << 8) | (data[pos + 2] << 16) | (data[pos + 3] << 24))

#define PUT_INT(y)                   \
    data[pos] = (y)&255;             \
    data[pos + 1] = (y >> 8) & 255;  \
    data[pos + 2] = (y >> 16) & 255; \
    data[pos + 3] = (y >> 24) & 255

constexpr Parameter Grain_Info::parameters[];

E_Grain::~E_Grain()  // set up default configuration
{
    if (depthBuffer) free(depthBuffer);
}

// configuration read/write

E_Grain::E_Grain()  // set up default configuration
{
    oldx = 0;
    oldy = 0;
    depthBuffer = NULL;
    unsigned int x;
    for (x = 0; x < sizeof(randtab); x++) randtab[x] = rand() & 255;
    randtab_pos = rand() % sizeof(randtab);
}

unsigned char inline E_Grain::fastrandbyte(void) {
    unsigned char r = randtab[randtab_pos];
    randtab_pos++;
    if (!(randtab_pos & 15)) {
        randtab_pos += rand() % 73;
    }
    if (randtab_pos >= 491) randtab_pos -= 491;
    return r;
}

void E_Grain::reinit(int w, int h) {
    int x, y;
    unsigned char* p;
    if (depthBuffer) free(depthBuffer);
    depthBuffer = (unsigned char*)malloc(w * h * 2);
    p = depthBuffer;
    if (p)
        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++) {
                *p++ = (rand() % 255);
                *p++ = (rand() % 100);
            }
}

int E_Grain::render(char[2][2][576],
                        int isBeat,
                        int* framebuffer,
                        int*,
                        int w,
                        int h) {
    if (isBeat & 0x80000000) return 0;
    if (!this->enabled) return 0;

    int amount_scaled = (this->config.amount * 255) / 100;
    int* p;
    unsigned char* q;
    int l = w * h;

    if (w != oldx || h != oldy) {
        reinit(w, h);
        oldx = w;
        oldy = h;
    }
    randtab_pos += rand() % 300;
    if (randtab_pos >= 491) randtab_pos -= 491; 

    p = framebuffer;
    q = depthBuffer;
    if (this->config.static_) {
        if (this->config.blend_mode == 1) {
            while (l--) {
                if (*p) {
                    int c = 0;
                    if (q[1] < amount_scaled) {
                        int s = q[0];
                        int r = (((p[0] & 0xff0000) * s) >> 8);
                        if (r > 0xff0000) r = 0xff0000;
                        c |= r & 0xff0000;
                        r = (((p[0] & 0xff00) * s) >> 8);
                        if (r > 0xff00) r = 0xff00;
                        c |= r & 0xff00;
                        r = (((p[0] & 0xff) * s) >> 8);
                        if (r > 0xff) r = 0xff;
                        c |= r;
                    }
                    *p = BLEND(*p, c);
                }
                p++;
                q += 2;
            }
        } else if (this->config.blend_mode == 2) {
            while (l--) {
                if (*p) {
                    int c = 0;
                    if (q[1] < amount_scaled) {
                        int s = q[0];
                        int r = (((p[0] & 0xff0000) * s) >> 8);
                        if (r > 0xff0000) r = 0xff0000;
                        c |= r & 0xff0000;
                        r = (((p[0] & 0xff00) * s) >> 8);
                        if (r > 0xff00) r = 0xff00;
                        c |= r & 0xff00;
                        r = (((p[0] & 0xff) * s) >> 8);
                        if (r > 0xff) r = 0xff;
                        c |= r;
                    }
                    *p = BLEND_AVG(*p, c);
                }
                p++;
                q += 2;
            }
        } else {
            //  = `replace-mode`
            while (l--) {
                if (*p) {
                    int c = 0;
                    if (q[1] < amount_scaled) {
                        int s = q[0];
                        int r = (((p[0] & 0xff0000) * s) >> 8);
                        if (r > 0xff0000) r = 0xff0000;
                        c |= r & 0xff0000;
                        r = (((p[0] & 0xff00) * s) >> 8);
                        if (r > 0xff00) r = 0xff00;
                        c |= r & 0xff00;
                        r = (((p[0] & 0xff) * s) >> 8);
                        if (r > 0xff) r = 0xff;
                        c |= r;
                    }
                    *p = c;
                }
                p++;
                q += 2;
            }
        }
    } else {
        if (this->config.blend_mode == 1) {
            while (l--) {
                if (*p) {
                    int c = 0;
                    if (fastrandbyte() < amount_scaled) {
                        int s = fastrandbyte();
                        int r = (((p[0] & 0xff0000) * s) >> 8);
                        if (r > 0xff0000) r = 0xff0000;
                        c |= r & 0xff0000;
                        r = (((p[0] & 0xff00) * s) >> 8);
                        if (r > 0xff00) r = 0xff00;
                        c |= r & 0xff00;
                        r = (((p[0] & 0xff) * s) >> 8);
                        if (r > 0xff) r = 0xff;
                        c |= r;
                    }
                    *p = BLEND(*p, c);
                }
                p++;
                q += 2;
            }
        } else if (this->config.blend_mode == 2) {
            while (l--) {
                if (*p) {
                    int c = 0;
                    if (fastrandbyte() < amount_scaled) {
                        int s = fastrandbyte();
                        int r = (((p[0] & 0xff0000) * s) >> 8);
                        if (r > 0xff0000) r = 0xff0000;
                        c |= r & 0xff0000;
                        r = (((p[0] & 0xff00) * s) >> 8);
                        if (r > 0xff00) r = 0xff00;
                        c |= r & 0xff00;
                        r = (((p[0] & 0xff) * s) >> 8);
                        if (r > 0xff) r = 0xff;
                        c |= r;
                    }
                    *p = BLEND_AVG(*p, c);
                }
                p++;
                q += 2;
            }
        } else {
            // blendmode `replace`
            while (l--) {
                if (*p) {
                    int c = 0;
                    if (fastrandbyte() < amount_scaled) {
                        int s = fastrandbyte();
                        int r = (((p[0] & 0xff0000) * s) >> 8);
                        if (r > 0xff0000) r = 0xff0000;
                        c |= r & 0xff0000;
                        r = (((p[0] & 0xff00) * s) >> 8);
                        if (r > 0xff00) r = 0xff00;
                        c |= r & 0xff00;
                        r = (((p[0] & 0xff) * s) >> 8);
                        if (r > 0xff) r = 0xff;
                        c |= r;
                    }
                    *p = c;
                }
                p++;
                q += 2;
            }
        }
    }
    return 0;
}

void E_Grain::load_legacy(unsigned char* data, int len) {
    int pos = 0;
    if (len - pos >= 4) {
        this->enabled = GET_INT();
        pos += 4;
    }
    this->config.blend_mode = 0; // "replace"
    if (len - pos >= 4) {
        if (GET_INT()) {
            this->config.blend_mode = 1;
        }
        pos += 4;
    }
    if (len - pos >= 4) {
        if (GET_INT() && this->config.blend_mode == 0) {
            this->config.blend_mode = 2;
        }
        pos += 4;
    }
    if (len - pos >= 4) {
        this->config.amount = GET_INT();
        pos += 4;
    }
    if (len - pos >= 4) {
        this->config.static_ = GET_INT();
        pos += 4;
    }
}

int E_Grain::save_legacy(unsigned char* data) {
    int pos = 0;

    PUT_INT(this->enabled);
    pos += 4;
    PUT_INT(this->config.blend_mode == 1);
    pos += 4;
    PUT_INT(this->config.blend_mode == 2);
    pos += 4;
    PUT_INT(this->config.amount);
    pos += 4;
    PUT_INT(this->config.static_);
    pos += 4;
    return pos;
}

Effect_Info* create_Grain_Info() { return new Grain_Info(); }
Effect* create_Grain() { return new E_Grain(); }