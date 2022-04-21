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
#include "e_svp.h"

#include "files.h"

#define PUT_INT(y)                   \
    data[pos] = (y)&255;             \
    data[pos + 1] = (y >> 8) & 255;  \
    data[pos + 2] = (y >> 16) & 255; \
    data[pos + 3] = (y >> 24) & 255
#define GET_INT() \
    (data[pos] | (data[pos + 1] << 8) | (data[pos + 2] << 16) | (data[pos + 3] << 24))

constexpr Parameter SVP_Info::parameters[];
std::vector<std::string> E_SVP::filenames;
const char** E_SVP::c_filenames;

void set_library(Effect* component, const Parameter*, std::vector<int64_t>) {
    E_SVP* svp = (E_SVP*)component;
    svp->set_library();
}

const char* const* SVP_Info::library_files(int64_t* length_out) {
    *length_out = E_SVP::filenames.size();
    auto new_list = (const char**)malloc(E_SVP::filenames.size() * sizeof(char*));
    for (size_t i = 0; i < E_SVP::filenames.size(); i++) {
        new_list[i] = E_SVP::filenames[i].c_str();
    }
    auto old_list = E_SVP::c_filenames;
    E_SVP::c_filenames = new_list;
    free(old_list);
    return E_SVP::c_filenames;
}

E_SVP::E_SVP() : library(0), library_lock(lock_init()), vi(NULL) {
    this->find_library_files();
}

E_SVP::~E_SVP() {
    if (this->vi) this->vi->SaveSettings("avs.ini");
    if (this->library) library_unload(this->library);
    lock_destroy(this->library_lock);
}

static void add_file_callback(const char* file, void* data) {
    E_SVP* avi = (E_SVP*)data;
    size_t filename_length = strnlen(file, MAX_PATH);
    char* filename = (char*)calloc(filename_length + 1, sizeof(char));
    strncpy(filename, file, filename_length + 1);
    avi->filenames.push_back(filename);
}

void E_SVP::find_library_files() {
    this->clear_library_files();
    const int num_extensions = 2;
    char* extensions[num_extensions] = {".SVP", ".UVS"};
    find_files_by_extensions(g_path,
                             extensions,
                             num_extensions,
                             add_file_callback,
                             this,
                             sizeof(*this),
                             /*recursive*/ false,
                             /*background*/ false);
}

void E_SVP::clear_library_files() { this->filenames.clear(); }

void E_SVP::set_library() {
    lock_lock(this->library_lock);
    if (this->library) {
        if (this->vi) this->vi->SaveSettings("avs.ini");
        this->vi = NULL;
        library_unload(this->library);
        this->library = NULL;
    }

    if (this->config.library < 0) {
        lock_unlock(this->library_lock);
        return;
    }

    char buf1[MAX_PATH];
    strcpy(buf1, g_path);
    strcat(buf1, "\\");
    strcat(buf1, this->filenames[this->config.library].c_str());

    this->vi = NULL;
    this->library = library_load(buf1);
    if (this->library) {
        VisInfo* (*qm)(void);
        *(void**)&qm = library_get(this->library, "QueryModule");
        if (!qm)
            *(void**)&qm =
                library_get(this->library, "?QueryModule@@YAPAUUltraVisInfo@@XZ");

        if (!qm || !(this->vi = qm())) {
            this->vi = NULL;
            library_unload(this->library);
            this->library = NULL;
        }
    }
    if (this->vi) {
        this->vi->OpenSettings("avs.ini");
        this->vi->Initialize();
    }
    lock_unlock(this->library_lock);
}

int E_SVP::render(char visdata[2][2][576],
                  int isBeat,
                  int* framebuffer,
                  int*,
                  int w,
                  int h) {
    if (isBeat & 0x80000000) return 0;
    lock_lock(this->library_lock);
    if (this->vi) {
        if (vi->lRequired & VI_WAVEFORM) {
            int ch, p;
            for (ch = 0; ch < 2; ch++) {
                unsigned char* v = (unsigned char*)visdata[1][ch];
                for (p = 0; p < 512; p++) this->vd.Waveform[ch][p] = v[p];
            }
        }
        if (vi->lRequired & VI_SPECTRUM) {
            int ch, p;
            for (ch = 0; ch < 2; ch++) {
                unsigned char* v = (unsigned char*)visdata[0][ch];
                for (p = 0; p < 256; p++)
                    this->vd.Spectrum[ch][p] = v[p * 2] / 2 + v[p * 2 + 1] / 2;
            }
        }

        this->vd.MillSec = (uint32_t)(timer_ms() & 0xffffffff);
        this->vi->Render((unsigned long*)framebuffer, w, h, w, &this->vd);
    }
    lock_unlock(this->library_lock);
    return 0;
}

void E_SVP::load_legacy(unsigned char* data, int len) {
    if (len >= LEGACY_SAVE_PATH_LEN) {
        char* str_data = (char*)data;
        std::string search_filename;
        this->string_nt_load_legacy(str_data, search_filename, LEGACY_SAVE_PATH_LEN);
        bool found = false;
        for (size_t i = 0; i < this->filenames.size(); i++) {
            if (this->filenames[i] == search_filename) {
                found = true;
                this->config.library = i;
            }
        }
        if (!found) {
            this->config.library = -1;
        }
        this->set_library();
    }
}

int E_SVP::save_legacy(unsigned char* data) {
    char* str_data = (char*)data;
    if (this->config.library < 0 || this->config.library > this->filenames.size()) {
        memset(data, '\0', LEGACY_SAVE_PATH_LEN);
    } else {
        auto& filename = this->filenames[this->config.library];
        this->string_nt_save_legacy(filename, str_data, LEGACY_SAVE_PATH_LEN);
        memset(
            &data[filename.length()], '\0', LEGACY_SAVE_PATH_LEN - filename.length());
    }
    return LEGACY_SAVE_PATH_LEN;
}

Effect_Info* create_SVP_Info() { return new SVP_Info(); }
Effect* create_SVP() { return new E_SVP(); }
