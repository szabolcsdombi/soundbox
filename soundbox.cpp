#include <Windows.h>

#include <Python.h>
#include <structmember.h>

#define QOA_NO_STDIO
#define QOA_IMPLEMENTATION
#include "qoa.h"

#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_PUSHDATA_API
#define STB_VORBIS_NO_FAST_SCALED_FLOAT
#define STB_VORBIS_MAX_CHANNELS 2
#include "stb_vorbis.h"

struct ALCdevice;
struct ALCcontext;

#define AL_PITCH 0x1003
#define AL_POSITION 0x1004
#define AL_DIRECTION 0x1005
#define AL_VELOCITY 0x1006
#define AL_LOOPING 0x1007
#define AL_BUFFER 0x1009
#define AL_GAIN 0x100A
#define AL_ORIENTATION 0x100F
#define AL_SOURCE_STATE 0x1010
#define AL_INITIAL 0x1011
#define AL_PLAYING 0x1012
#define AL_PAUSED 0x1013
#define AL_STOPPED 0x1014
#define AL_FORMAT_MONO16 0x1101
#define AL_FORMAT_STEREO16 0x1103
#define AL_SOURCE_RELATIVE 0x202
#define AL_LOOPING 0x1007

static ALCcontext * (* alcCreateContext)(ALCdevice * device, const int * attrlist);
static char (* alcMakeContextCurrent)(ALCcontext * context);
static ALCdevice * (* alcOpenDevice)(const char * devicename);
static char (* alcCloseDevice)(ALCdevice * device);

static void (* alListenerfv)(int param, const float * values);
static void (* alListenerf)(int param, float value);

static void (* alSourcef)(int source, int param, float value);
static void (* alSourcefv)(int source, int param, const float * values);
static void (* alSourcei)(int source, int param, int value);

static void (* alGenSources)(int n, int * sources);
static void (* alDeleteSources)(int n, const int * sources);
static void (* alGenBuffers)(int n, int * buffers);
static void (* alDeleteBuffers)(int n, const int * buffers);
static void (* alBufferData)(int buffer, int format, const void * data, int size, int samplerate);

static void (* alGetSourcei)(int source, int param, int * value);

static void (* alSourcePlay)(int source);
static void (* alSourceStop)(int source);
static void (* alSourcePause)(int source);

// static void (* alSourcePlayv)(int n, const int * sources);
// static void (* alSourcePausev)(int n, const int * sources);

struct Buffer {
    PyObject_HEAD
    int buffer;
    int size;
    int samplerate;
    int stereo;
};

struct Source {
    PyObject_HEAD
    Source * next;
    Buffer * buffer;
    int source;
};

struct Listener {
    float position[3];
    float velocity[3];
    float orientation[6];
    float gain;
};

static PyTypeObject * Buffer_type;
static PyTypeObject * Source_type;

static PyObject * helper;
static PyObject * buffers;

static ALCdevice * device;
static ALCcontext * context;
static Source * playing;

static Listener listener;

static PyObject * meth_qoa_decode(PyObject * self, PyObject * args, PyObject * kwargs) {
    static char * keywords[] = {"data", NULL};
    PyObject * qoa_data;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O", keywords, &qoa_data)) {
        return NULL;
    }
    if (!PyBytes_Check(qoa_data)) {
        PyErr_SetString(PyExc_TypeError, "data must be bytes");
        return NULL;
    }
    Py_ssize_t size = PyBytes_Size(qoa_data);
    const char * buffer = PyBytes_AsString(qoa_data);
    qoa_desc qoa = {};
    short * ptr = qoa_decode((unsigned char *)buffer, (int)size, &qoa);
    PyObject * data = PyBytes_FromStringAndSize((char *)ptr, qoa.samples * sizeof(short));
    free(ptr);
    PyObject * is_stereo = qoa.channels == 2 ? Py_True : Py_False;
    return Py_BuildValue("(NiO)", data, qoa.samplerate, is_stereo);
}

static PyObject * meth_qoa_encode(PyObject * self, PyObject * args, PyObject * kwargs) {
    static char * keywords[] = {"data", "samplerate", "stereo", NULL};

    PyObject * data;
    int samplerate = 44100;
    int stereo = false;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|ip", keywords, &data, &samplerate, &stereo)) {
        return NULL;
    }

    if (!PyBytes_Check(data)) {
        PyErr_SetString(PyExc_TypeError, "data must be bytes");
        return NULL;
    }

    const char * buffer = PyBytes_AsString(data);
    Py_ssize_t samples = PyBytes_Size(data) / sizeof(short);
    qoa_desc qoa = {(unsigned)(stereo ? 2 : 1), (unsigned)samplerate, (unsigned)samples};
    unsigned int size = 0;
    void * ptr = qoa_encode((short *)buffer, &qoa, &size);
    PyObject * res = PyBytes_FromStringAndSize((char *)ptr, size);
    free(ptr);
    return res;
}

