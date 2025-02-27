#pragma once

#include "c__base.h"

#define C_THISCLASS C_RotBlitClass
#define MOD_NAME "Trans / Roto Blitter"


class C_THISCLASS : public C_RBASE {
	protected:
	public:
		C_THISCLASS();
		virtual ~C_THISCLASS();
		virtual int render(char visdata[2][2][576], int isBeat, int *framebuffer, int *fbout, int w, int h);
		virtual char *get_desc() { return MOD_NAME; }
		virtual void load_config(unsigned char *data, int len);
		virtual int  save_config(unsigned char *data);

		int zoom_scale, rot_dir, blend, beatch, beatch_speed, zoom_scale2, beatch_scale,scale_fpos;
		int rot_rev;
    int subpixel;
    double rot_rev_pos;

    int l_w, l_h;
    int *w_mul;
};