static PyObject * meth_ogg_decode(PyObject * self, PyObject * args, PyObject * kwargs) {
    static char * keywords[] = {"data", NULL};

    PyObject * ogg_data;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O", keywords, &ogg_data)) {
        return NULL;
    }

    if (!PyBytes_Check(ogg_data)) {
        PyErr_SetString(PyExc_TypeError, "data must be bytes");
        return NULL;
    }

    Py_ssize_t buffer_size = PyBytes_Size(ogg_data);
    const char * buffer = PyBytes_AsString(ogg_data);
    int samplerate = 0;
    int channels = 0;
    short * output = NULL;

    int size = stb_vorbis_decode_memory((unsigned char *)buffer, (int)buffer_size, &channels, &samplerate, &output);
    if (size <= 0) {
        PyErr_SetString(PyExc_RuntimeError, "failed to decode ogg data");
        return NULL;
    }

    PyObject * data = PyBytes_FromStringAndSize((char *)output, size * sizeof(short));
    free(output);
    PyObject * is_stereo = channels == 2 ? Py_True : Py_False;
    return Py_BuildValue("(NiO)", data, samplerate, is_stereo);
}

static PyObject * meth_init(PyObject * self, PyObject * args, PyObject * kwargs) {
    const char * keywords[] = {NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "", (char **)keywords)) {
        return NULL;
    }

    PyObject * dll_path = PyObject_CallMethod(helper, "find_openal", NULL);
    HMODULE openal = LoadLibraryA(PyUnicode_AsUTF8(dll_path));
    Py_DECREF(dll_path);
    if (!openal) {
        PyErr_BadInternalCall();
        return NULL;
    }

    *(PROC *)&alcCreateContext = GetProcAddress(openal, "alcCreateContext");
    *(PROC *)&alcMakeContextCurrent = GetProcAddress(openal, "alcMakeContextCurrent");
    *(PROC *)&alcOpenDevice = GetProcAddress(openal, "alcOpenDevice");
    *(PROC *)&alcCloseDevice = GetProcAddress(openal, "alcCloseDevice");

    *(PROC *)&alListenerfv = GetProcAddress(openal, "alListenerfv");
    *(PROC *)&alListenerf = GetProcAddress(openal, "alListenerf");

    *(PROC *)&alSourcef = GetProcAddress(openal, "alSourcef");
    *(PROC *)&alSourcefv = GetProcAddress(openal, "alSourcefv");
    *(PROC *)&alSourcei = GetProcAddress(openal, "alSourcei");

    *(PROC *)&alGenSources = GetProcAddress(openal, "alGenSources");
    *(PROC *)&alDeleteSources = GetProcAddress(openal, "alDeleteSources");
    *(PROC *)&alGenBuffers = GetProcAddress(openal, "alGenBuffers");
    *(PROC *)&alDeleteBuffers = GetProcAddress(openal, "alDeleteBuffers");
    *(PROC *)&alBufferData = GetProcAddress(openal, "alBufferData");

    *(PROC *)&alGetSourcei = GetProcAddress(openal, "alGetSourcei");
    *(PROC *)&alSourcePlay = GetProcAddress(openal, "alSourcePlay");
    *(PROC *)&alSourceStop = GetProcAddress(openal, "alSourceStop");
    *(PROC *)&alSourcePause = GetProcAddress(openal, "alSourcePause");

    device = alcOpenDevice(NULL);
    if (!device) {
        PyErr_BadInternalCall();
        return NULL;
    }

    context = alcCreateContext(device, NULL);
    if (!context) {
        PyErr_BadInternalCall();
        return NULL;
    }

    alcMakeContextCurrent(context);
    Py_RETURN_NONE;
}

static PyObject * meth_update(PyObject * self, PyObject * args, PyObject * kwargs) {
    const char * keywords[] = {"position", "velocity", "direction", "up", "gain", NULL};

    int args_ok = PyArg_ParseTupleAndKeywords(
        args, kwargs, "|(fff)(fff)(fff)(fff)f", (char **)keywords,
        &listener.position[0], &listener.position[1], &listener.position[2],
        &listener.velocity[0], &listener.velocity[1], &listener.velocity[2],
        &listener.orientation[0], &listener.orientation[1], &listener.orientation[2],
        &listener.orientation[3], &listener.orientation[4], &listener.orientation[5],
        &listener.gain
    );

    if (!args_ok) {
        return NULL;
    }

    alListenerfv(AL_POSITION, listener.position);
    alListenerfv(AL_VELOCITY, listener.velocity);
    alListenerfv(AL_ORIENTATION, listener.orientation);
    alListenerf(AL_GAIN, listener.gain);

    Source ** ptr = &playing;
    while (Source * source = *ptr) {
        int state = 0;
        alGetSourcei(source->source, AL_SOURCE_STATE, &state);
        if (state == AL_STOPPED) {
            alSourcei(source->source, AL_BUFFER, 0);
            Py_DECREF(source->buffer);
            source->buffer = NULL;
            Source * next = source->next;
            source->next = NULL;
            Py_DECREF(source);
            *ptr = next;
        } else {
            ptr = &source->next;
        }
    }
    Py_RETURN_NONE;
}

static PyObject * meth_pause(PyObject * self, PyObject * args) {
    Source * source = playing;
    while (source) {
        alSourcePause(source->source);
        source = source->next;
    }
    Py_RETURN_NONE;
}

static PyObject * meth_resume(PyObject * self, PyObject * args) {
    Source * source = playing;
    while (source) {
        int state = 0;
        alGetSourcei(source->source, AL_SOURCE_STATE, &state);
        if (state == AL_PAUSED) {
            alSourcePlay(source->source);
        }
        source = source->next;
    }
    Py_RETURN_NONE;
}

static PyObject * meth_load(PyObject * self, PyObject * args, PyObject * kwargs) {
    const char * keywords[] = {"name", "data", NULL};

    PyObject * name;
    PyObject * data;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OO", (char **)keywords, &name, &data)) {
        return NULL;
    }

    PyObject * tup = PyObject_CallMethod(helper, "load", "(OO)", data, self);
    if (!tup) {
        return NULL;
    }

    PyObject * audio_data = PyTuple_GetItem(tup, 0);
    int samplerate = (int)PyLong_AsLong(PyTuple_GetItem(tup, 1));
    int stereo = PyObject_IsTrue(PyTuple_GetItem(tup, 2));

    const char * ptr = PyBytes_AsString(audio_data);
    int size = (int)PyBytes_Size(audio_data);

    int buffer = 0;
    alGenBuffers(1, &buffer);
    int format = stereo ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16;
    alBufferData(buffer, format, ptr, size, samplerate);

    Buffer * res = PyObject_New(Buffer, Buffer_type);
    res->buffer = buffer;
    res->size = size;
    res->samplerate = samplerate;
    res->stereo = stereo;

    PyDict_SetItem(buffers, name, (PyObject *)res);
    Py_DECREF(res);

    Py_DECREF(tup);
    Py_RETURN_NONE;
}

static PyObject * meth_unload(PyObject * self, PyObject * args, PyObject * kwargs) {
    const char * keywords[] = {"name", NULL};

    PyObject * name;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O", (char **)keywords, &name)) {
        return NULL;
    }

    PyDict_DelItem(buffers, name);
    Py_RETURN_NONE;
}

static Source * meth_play(PyObject * self, PyObject * args, PyObject * kwargs) {
    const char * keywords[] = {
        "name", "position", "velocity", "direction", "pitch", "gain", "relative", "loop", NULL,
    };

    PyObject * name;
    float position[3] = {0.0f, 0.0f, 0.0f};
    float velocity[3] = {0.0f, 0.0f, 0.0f};
    float direction[3] = {0.0f, 0.0f, 0.0f};
    float pitch = 1.0f;
    float gain = 1.0f;
    int relative = false;
    int loop = false;

    int args_ok = PyArg_ParseTupleAndKeywords(
        args, kwargs, "O|(fff)(fff)(fff)ffpp", (char **)keywords,
        &name,
        &position[0], &position[1], &position[2],
        &velocity[0], &velocity[1], &velocity[2],
        &direction[0], &direction[1], &direction[2],
        &pitch,
        &gain,
        &relative,
        &loop
    );

    if (!args_ok) {
        return NULL;
    }

    Buffer * buffer = (Buffer *)PyDict_GetItem(buffers, name);
    if (!buffer) {
        PyErr_SetString(PyExc_KeyError, "buffer not found");
        return NULL;
    }

    int source = 0;
    alGenSources(1, &source);
    alSourcefv(source, AL_POSITION, position);
    alSourcefv(source, AL_VELOCITY, velocity);
    alSourcefv(source, AL_DIRECTION, direction);
    alSourcef(source, AL_PITCH, pitch);
    alSourcef(source, AL_GAIN, gain);
    alSourcei(source, AL_SOURCE_RELATIVE, relative);
    alSourcei(source, AL_LOOPING, loop);
    alSourcei(source, AL_BUFFER, buffer->buffer);
    alSourcePlay(source);

    Source * res = PyObject_New(Source, Source_type);
    res->next = playing;
    res->buffer = buffer;
    res->source = source;
    playing = res;
    Py_INCREF(res);
    return res;
}

static PyObject * Source_meth_stop(Source * self, PyObject * args) {
    alSourceStop(self->source);
    Py_RETURN_NONE;
}

static PyObject * Source_meth_pause(Source * self, PyObject * args) {
    alSourcePause(self->source);
    Py_RETURN_NONE;
}

static PyObject * Source_meth_resume(Source * self, PyObject * args) {
    int state = 0;
    alGetSourcei(self->source, AL_SOURCE_STATE, &state);
    if (state == AL_PAUSED) {
        alSourcePlay(self->source);
    }
    Py_RETURN_NONE;
}

static PyObject * Source_get_ended(Source * self, void * closure) {
    int state = 0;
    alGetSourcei(self->source, AL_SOURCE_STATE, &state);
    return state == AL_STOPPED ? Py_True : Py_False;
}

static void Buffer_dealloc(Buffer * self) {
    alDeleteBuffers(1, &self->buffer);
    Py_TYPE(self)->tp_free(self);
}

static void Source_dealloc(Source * self) {
    alDeleteSources(1, &self->source);
    Py_TYPE(self)->tp_free(self);
}

static PyType_Slot Buffer_slots[] = {
    {Py_tp_dealloc, (void *)Buffer_dealloc},
    {},
};

static PyMethodDef Source_methods[] = {
    {"stop", (PyCFunction)Source_meth_stop, METH_NOARGS},
    {"pause", (PyCFunction)Source_meth_pause, METH_NOARGS},
    {"resume", (PyCFunction)Source_meth_resume, METH_NOARGS},
    {},
};

static PyMemberDef Source_members[] = {
    // {"foo", T_OBJECT, offsetof(Source, foo), READONLY},
    {},
};

static PyGetSetDef Source_getset[] = {
    {"ended", (getter)Source_get_ended, NULL},
    // {"playing", (getter)Source_get_playing, (setter)Source_set_playing},
    {},
};

static PyType_Slot Source_slots[] = {
    {Py_tp_methods, Source_methods},
    {Py_tp_members, Source_members},
    {Py_tp_getset, Source_getset},
    {Py_tp_dealloc, (void *)Source_dealloc},
    {},
};

static PyType_Spec Buffer_spec = {"Buffer", sizeof(Buffer), 0, Py_TPFLAGS_DEFAULT, Buffer_slots};
static PyType_Spec Source_spec = {"Source", sizeof(Source), 0, Py_TPFLAGS_DEFAULT, Source_slots};

static PyMethodDef module_methods[] = {
    {"init", (PyCFunction)meth_init, METH_VARARGS | METH_KEYWORDS},
    {"update", (PyCFunction)meth_update, METH_VARARGS | METH_KEYWORDS},
    {"pause", (PyCFunction)meth_pause, METH_NOARGS},
    {"resume", (PyCFunction)meth_resume, METH_NOARGS},
    {"load", (PyCFunction)meth_load, METH_VARARGS | METH_KEYWORDS},
    {"unload", (PyCFunction)meth_unload, METH_VARARGS | METH_KEYWORDS},
    {"play", (PyCFunction)meth_play, METH_VARARGS | METH_KEYWORDS},
    {"qoa_decode", (PyCFunction)meth_qoa_decode, METH_VARARGS | METH_KEYWORDS},
    {"qoa_encode", (PyCFunction)meth_qoa_encode, METH_VARARGS | METH_KEYWORDS},
    {"ogg_decode", (PyCFunction)meth_ogg_decode, METH_VARARGS | METH_KEYWORDS},
    {},
};

static void module_free(void * module) {
    if (device) {
        alcCloseDevice(device);
        device = NULL;
    }
}

static PyModuleDef module_def = {
    PyModuleDef_HEAD_INIT, "soundbox", NULL, -1, module_methods, NULL, NULL, NULL, (freefunc)module_free,
};

extern "C" PyObject * PyInit_soundbox() {
    PyObject * module = PyModule_Create(&module_def);
    helper = PyImport_ImportModule("soundbox_core");
    if (!helper) {
        return NULL;
    }

    buffers = PyDict_New();
    Buffer_type = (PyTypeObject *)PyType_FromSpec(&Buffer_spec);
    Source_type = (PyTypeObject *)PyType_FromSpec(&Source_spec);
    PyModule_AddObject(module, "Buffer", (PyObject *)Buffer_type);
    PyModule_AddObject(module, "Source", (PyObject *)Source_type);
    return module;
}
